/*
 * cpu_emu.c - Unicorn Engine wrapper for ARM Cortex-A8 emulation
 */
#include "cpu_emu.h"
#include "bbb_sim.h"
#include "syscall_emu.h"
#include "ko_loader.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* SVC/interrupt hook - handle Linux syscalls and mock kernel calls */
static void hook_intr(uc_engine *uc, uint32_t intno, void *user_data)
{
    cpu_emu_t *cpu = (cpu_emu_t *)user_data;

    if (intno == 2) {
        /* ARM SVC - read the SVC instruction to get the immediate value */
        uint32_t pc = cpu_emu_get_pc(cpu);
        uint32_t svc_instr = 0;
        uc_mem_read(uc, pc - 4, &svc_instr, 4); /* PC already advanced past SVC */
        uint32_t svc_num = svc_instr & 0x00FFFFFF;

        if (svc_num == 0xFF) {
            /* Mock kernel SVC - R12 contains mock function ID */
            uint32_t mock_id = cpu_emu_get_reg(cpu, UC_ARM_REG_R12);
            ko_handle_mock_svc(cpu->sim, mock_id);
        } else {
            /* Linux syscall (SVC #0 / SWI #0) */
            syscall_emu_handle(cpu->sim);
        }
    }
}

/* Code hook - for instruction counting and step mode */
static void hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    cpu_emu_t *cpu = (cpu_emu_t *)user_data;
    cpu->instr_count++;

    if (cpu->stop_requested) {
        uc_emu_stop(uc);
    }

    if (cpu->step_mode) {
        uc_emu_stop(uc);
    }
}

cpu_emu_t *cpu_emu_create(bbb_sim_t *sim)
{
    cpu_emu_t *cpu = calloc(1, sizeof(cpu_emu_t));
    if (!cpu) return NULL;
    cpu->sim = sim;
    pthread_mutex_init(&cpu->run_lock, NULL);
    pthread_cond_init(&cpu->run_cond, NULL);
    return cpu;
}

void cpu_emu_destroy(cpu_emu_t *cpu)
{
    if (!cpu) return;
    if (cpu->uc) {
        uc_close(cpu->uc);
    }
    pthread_mutex_destroy(&cpu->run_lock);
    pthread_cond_destroy(&cpu->run_cond);
    free(cpu);
}

int cpu_emu_init(cpu_emu_t *cpu)
{
    uc_err err;

    /* Initialize Unicorn for ARM mode */
    err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &cpu->uc);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[CPU] Failed to initialize Unicorn: %s\n", uc_strerror(err));
        return -1;
    }

    /* Add interrupt hook for SVC (syscall) handling */
    err = uc_hook_add(cpu->uc, &cpu->hook_intr, UC_HOOK_INTR, hook_intr, cpu, 1, 0);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[CPU] Failed to add interrupt hook: %s\n", uc_strerror(err));
        return -1;
    }

    /* Add code hook for instruction counting / step mode */
    err = uc_hook_add(cpu->uc, &cpu->hook_code, UC_HOOK_CODE, hook_code, cpu, 1, 0);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[CPU] Failed to add code hook: %s\n", uc_strerror(err));
        return -1;
    }

    printf("[CPU] Unicorn ARM engine initialized\n");
    return 0;
}

void cpu_emu_reset(cpu_emu_t *cpu)
{
    cpu->instr_count = 0;
    cpu->running = false;
    cpu->step_mode = false;
    cpu->stop_requested = false;

    /* Reset all registers to 0 */
    uint32_t zero = 0;
    for (int i = UC_ARM_REG_R0; i <= UC_ARM_REG_R12; i++) {
        uc_reg_write(cpu->uc, i, &zero);
    }

    /* Set SP to top of DDR */
    uint32_t sp = AM335X_STACK_TOP;
    uc_reg_write(cpu->uc, UC_ARM_REG_SP, &sp);
}

