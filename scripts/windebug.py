"""issn: runs the probe under a minimal Windows debugger (GOW3_DEBUGGER=1).

A fast fail (status 0xC0000409: __fastfail, a /GS stack cookie check, an invalid-parameter
report in a DLL with its own C runtime) ends a process without running any of its handlers, and
Windows logs it only when error reporting is on. A debugger is told: this one reports the module
and offset, the fast fail code, the registers and the stack of the thread, and of any exception
the process does not handle. Every other exception goes back to the process (its own handlers
resolve the GPU's page-tracking faults and the ISA rewrite traps), so the game runs as usual,
only slower where it faults often.
"""
import ctypes
import os
import subprocess
import sys
from ctypes import wintypes

DEBUG_ONLY_THIS_PROCESS = 0x2
EXCEPTION_DEBUG_EVENT, CREATE_THREAD_DEBUG_EVENT, CREATE_PROCESS_DEBUG_EVENT = 1, 2, 3
EXIT_PROCESS_DEBUG_EVENT, LOAD_DLL_DEBUG_EVENT, UNLOAD_DLL_DEBUG_EVENT = 5, 6, 7
DBG_CONTINUE, DBG_EXCEPTION_NOT_HANDLED = 0x00010002, 0x80010001
STATUS_BREAKPOINT, STATUS_FAST_FAIL = 0x80000003, 0xC0000409
THREAD_ALL_ACCESS = 0x1FFFFF
CONTEXT_AMD64_CONTROL_INTEGER = 0x100003
CONTEXT_SIZE = 1232

FAST_FAIL_CODES = {
    2: 'stack cookie check (/GS): a stack buffer overrun',
    3: 'corrupt list entry',
    5: 'invalid argument (a C runtime invalid-parameter report)',
    7: 'fatal application exit (abort)',
    10: 'Control Flow Guard check',
    13: 'invalid exception chain',
    18: 'invalid jump buffer (longjmp)',
    0x39: 'shadow stack (CET) violation',
}


class ExceptionRecord(ctypes.Structure):
    _fields_ = [('code', ctypes.c_uint32), ('flags', ctypes.c_uint32), ('record', ctypes.c_void_p),
                ('address', ctypes.c_void_p), ('count', ctypes.c_uint32),
                ('info', ctypes.c_uint64 * 15)]


class ExceptionInfo(ctypes.Structure):
    _fields_ = [('record', ExceptionRecord), ('first_chance', ctypes.c_uint32)]


class CreateProcessInfo(ctypes.Structure):
    _fields_ = [('file', wintypes.HANDLE), ('process', wintypes.HANDLE), ('thread', wintypes.HANDLE),
                ('base', ctypes.c_void_p)]


class LoadDllInfo(ctypes.Structure):
    _fields_ = [('file', wintypes.HANDLE), ('base', ctypes.c_void_p)]


class UnloadDllInfo(ctypes.Structure):
    _fields_ = [('base', ctypes.c_void_p)]


class ExitProcessInfo(ctypes.Structure):
    _fields_ = [('exit_code', ctypes.c_uint32)]


class DebugUnion(ctypes.Union):
    _fields_ = [('exception', ExceptionInfo), ('create_process', CreateProcessInfo),
                ('load_dll', LoadDllInfo), ('unload_dll', UnloadDllInfo),
                ('exit_process', ExitProcessInfo), ('raw', ctypes.c_byte * 160)]


class DebugEvent(ctypes.Structure):
    _fields_ = [('code', ctypes.c_uint32), ('pid', ctypes.c_uint32), ('tid', ctypes.c_uint32),
                ('u', DebugUnion)]


def _kernel32():
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.WaitForDebugEvent.argtypes = [ctypes.POINTER(DebugEvent), wintypes.DWORD]
    k.ContinueDebugEvent.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.DWORD]
    k.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                    ctypes.POINTER(ctypes.c_size_t)]
    k.OpenThread.restype = wintypes.HANDLE
    k.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    k.GetThreadContext.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
    k.GetFinalPathNameByHandleW.argtypes = [wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD]
    k.CloseHandle.argtypes = [wintypes.HANDLE]
    return k


