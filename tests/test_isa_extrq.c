/* isa_rewrite (src/isa_rewrite.c): the generated EXTRQ stub (SSE4A, which Intel CPUs lack) on real
 * patched code in an RWX region, against a C model of the instruction. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include "isa_rewrite.h"

enum { PAGE = 4096, A = 0x000, B = 0x040 };
/* A: extrq xmm1, xmm0 (4 bytes); nop; ret        -> JMP_RELOCATE with the nop */
static const uint8_t A_CODE[] = {0x66, 0x0f, 0x79, 0xc8, 0x90, 0xc3};
/* B: extrq xmm9, xmm10 (5 bytes, REX); ret       -> JMP */
static const uint8_t B_CODE[] = {0x66, 0x45, 0x0f, 0x79, 0xca, 0xc3};

static unsigned char *image, *stubs;

static uint64_t model(uint64_t value, uint64_t control) {
    unsigned length = control & 63, index = (control >> 8) & 63;
    uint64_t field = value >> index;
    return length ? field & ((1ull << length) - 1) : field;
}

/* Runs A with xmm1 = {lo, hi}, xmm0 = control; returns xmm1 and checks rax/rcx/rdx/flags. */
static void run_a(uint64_t lo, uint64_t hi, uint64_t control, uint64_t out[2]) {
    uint64_t in[2] = {lo, hi}, ctl[2] = {control, 0};
    uint64_t rax = 0x1111, rcx = 0x2222, rdx = 0x3333, flags_before, flags_after;
    void (*fn)(void) = (void (*)(void))(image + A);
    __asm__ volatile(
        "movdqu (%[in]), %%xmm1\n\t"
        "movdqu (%[ctl]), %%xmm0\n\t"
        "stc\n\t"
        "pushfq\n\tpopq %[fb]\n\t"
        "call *%[fn]\n\t"
        "pushfq\n\tpopq %[fa]\n\t"
        "movdqu %%xmm1, (%[out])\n\t"
        : [fb] "=&r"(flags_before), [fa] "=&r"(flags_after), "+a"(rax), "+c"(rcx), "+d"(rdx)
        : [in] "r"(in), [ctl] "r"(ctl), [out] "r"(out), [fn] "r"(fn)
        : "xmm0", "xmm1", "memory", "cc");
    assert(rax == 0x1111 && rcx == 0x2222 && rdx == 0x3333);
    assert(flags_before == flags_after);
}

static void run_b(uint64_t lo, uint64_t hi, uint64_t control, uint64_t out[2]) {
    uint64_t in[2] = {lo, hi}, ctl[2] = {control, 0};
    void (*fn)(void) = (void (*)(void))(image + B);
    __asm__ volatile("movdqu (%[in]), %%xmm9\n\t"
                     "movdqu (%[ctl]), %%xmm10\n\t"
                     "call *%[fn]\n\t"
                     "movdqu %%xmm9, (%[out])\n\t"
                     :
                     : [in] "r"(in), [ctl] "r"(ctl), [out] "r"(out), [fn] "r"(fn)
                     : "rax", "rcx", "rdx", "xmm9", "xmm10", "memory", "cc");
}

static IsaSite site(uint64_t offset, uint8_t length, uint8_t mode, uint8_t dst, uint8_t src) {
    IsaSite s;
    memset(&s, 0, sizeof s);
    s.offset = offset;
    s.length = length;
    s.kind = ISA_EXTRQ;
    s.width = 128;
    s.mode = mode;
    s.dst = dst;
    s.src1 = src;
    s.src2 = s.base = s.index = ISA_NO_REG;
    memcpy(s.original, image + offset, length);
    return s;
}

int main(void) {
#ifdef _WIN32
    unsigned char *region = VirtualAlloc(NULL, 3 * PAGE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    assert(region != NULL);
#else
    unsigned char *region = mmap(NULL, 3 * PAGE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(region != MAP_FAILED);
#endif
    image = region;
    stubs = region + PAGE;
    memset(image, 0xcc, PAGE);
    memcpy(image + A, A_CODE, sizeof A_CODE);
    memcpy(image + B, B_CODE, sizeof B_CODE);

    IsaSite sites[2] = {site(A, 4, ISA_MODE_JMP_RELOCATE, 1, 0), site(B, 5, ISA_MODE_JMP, 9, 10)};
    sites[0].relocated_length = 1;
    sites[0].relocated[0] = 0x90;
    IsaTable t = {0};
    t.sites = sites;
    t.hits = calloc(2, sizeof *t.hits);
    t.count = 2;
    t.image_size = PAGE;
    /* Not applied without the SSE4A bit (an AMD host). */
    IsaReport none = isa_rewrite_apply(&t, image, stubs, CPU_SET_BMI1);
    assert(none.rewritten == 0 && image[A] == 0x66);
    IsaReport r = isa_rewrite_apply(&t, image, stubs, CPU_SET_SSE4A);
    assert(r.rewritten == 2 && r.relocated == 1 && r.jmp == 1 && r.skipped == 0);
    assert(image[A] == 0xe9 && image[B] == 0xe9);

    const uint64_t values[] = {0x0123456789abcdefull, 0xffffffffffffffffull, 0x8000000000000001ull, 0};
    unsigned checked = 0;
    for (size_t v = 0; v < sizeof values / sizeof *values; v++)
        for (unsigned length = 0; length < 64; length += 7)
            for (unsigned index = 0; index + (length ? length : 64) <= 64; index += 5) {
                const uint64_t control = length | (uint64_t)index << 8 | 0xffff0000ull; /* upper bits ignored */
                uint64_t out[2];
                run_a(values[v], 0xfeedfacecafebeefull, control, out);
                assert(out[0] == model(values[v], control));
                assert(out[1] == 0xfeedfacecafebeefull);
                run_b(values[v], 0x0badf00d0badf00dull, control, out);
                assert(out[0] == model(values[v], control));
                assert(out[1] == 0x0badf00d0badf00dull);
                checked++;
            }
    assert(t.hits[0] == checked && t.hits[1] == checked);
    printf("isa extrq test: %u field cases on both sites passed\n", checked);
    return 0;
}
