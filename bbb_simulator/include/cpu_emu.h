/*
 * cpu_emu.h - Unicorn Engine wrapper for ARM Cortex-A8 emulation
 */
#ifndef CPU_EMU_H
#define CPU_EMU_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unicorn/unicorn.h>

typedef struct bbb_sim bbb_sim_t;

typedef struct cpu_emu {
    uc_engine *uc;
    bbb_sim_t *sim;

    /* Hooks */
    uc_hook hook_intr;      /* Interrupt/syscall hook */
    uc_hook hook_mmio_r;    /* MMIO read hook */
    uc_hook hook_mmio_w;    /* MMIO write hook */
    uc_hook hook_code;      /* Code execution hook (for step/breakpoint) */

    /* Worker thread */
    pthread_t thread;
    volatile bool running;
    volatile bool step_mode;
    volatile bool stop_requested;
    pthread_mutex_t run_lock;
    pthread_cond_t run_cond;

    /* Statistics */
    uint64_t instr_count;
} cpu_emu_t;

/* Lifecycle */
cpu_emu_t *cpu_emu_create(bbb_sim_t *sim);
void cpu_emu_destroy(cpu_emu_t *cpu);
int cpu_emu_init(cpu_emu_t *cpu);
void cpu_emu_reset(cpu_emu_t *cpu);

/* Memory mapping */
int cpu_emu_map_memory(cpu_emu_t *cpu, uint64_t addr, size_t size, uint32_t perms);
int cpu_emu_write_mem(cpu_emu_t *cpu, uint64_t addr, const void *data, size_t size);
int cpu_emu_read_mem(cpu_emu_t *cpu, uint64_t addr, void *data, size_t size);

/* Execution control */
int cpu_emu_start(cpu_emu_t *cpu, uint64_t pc);
void cpu_emu_stop(cpu_emu_t *cpu);
void cpu_emu_step(cpu_emu_t *cpu);

/* Register access */
uint32_t cpu_emu_get_reg(cpu_emu_t *cpu, int regid);
void cpu_emu_set_reg(cpu_emu_t *cpu, int regid, uint32_t value);
uint32_t cpu_emu_get_pc(cpu_emu_t *cpu);
uint32_t cpu_emu_get_sp(cpu_emu_t *cpu);

#endif /* CPU_EMU_H */
