/*
 * syscall_emu.h - Linux ARM syscall emulation
 */
#ifndef SYSCALL_EMU_H
#define SYSCALL_EMU_H

#include <stdint.h>

typedef struct bbb_sim bbb_sim_t;

/* ARM Linux syscall numbers */
#define SYS_EXIT            1
#define SYS_READ            3
#define SYS_WRITE           4
#define SYS_OPEN            5
#define SYS_CLOSE           6
#define SYS_BRK             45
#define SYS_IOCTL           54
#define SYS_MUNMAP           91
#define SYS_WRITEV          146
#define SYS_NANOSLEEP       162
#define SYS_MMAP2           192
#define SYS_FSTAT64         197
#define SYS_EXIT_GROUP      248
#define SYS_SET_TLS         0xF0005
#define SYS_CLOCK_GETTIME   263
#define SYS_GETTID          224
#define SYS_TGKILL          268
#define SYS_SET_ROBUST_LIST 338

/* Fake file descriptor for /dev/mem */
#define DEVMEM_FD           100

/* Heap region */
#define HEAP_BASE           0x81000000
#define HEAP_MAX            0x82000000

/* Initialize syscall emulation */
void syscall_emu_init(bbb_sim_t *sim);

/* Handle a syscall (called from SVC interrupt hook) */
void syscall_emu_handle(bbb_sim_t *sim);

#endif /* SYSCALL_EMU_H */
