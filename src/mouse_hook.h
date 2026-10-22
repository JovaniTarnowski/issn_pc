/* SPDX-License-Identifier: GPL-2.0-or-later */
/* inFAMOUS Second Son: 1:1 mouse aim, ported from axylpup's shadPS4 build (src/core/mouse_aim.cpp,
 * release 1.0.2; his notes/study-input-kbm-2026-10-14.md sec. 6). The gameplay camera update
 * (0x14ddd0) turns the right stick into pan / tilt speeds and stores the integrated angles at
 * camera+0x330 with one instruction (0x14ebce). A cave at that store adds the mouse's yaw and
 * pitch, so mouse motion skips the stick curve, sensitivity, acceleration and aim assist while
 * a pad's stick keeps the game's feel.
 *
 * Offsets are eboot virtual addresses (= image offsets); checked on CUSA00223 with the 1.07
 * update (tester's eboot, 2026-10-22: the same code and data as axylpup's CUSA00004). The patch
 * goes in only when every byte below matches. */
#ifndef ISSN_MOUSE_HOOK_H
#define ISSN_MOUSE_HOOK_H
#include <stdint.h>
#include <string.h>

#define ISSN_MOUSE_STORE 0x14ebceu      /* vmovups %xmm0,0x330(%r13), 9 bytes */
#define ISSN_MOUSE_STORE_LENGTH 9u
#define ISSN_MOUSE_EXPECTED_AT 0x14ebc5u /* the store before, the store, the store after */
static const unsigned char issn_mouse_expected[27] = {
    0xc5, 0xf8, 0x11, 0x84, 0x24, 0x00, 0x01, 0x00, 0x00, /* vmovups %xmm0,0x100(%rsp) */
    0xc4, 0xc1, 0x78, 0x11, 0x85, 0x30, 0x03, 0x00, 0x00, /* vmovups %xmm0,0x330(%r13) */
    0xc4, 0xc1, 0x7a, 0x7f, 0x8d, 0x40, 0x03, 0x00, 0x00, /* vmovdqu %xmm1,0x340(%r13) */
};
/* lea 0xe16c40(%rip),%rax at 0x14936e: the game's "no auto-follow" byte (.bss), read by the
 * camera's follow logic. The right stick turns follow off through the game's own event; the
 * mouse never does, so the runtime sets the byte while the mouse moves. */
#define ISSN_MOUSE_NO_FOLLOW_LEA 0x14936eu
#define ISSN_MOUSE_NO_FOLLOW 0xe16c40u
/* lea 0xefacf8(%rip),%rax at 0x14e5f3: the options; +0x6d5 / +0x6d6 invert the camera's X / Y. */
#define ISSN_MOUSE_OPTIONS_LEA 0x14e5f3u
#define ISSN_MOUSE_OPTIONS 0xefacf8u
#define ISSN_MOUSE_INVERT_X 0x6d5u
#define ISSN_MOUSE_INVERT_Y 0x6d6u

/* The cave (assembled from axylpup's Xbyak code). Registers at the store: xmm0 = (pan, tilt, 0,
 * 0) to store, r13 = camera, r12 = camera data, 0x50(%rsp) = the context whose +0x1c0 pair limits
 * the tilt together with the data's +0x1c0 pair; rax, r11, xmm2-xmm5 and the flags are dead there
 * (his disassembly review). It takes the mailbox's motion (leaving 0), adds it, clamps the tilt to
 * the game's limits (a tilt already outside them is not pulled in), reports the result, replays
 * the store and jumps back. ISSN_MOUSE_PATCH_MAILBOX / _RETURN: the 8-byte slots to fill. */