class Debugger:
    def __init__(self, k):
        self.k = k
        self.process = None
        self.modules = {}  # base -> (end, name)

    def read(self, address, size):
        buffer = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not self.k.ReadProcessMemory(self.process, ctypes.c_void_p(address), buffer, size, ctypes.byref(got)):
            return None
        return buffer.raw[:got.value]

    def add_module(self, file, base):
        name = '?'
        if file:
            path = ctypes.create_unicode_buffer(1024)
            if self.k.GetFinalPathNameByHandleW(file, path, 1024, 0):
                name = path.value.replace('\\', '/').rsplit('/', 1)[-1]
            self.k.CloseHandle(file)
        size = 0x1000
        header = self.read(base, 0x400)
        if header and header[:2] == b'MZ':
            pe = int.from_bytes(header[0x3c:0x40], 'little')
            if pe + 0x54 <= len(header):
                size = int.from_bytes(header[pe + 0x50:pe + 0x54], 'little')
        self.modules[base] = (base + size, name)

    def describe(self, address):
        for base, (end, name) in self.modules.items():
            if base <= address < end:
                return f'{name}+0x{address - base:x}'
        return f'0x{address:x}'

    def in_module(self, address):
        return any(base <= address < end for base, (end, _) in self.modules.items())

    def report(self, tid, info):
        record = info.record
        code = record.code
        first = 'first chance' if info.first_chance else 'not handled'
        out = sys.stderr
        print(f'STOP: debugger: exception 0x{code:08X} ({first}) at {self.describe(record.address or 0)} '
              f'on thread {tid}', file=out)
        if code == STATUS_FAST_FAIL and record.count:
            kind = record.info[0]
            print(f'  fast fail code {kind}: {FAST_FAIL_CODES.get(kind, "see winnt.h FAST_FAIL_*")}', file=out)
        thread = self.k.OpenThread(THREAD_ALL_ACCESS, False, tid)
        raw = ctypes.create_string_buffer(CONTEXT_SIZE + 16)
        address = (ctypes.addressof(raw) + 15) & ~15
        ctypes.c_uint32.from_address(address + 0x30).value = CONTEXT_AMD64_CONTROL_INTEGER
        if thread and self.k.GetThreadContext(thread, ctypes.c_void_p(address)):
            reg = lambda offset: ctypes.c_uint64.from_address(address + offset).value  # noqa: E731
            rip, rsp, rbp = reg(0xF8), reg(0x98), reg(0xA0)
            names = ['rax', 'rcx', 'rdx', 'rbx', 'rsp', 'rbp', 'rsi', 'rdi']
            print('  ' + ' '.join(f'{n}={reg(0x78 + 8 * i):016x}' for i, n in enumerate(names)), file=out)
            print(f'  rip={self.describe(rip)}', file=out)
            print('  frame chain (rbp):', file=out)
            for depth in range(24):
                frame = self.read(rbp, 16) if rbp else None
                if not frame:
                    break
                next_rbp, ret = int.from_bytes(frame[:8], 'little'), int.from_bytes(frame[8:], 'little')
                print(f'    #{depth} {self.describe(ret)}', file=out)
                if next_rbp <= rbp:
                    break
                rbp = next_rbp
            print('  return addresses on the stack (scan of 16 KiB from rsp, code of loaded modules):', file=out)
            stack = self.read(rsp, 16384) or b''
            shown = 0
            for at in range(0, len(stack) - 7, 8):
                value = int.from_bytes(stack[at:at + 8], 'little')
                if self.in_module(value):
                    print(f'    rsp+0x{at:04x} {self.describe(value)}', file=out)
                    shown += 1
                    if shown >= 48:
                        break
        if thread:
            self.k.CloseHandle(thread)
        out.flush()


def run(command):
    """Runs command under the debugger; returns its exit code."""
    k = _kernel32()
    env = dict(os.environ, _NO_DEBUG_HEAP='1')
    child = subprocess.Popen(command, stdin=subprocess.DEVNULL, env=env, creationflags=DEBUG_ONLY_THIS_PROCESS)
    debugger = Debugger(k)
    event = DebugEvent()
    initial_breakpoint = True
    reported = 0
    print('Debugger: watching issn-probe for fast fails and unhandled exceptions', file=sys.stderr, flush=True)
    while True:
        if not k.WaitForDebugEvent(ctypes.byref(event), 0xFFFFFFFF):
            print(f'Debugger: WaitForDebugEvent failed ({ctypes.get_last_error()})', file=sys.stderr)
            break
        status = DBG_CONTINUE
        if event.code == CREATE_PROCESS_DEBUG_EVENT:
            info = event.u.create_process
            debugger.process = info.process
            debugger.add_module(info.file, info.base)
        elif event.code == LOAD_DLL_DEBUG_EVENT:
            debugger.add_module(event.u.load_dll.file, event.u.load_dll.base)
        elif event.code == UNLOAD_DLL_DEBUG_EVENT:
            debugger.modules.pop(event.u.unload_dll.base, None)
        elif event.code == EXCEPTION_DEBUG_EVENT:
            info = event.u.exception
            code = info.record.code
            if code == STATUS_BREAKPOINT and initial_breakpoint:
                initial_breakpoint = False  # the loader's breakpoint at process start
            else:
                status = DBG_EXCEPTION_NOT_HANDLED
                if (code == STATUS_FAST_FAIL or not info.first_chance) and reported < 4:
                    reported += 1
                    debugger.report(event.tid, info)
        elif event.code == EXIT_PROCESS_DEBUG_EVENT:
            code = event.u.exit_process.exit_code
            k.ContinueDebugEvent(event.pid, event.tid, DBG_CONTINUE)
            print(f'Debugger: issn-probe exited with status 0x{code:08X}', file=sys.stderr, flush=True)
            break
        k.ContinueDebugEvent(event.pid, event.tid, status)
    return child.wait()
