/*
 * syscall_emu.c - Linux ARM syscall emulation
 *
 * Intercepts ARM SVC instructions and emulates key Linux syscalls
 * to allow statically-linked ARM binaries to run in the simulator.
 */
#include "syscall_emu.h"
#include "bbb_sim.h"
#include "cpu_emu.h"
#include "am335x_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <unicorn/unicorn.h>

/* Console output callback - declared in bbb_sim.c */
extern console_output_cb g_console_cb;
extern void *g_console_userdata;

/* Read a string from Unicorn memory (with length limit) */
static int read_string(cpu_emu_t *cpu, uint32_t addr, char *buf, int maxlen)
{
    int i;
    for (i = 0; i < maxlen - 1; i++) {
        uint8_t ch = 0;
        if (cpu_emu_read_mem(cpu, addr + i, &ch, 1) != 0) break;
        buf[i] = (char)ch;
        if (ch == 0) break;
    }
    buf[i] = '\0';
    return i;
}

void syscall_emu_init(bbb_sim_t *sim)
{
    sim->devmem_fd = -1;
    sim->brk_current = HEAP_BASE;
}

void syscall_emu_handle(bbb_sim_t *sim)
{
    cpu_emu_t *cpu = sim->cpu;
    uc_engine *uc = cpu->uc;

    /* ARM Linux syscall convention: R7 = syscall number, R0-R5 = args, R0 = return */
    uint32_t r7 = cpu_emu_get_reg(cpu, UC_ARM_REG_R7);
    uint32_t r0 = cpu_emu_get_reg(cpu, UC_ARM_REG_R0);
    uint32_t r1 = cpu_emu_get_reg(cpu, UC_ARM_REG_R1);
    uint32_t r2 = cpu_emu_get_reg(cpu, UC_ARM_REG_R2);
    uint32_t r3 = cpu_emu_get_reg(cpu, UC_ARM_REG_R3);
    uint32_t r4 = cpu_emu_get_reg(cpu, UC_ARM_REG_R4);
    uint32_t r5 = cpu_emu_get_reg(cpu, UC_ARM_REG_R5);
    (void)r3; (void)r4; (void)r5;

    uint32_t ret = 0;

    switch (r7) {
    case SYS_EXIT:
    case SYS_EXIT_GROUP:
        printf("[SYS] exit(%d)\n", (int)r0);
        cpu_emu_stop(cpu);
        sim->state = SIM_STATE_STOPPED;
        return;

    case SYS_READ: {
        /* read(fd, buf, count) */
        uint32_t fd = r0;
        uint32_t buf_addr = r1;
        uint32_t count = r2;
        (void)fd;
        /* For stdin (fd=0), we don't block - return 0 bytes for now */
        ret = 0;
        break;
    }

    case SYS_WRITE: {
        /* write(fd, buf, count) */
        uint32_t fd = r0;
        uint32_t buf_addr = r1;
        uint32_t count = r2;

        if (count > 4096) count = 4096;

        char *buf = malloc(count + 1);
        if (buf) {
            cpu_emu_read_mem(cpu, buf_addr, buf, count);
            buf[count] = '\0';

            if (fd == 1 || fd == 2) {
                /* stdout/stderr -> console output */
                printf("[OUT] %.*s", (int)count, buf);
                fflush(stdout);

                if (g_console_cb) {
                    g_console_cb(buf, count, g_console_userdata);
                }
            }
            free(buf);
        }
        ret = count;
        break;
    }

    case SYS_WRITEV: {
        /* writev(fd, iov, iovcnt) */
        uint32_t fd = r0;
        uint32_t iov_addr = r1;
        uint32_t iovcnt = r2;
        uint32_t total = 0;

        for (uint32_t i = 0; i < iovcnt && i < 16; i++) {
            uint32_t iov_base, iov_len;
            cpu_emu_read_mem(cpu, iov_addr + i * 8, &iov_base, 4);
            cpu_emu_read_mem(cpu, iov_addr + i * 8 + 4, &iov_len, 4);

            if (iov_len > 4096) iov_len = 4096;
            if (iov_len > 0 && (fd == 1 || fd == 2)) {
                char *buf = malloc(iov_len + 1);
                if (buf) {
                    cpu_emu_read_mem(cpu, iov_base, buf, iov_len);
                    buf[iov_len] = '\0';
                    printf("[OUT] %.*s", (int)iov_len, buf);
                    fflush(stdout);
                    if (g_console_cb) {
                        g_console_cb(buf, iov_len, g_console_userdata);
                    }
                    free(buf);
                }
            }
            total += iov_len;
        }
        ret = total;
        break;
    }

    case SYS_OPEN: {
        /* open(pathname, flags, mode) */
        char pathname[256];
        read_string(cpu, r0, pathname, sizeof(pathname));

        if (strcmp(pathname, "/dev/mem") == 0) {
            printf("[SYS] open(\"/dev/mem\") -> fd=%d\n", DEVMEM_FD);
            sim->devmem_fd = DEVMEM_FD;
            ret = DEVMEM_FD;
        } else {
            printf("[SYS] open(\"%s\") -> ENOENT\n", pathname);
            ret = (uint32_t)-2; /* -ENOENT */
        }
        break;
    }

    case SYS_CLOSE: {
        uint32_t fd = r0;
        if (fd == (uint32_t)DEVMEM_FD) {
            sim->devmem_fd = -1;
        }
        ret = 0;
        break;
    }

    case SYS_BRK: {
        /* brk(addr) */
        uint32_t new_brk = r0;
        if (new_brk == 0) {
            ret = sim->brk_current;
        } else if (new_brk >= HEAP_BASE && new_brk < HEAP_MAX) {
            sim->brk_current = new_brk;
            ret = new_brk;
        } else {
            ret = sim->brk_current;
        }
        break;
    }

    case SYS_MMAP2: {
        /* mmap2(addr, length, prot, flags, fd, pgoffset)
         * pgoffset is in 4096-byte pages, so physical_addr = pgoffset * 4096 */
        uint32_t addr = r0;
        uint32_t length = r1;
        uint32_t prot = r2;
        uint32_t flags = r3;
        uint32_t fd = r4;
        uint32_t pgoffset = r5;
        (void)addr; (void)prot; (void)flags;

        if (fd == (uint32_t)DEVMEM_FD || fd == (uint32_t)sim->devmem_fd) {
            /* mmap of /dev/mem - return the physical address directly
             * This works because Unicorn already has MMIO hooks at those addresses */
            uint32_t phys_addr = pgoffset * 4096;
            printf("[SYS] mmap2(/dev/mem, len=0x%X, phys=0x%08X) -> 0x%08X\n",
                   length, phys_addr, phys_addr);
            ret = phys_addr;
        } else if (flags & 0x20) { /* MAP_ANONYMOUS */
            /* Anonymous mmap - allocate from heap */
            uint32_t alloc_addr = sim->brk_current;
            /* Align to page */
            alloc_addr = (alloc_addr + 0xFFF) & ~0xFFF;
            sim->brk_current = alloc_addr + length;
            printf("[SYS] mmap2(anon, len=0x%X) -> 0x%08X\n", length, alloc_addr);
            ret = alloc_addr;
        } else {
            printf("[SYS] mmap2(fd=%d) -> unsupported\n", fd);
            ret = (uint32_t)-1; /* MAP_FAILED */
        }
        break;
    }

    case SYS_MUNMAP:
        /* munmap - no-op in simulator */
        ret = 0;
        break;

    case SYS_IOCTL:
        /* ioctl - stub */
        ret = 0;
        break;

    case SYS_NANOSLEEP: {
        /* nanosleep(req, rem) */
        uint32_t req_addr = r0;
        uint32_t tv_sec, tv_nsec;
        cpu_emu_read_mem(cpu, req_addr, &tv_sec, 4);
        cpu_emu_read_mem(cpu, req_addr + 4, &tv_nsec, 4);

        /* Sleep on host (scaled down for responsiveness) */
        uint64_t total_us = (uint64_t)tv_sec * 1000000 + tv_nsec / 1000;
        /* Cap at 2 seconds to keep simulator responsive */
        if (total_us > 2000000) total_us = 2000000;

        usleep((useconds_t)total_us);
        ret = 0;
        break;
    }

    case SYS_CLOCK_GETTIME: {
        /* clock_gettime(clockid, tp) */
        uint32_t tp_addr = r1;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint32_t sec = (uint32_t)ts.tv_sec;
        uint32_t nsec = (uint32_t)ts.tv_nsec;
        cpu_emu_write_mem(cpu, tp_addr, &sec, 4);
        cpu_emu_write_mem(cpu, tp_addr + 4, &nsec, 4);
        ret = 0;
        break;
    }

    case SYS_FSTAT64:
        /* fstat64 - return 0 (stub) */
        ret = 0;
        break;

    case SYS_GETTID:
        ret = 1; /* Fake TID */
        break;

    case SYS_SET_TLS:
        /* set_tls - store TLS pointer, no-op for our purposes */
        ret = 0;
        break;

    case SYS_SET_ROBUST_LIST:
        ret = 0;
        break;

    case SYS_TGKILL:
        printf("[SYS] tgkill - stopping emulation\n");
        cpu_emu_stop(cpu);
        sim->state = SIM_STATE_STOPPED;
        return;

    default:
        printf("[SYS] Unhandled syscall %d (R0=0x%X R1=0x%X R2=0x%X)\n",
               r7, r0, r1, r2);
        ret = (uint32_t)-38; /* -ENOSYS */
        break;
    }

    /* Set return value in R0 */
    cpu_emu_set_reg(cpu, UC_ARM_REG_R0, ret);
}