static const unsigned char issn_mouse_cave_clamp[127] = {
    0x48, 0xb8, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, /* movabs $mailbox,%rax */
    0x45, 0x31, 0xdb,                                           /* xor %r11d,%r11d */
    0x4c, 0x87, 0x18,                                           /* xchg %r11,(%rax) */
    0x4d, 0x85, 0xdb,                                           /* test %r11,%r11 */
    0x74, 0x4b,                                                 /* je store */
    0xc4, 0xc1, 0xf9, 0x6e, 0xd3,                               /* vmovq %r11,%xmm2 */
    0xc5, 0xf8, 0x58, 0xda,                                     /* vaddps %xmm2,%xmm0,%xmm3 */
    0x4c, 0x8b, 0x5c, 0x24, 0x50,                               /* mov 0x50(%rsp),%r11 */
    0xc4, 0xc1, 0x7b, 0x10, 0xa4, 0x24, 0xc0, 0x01, 0x00, 0x00, /* vmovsd 0x1c0(%r12),%xmm4 */
    0xc4, 0xc1, 0x7b, 0x10, 0x93, 0xc0, 0x01, 0x00, 0x00,       /* vmovsd 0x1c0(%r11),%xmm2 */
    0xc5, 0xd8, 0x5f, 0xea,                                     /* vmaxps %xmm2,%xmm4,%xmm5 */
    0xc5, 0xd8, 0x5d, 0xe2,                                     /* vminps %xmm2,%xmm4,%xmm4 */
    0xc4, 0xe3, 0x79, 0x04, 0xed, 0x00,                         /* vpermilps $0,%xmm5,%xmm5 */
    0xc4, 0xe3, 0x79, 0x04, 0xe4, 0x55,                         /* vpermilps $0x55,%xmm4,%xmm4 */
    0xc5, 0xd0, 0x5d, 0xe8,                                     /* vminps %xmm0,%xmm5,%xmm5 */
    0xc5, 0xd8, 0x5f, 0xe0,                                     /* vmaxps %xmm0,%xmm4,%xmm4 */
    0xc5, 0xd8, 0x5d, 0xd3,                                     /* vminps %xmm3,%xmm4,%xmm2 */
    0xc5, 0xe8, 0x5f, 0xd5,                                     /* vmaxps %xmm5,%xmm2,%xmm2 */
    0xc4, 0xe3, 0x61, 0x0c, 0xc2, 0x02,                         /* vblendps $2,%xmm2,%xmm3,%xmm0 */
    0xc5, 0xf9, 0xd6, 0x40, 0x08,                               /* store: vmovq %xmm0,8(%rax) */
    0xff, 0x40, 0x10,                                           /* incl 0x10(%rax) */
    0xc4, 0xc1, 0x78, 0x11, 0x85, 0x30, 0x03, 0x00, 0x00,       /* vmovups %xmm0,0x330(%r13) */
    0xff, 0x25, 0x00, 0x00, 0x00, 0x00,                         /* jmp *0(%rip) */
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,             /* return address */
};
/* ISSN_MOUSE_AIM_CLAMP=0: no tilt limit from the cave. */
static const unsigned char issn_mouse_cave_free[65] = {
    0x48, 0xb8, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x45, 0x31, 0xdb, 0x4c, 0x87,
    0x18, 0x4d, 0x85, 0xdb, 0x74, 0x0d, 0xc4, 0xc1, 0xf9, 0x6e, 0xd3, 0xc5, 0xf8, 0x58, 0xda,
    0xc5, 0xf8, 0x28, 0xc3, /* vmovaps %xmm3,%xmm0 */
    0xc5, 0xf9, 0xd6, 0x40, 0x08, 0xff, 0x40, 0x10, 0xc4, 0xc1, 0x78, 0x11, 0x85, 0x30, 0x03,
    0x00, 0x00, 0xff, 0x25, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22,
};
#define ISSN_MOUSE_PATCH_MAILBOX 2u

/* The mailbox the cave and the runtime share (runtime_pad.c). */
typedef struct {
    uint64_t delta;  /* f32 yaw, f32 pitch (radians); the cave swaps it with 0 every camera update */
    float pan, tilt; /* the stored angles after the cave */
    uint32_t frames; /* camera updates seen by the cave */
} IssnMouseMailbox;

static inline int issn_mouse_lea_targets(const unsigned char *image, uint32_t at, uint32_t target) {
    int32_t disp;
    if (image[at] != 0x48 || image[at + 1] != 0x8d || image[at + 2] != 0x05) return 0;
    memcpy(&disp, image + at + 3, 4);
    return (int64_t)at + 7 + disp == (int64_t)target;
}

static inline int issn_mouse_signature_match(const unsigned char *image, uint64_t size) {
    return size > ISSN_MOUSE_OPTIONS + ISSN_MOUSE_INVERT_Y &&
           !memcmp(image + ISSN_MOUSE_EXPECTED_AT, issn_mouse_expected, sizeof(issn_mouse_expected)) &&
           issn_mouse_lea_targets(image, ISSN_MOUSE_NO_FOLLOW_LEA, ISSN_MOUSE_NO_FOLLOW) &&
           issn_mouse_lea_targets(image, ISSN_MOUSE_OPTIONS_LEA, ISSN_MOUSE_OPTIONS);
}

#endif
