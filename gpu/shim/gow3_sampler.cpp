// SPDX-License-Identifier: GPL-2.0-or-later
#include "gow3_sampler.h"

#ifdef _WIN32
#include <windows.h>
#include <timeapi.h>
#undef near

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Gow3Sampler {
namespace {
constexpr int MaxFrames = 48;
constexpr DWORD ReportMs = 10000;

struct Key {
    std::uint64_t base, begin; // image base, function start RVA (or the address when unknown)
    bool operator==(const Key& o) const { return base == o.base && begin == o.begin; }
};
struct KeyHash {
    std::size_t operator()(const Key& k) const { return std::hash<std::uint64_t>{}(k.base * 31 + k.begin); }
};

struct Target {
    std::string name;
    HANDLE handle{};
    DWORD id{};
    std::uint64_t samples = 0;
    std::unordered_map<Key, std::uint64_t, KeyHash> self, total;
};

std::mutex targets_mutex;
std::vector<Target*> targets;
std::once_flag start_once;

bool Enabled() {
    static const bool enabled = [] {
        const char* e = std::getenv("GOW3_SAMPLE");
        return e && *e == '1';
    }();
    return enabled;
}

std::string ModuleName(std::uint64_t base) {
    static std::unordered_map<std::uint64_t, std::string> names;
    if (const auto it = names.find(base); it != names.end()) {
        return it->second;
    }
    char path[MAX_PATH] = "?";
    if (base) {
        GetModuleFileNameA(reinterpret_cast<HMODULE>(base), path, sizeof(path));
    }
    std::string name = path;
    if (const auto slash = name.find_last_of("\\/"); slash != std::string::npos) {
        name = name.substr(slash + 1);
    }
    return names[base] = name;
}

/// Walks the suspended thread's stack; frames[0] is where it is running.
int Walk(CONTEXT context, Key* frames) {
    int count = 0;
    for (; count < MaxFrames && context.Rip; ++count) {
        DWORD64 image_base = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
        if (!function) {
            // A leaf function without unwind data (or code outside any image: guest code,
            // JIT): the return address is at the stack pointer.
            frames[count] = {0, context.Rip};
            if (!image_base && count > 0) {
                ++count;
                break; // not in an image: stop rather than guess
            }
            if (context.Rsp == 0 || IsBadReadPtr(reinterpret_cast<void*>(context.Rsp), 8)) {
                ++count;
                break;
            }
            context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
            context.Rsp += 8;
            continue;
        }
        frames[count] = {image_base, function->BeginAddress};
        void* handler_data = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, function, &context,
                         &handler_data, &establisher, nullptr);
    }
    return count;
}

void Print(Target& t) {
    const auto top = [](const std::unordered_map<Key, std::uint64_t, KeyHash>& counts) {
        std::vector<std::pair<Key, std::uint64_t>> v(counts.begin(), counts.end());
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (v.size() > 30) {
            v.resize(30);
        }
        return v;
    };
    std::printf("Sampler thread %s: %llu samples in %u s\n", t.name.c_str(),
                static_cast<unsigned long long>(t.samples), unsigned(ReportMs / 1000));
    if (t.samples == 0) {
        return;
    }
    const auto line = [&](const char* kind, const std::pair<Key, std::uint64_t>& entry) {
        const Key& k = entry.first;
        std::printf("  %s %5.1f%% %s+0x%llx\n", kind, 100.0 * entry.second / t.samples,
                    k.base ? ModuleName(k.base).c_str() : "(no image)",
                    static_cast<unsigned long long>(k.begin));
    };
    for (const auto& e : top(t.self)) {
        line("self ", e);
    }
    for (const auto& e : top(t.total)) {
        line("total", e);
    }
    t.samples = 0;
    t.self.clear();
    t.total.clear();
}

void Run() {
    timeBeginPeriod(1);
    std::printf("Sampler: on (GOW3_SAMPLE=1); issn-probe.exe at %p\n", GetModuleHandleA(nullptr));
    std::fflush(stdout);
    DWORD last_report = GetTickCount();
    Key frames[MaxFrames];
    std::unordered_set<std::uint64_t> seen;
    for (;;) {
        Sleep(1);
        std::vector<Target*> list;
        {
            std::scoped_lock lock{targets_mutex};
            list = targets;
        }
        for (Target* t : list) {
            if (SuspendThread(t->handle) == static_cast<DWORD>(-1)) {
                continue;
            }
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            const bool ok = GetThreadContext(t->handle, &context);
            int count = 0;
            if (ok) {
                // The thread is stopped: its stack can be read, but nothing that may hold a lock
                // it holds (no heap use until it resumes).
                count = Walk(context, frames);
            }
            ResumeThread(t->handle);
            if (!count) {
                continue;
            }
            ++t->samples;
            ++t->self[frames[0]];
            seen.clear();
            for (int i = 0; i < count; ++i) {
                if (seen.insert(frames[i].base * 31 + frames[i].begin).second) {
                    ++t->total[frames[i]];
                }
            }
        }
        if (GetTickCount() - last_report >= ReportMs) {
            last_report = GetTickCount();
            for (Target* t : list) {
                Print(*t);
            }
            std::fflush(stdout);
        }
    }
}
} // namespace

void RegisterThisThread(const char* name) {
    if (!Enabled()) {
        return;
    }
    HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                               FALSE, GetCurrentThreadId());
    if (!handle) {
        return;
    }
    {
        std::scoped_lock lock{targets_mutex};
        auto* target = new Target;
        target->name = name;
        target->handle = handle;
        target->id = GetCurrentThreadId();
        targets.push_back(target);
    }
    std::call_once(start_once, [] { std::thread(Run).detach(); });
}
} // namespace Gow3Sampler

#else

namespace Gow3Sampler {
void RegisterThisThread(const char*) {}
} // namespace Gow3Sampler

#endif