int cpu_emu_map_memory(cpu_emu_t *cpu, uint64_t addr, size_t size, uint32_t perms)
{
    uc_err err = uc_mem_map(cpu->uc, addr, size, perms);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[CPU] mem_map(0x%lx, 0x%lx) failed: %s\n",
                (unsigned long)addr, (unsigned long)size, uc_strerror(err));
        return -1;
    }
    return 0;
}

int cpu_emu_write_mem(cpu_emu_t *cpu, uint64_t addr, const void *data, size_t size)
{
    uc_err err = uc_mem_write(cpu->uc, addr, data, size);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[CPU] mem_write(0x%lx, %zu) failed: %s\n",
                (unsigned long)addr, size, uc_strerror(err));
        return -1;
    }
    return 0;
}

int cpu_emu_read_mem(cpu_emu_t *cpu, uint64_t addr, void *data, size_t size)
{
    uc_err err = uc_mem_read(cpu->uc, addr, data, size);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[CPU] mem_read(0x%lx, %zu) failed: %s\n",
                (unsigned long)addr, size, uc_strerror(err));
        return -1;
    }
    return 0;
}

uint32_t cpu_emu_get_reg(cpu_emu_t *cpu, int regid)
{
    uint32_t val = 0;
    uc_reg_read(cpu->uc, regid, &val);
    return val;
}

void cpu_emu_set_reg(cpu_emu_t *cpu, int regid, uint32_t value)
{
    uc_reg_write(cpu->uc, regid, &value);
}

uint32_t cpu_emu_get_pc(cpu_emu_t *cpu)
{
    return cpu_emu_get_reg(cpu, UC_ARM_REG_PC);
}

uint32_t cpu_emu_get_sp(cpu_emu_t *cpu)
{
    return cpu_emu_get_reg(cpu, UC_ARM_REG_SP);
}

/* Worker thread entry point */
static void *cpu_worker_thread(void *arg)
{
    cpu_emu_t *cpu = (cpu_emu_t *)arg;

    while (!cpu->stop_requested) {
        uint32_t pc = cpu_emu_get_pc(cpu);
        uc_err err = uc_emu_start(cpu->uc, pc, 0, 0, 0);

        if (err != UC_ERR_OK) {
            fprintf(stderr, "[CPU] Emulation error at PC=0x%08X: %s\n",
                    cpu_emu_get_pc(cpu), uc_strerror(err));
            cpu->running = false;
            cpu->sim->state = SIM_STATE_ERROR;
            break;
        }

        if (cpu->step_mode) {
            cpu->running = false;
            cpu->sim->state = SIM_STATE_PAUSED;
            break;
        }

        if (cpu->stop_requested) {
            break;
        }
    }

    cpu->running = false;
    return NULL;
}

int cpu_emu_start(cpu_emu_t *cpu, uint64_t pc)
{
    if (cpu->running) return -1;

    cpu->running = true;
    cpu->stop_requested = false;
    cpu->step_mode = false;

    /* Set PC */
    uint32_t pc32 = (uint32_t)pc;
    uc_reg_write(cpu->uc, UC_ARM_REG_PC, &pc32);

    /* Start worker thread */
    int ret = pthread_create(&cpu->thread, NULL, cpu_worker_thread, cpu);
    if (ret != 0) {
        fprintf(stderr, "[CPU] Failed to create worker thread\n");
        cpu->running = false;
        return -1;
    }
    pthread_detach(cpu->thread);

    return 0;
}

void cpu_emu_stop(cpu_emu_t *cpu)
{
    cpu->stop_requested = true;
    if (cpu->uc) {
        uc_emu_stop(cpu->uc);
    }
}

void cpu_emu_step(cpu_emu_t *cpu)
{
    if (cpu->running) return;

    cpu->step_mode = true;
    cpu->stop_requested = false;
    cpu->running = true;

    uint32_t pc = cpu_emu_get_pc(cpu);
    uc_err err = uc_emu_start(cpu->uc, pc, 0, 0, 1);  /* Execute 1 instruction */
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[CPU] Step error: %s\n", uc_strerror(err));
    }

    cpu->running = false;
    cpu->step_mode = false;
}
