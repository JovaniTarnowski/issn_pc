/* SPDX-License-Identifier: GPL-2.0-or-later
 * issn: system functions the inFAMOUS Second Son 01.07 update (CUSA00223) imports and 01.00 did
 * not. Answers of an offline base PS4 with the whole game installed. Ported from the iss_pc port
 * by HayatoG (github.com/HayatoG/iss_pc: runtime_tlg.c, runtime_gr2.c, runtime_infamous.c, GPL-2.0). */
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "runtime.h"

#define KERR_EINVAL ((int32_t)0x80020016)
#define KERR_ESRCH ((int32_t)0x80020003)
#define NP_SIGNED_OUT ((int32_t)0x80550006)
#define DISC_MAP_NO_BITMAP ((int32_t)0x81100004)

static void once(const char *what) { printf("Runtime: %s (01.07)\n", what); fflush(stdout); }

/* Base PS4, not a Pro. */
static ABI int32_t is_neo_mode(void) { return 0; }
static ABI int32_t prt_aperture(int32_t index, void **address, uint64_t *size) {
    if (index < 0 || index > 2 || !address || !size) return KERR_EINVAL;
    *address = NULL; *size = 0; return 0;
}
/* The libc unwinder asks for a module's .eh_frame_hdr only when a C++ exception is thrown. */
static ABI int32_t module_info_for_unwind(uintptr_t address, int32_t flags, void *info) {
    (void)flags; (void)info;
    fprintf(stderr, "Runtime: sceKernelGetModuleInfoForUnwind(%#lx) not implemented (C++ exception?)\n",
            (unsigned long)address);
    return KERR_ESRCH;
}
static ABI int32_t module_segment_info(void *info) {
    (void)info; once("sceKernelInternalMemoryGetModuleSegmentInfo"); return KERR_ESRCH;
}

/* Local time and its offset from UTC in seconds. */
static int local_time(time_t t, struct tm *out, int64_t *offset) {
#ifdef _WIN32
    if (localtime_s(out, &t)) return 0;
    struct tm copy = *out;
    *offset = (int64_t)_mkgmtime(&copy) - (int64_t)t;
#else
    if (!localtime_r(&t, out)) return 0;
    *offset = out->tm_gmtoff;
#endif
    return 1;
}
typedef struct { int64_t t; uint32_t west_sec, dst_sec; } Timesec;
typedef struct { int32_t minutes_west, dst_time; } Timezone;
static ABI int32_t utc_to_local(int64_t utc, int64_t *local, Timesec *ts, uint64_t *dst) {
    struct tm tm; int64_t offset;
    if (!local_time((time_t)utc, &tm, &offset)) return KERR_EINVAL;
    int64_t dst_sec = tm.tm_isdst > 0 ? 3600 : 0;
    if (local) *local = utc + offset;
    if (ts) { ts->t = utc; ts->west_sec = (uint32_t)(offset - dst_sec); ts->dst_sec = (uint32_t)dst_sec; }
    if (dst) *dst = (uint64_t)dst_sec;
    return 0;
}
static ABI int32_t local_to_utc(int64_t local, int64_t unused, int64_t *utc, Timezone *tz, int32_t *dst) {
    (void)unused;
    struct tm tm; int64_t offset;
    if (!local_time((time_t)local, &tm, &offset)) return KERR_EINVAL;
    if (utc) *utc = local - offset;
    if (tz) { tz->minutes_west = (int32_t)(-offset / 60); tz->dst_time = tm.tm_isdst > 0; }
    if (dst) *dst = tm.tm_isdst > 0 ? 3600 : 0;
    return 0;
}

/* libc's internal heap tracing: off. */
typedef struct { uint64_t size; uint32_t flag, get_segment_info; uint64_t *mspace_id_mask, *mstate_table; } HeapTraceInfo;
static ABI int32_t heap_trace_info(HeapTraceInfo *info) {
    static uint64_t mask, table[64];
    if (!info) return -1;
    info->get_segment_info = 0; info->mspace_id_mask = &mask; info->mstate_table = table;
    return 0;
}

/* Error dialog: shown at once and finished. */
typedef struct { uint64_t size; int32_t error_code; } ErrorDialogParam;
static int32_t error_dialog_status;
static ABI int32_t error_dialog_init(void) { error_dialog_status = 1; return 0; }
static ABI int32_t error_dialog_open(const ErrorDialogParam *param) {
    printf("Runtime: sceErrorDialogOpen(error %#x)\n", param ? (unsigned)param->error_code : 0u);
    fflush(stdout);
    error_dialog_status = 3; /* finished */
    return 0;
}
static ABI int32_t error_dialog_status_get(void) { return error_dialog_status; }
static ABI int32_t error_dialog_term(void) { error_dialog_status = 0; return 0; }

static ABI int32_t ok(void) { return 0; }
/* PSN signed out; the "A" NpManager variants take a user id. */
static ABI int32_t signed_out(void) { return NP_SIGNED_OUT; }
static ABI int32_t np_register_a(void *callback, void *arg) {
    (void)callback; (void)arg;
    static int32_t next = 0x100;
    return ++next;
}
static ABI int32_t discmap_7c98(const char *path, int64_t offset, int64_t size, int32_t *a, int32_t *b, int32_t *c) {
    (void)path; (void)offset; (void)size; (void)a; (void)b; (void)c;
    return DISC_MAP_NO_BITMAP;
}

static const RuntimeExport exports[] = {
    {"sceKernelIsNeoMode", is_neo_mode}, {"sceKernelGetPrtAperture", prt_aperture},
    {"sceKernelGetModuleInfoForUnwind", module_info_for_unwind},
    {"sceKernelInternalMemoryGetModuleSegmentInfo", module_segment_info},
    {"sceKernelConvertUtcToLocaltime", utc_to_local}, {"sceKernelConvertLocaltimeToUtc", local_to_utc},
    {"sceLibcHeapGetTraceInfo", heap_trace_info},
    {"sceErrorDialogInitialize", error_dialog_init}, {"sceErrorDialogOpen", error_dialog_open},
    {"sceErrorDialogUpdateStatus", error_dialog_status_get}, {"sceErrorDialogGetStatus", error_dialog_status_get},
    {"sceErrorDialogTerminate", error_dialog_term},
    {"sceHttpSetChunkedTransferEnabled", ok},
    {"sceNpCheckNpAvailabilityA", signed_out}, {"sceNpGetAccountCountryA", signed_out},
    {"sceNpGetAccountIdA", signed_out}, {"sceNpGetGamePresenceStatusA", signed_out},
    {"sceNpRegisterGamePresenceCallbackA", np_register_a}, {"sceNpRegisterStateCallbackA", np_register_a},
    {"sceDiscMap_7C980FFB0AA27E7A", discmap_7c98},
};
uintptr_t runtime_issn107_resolve(const char *name) { return RUNTIME_LOOKUP(exports, name); }
