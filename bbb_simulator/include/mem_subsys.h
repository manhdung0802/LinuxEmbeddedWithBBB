/*
 * mem_subsys.h - Memory subsystem with MMIO dispatch
 */
#ifndef MEM_SUBSYS_H
#define MEM_SUBSYS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;
typedef struct cpu_emu cpu_emu_t;

/* MMIO read/write callbacks */
typedef uint32_t (*mmio_read_cb)(void *opaque, uint32_t addr, uint32_t offset);
typedef void (*mmio_write_cb)(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

#define MAX_MMIO_REGIONS 64

typedef struct mmio_region {
    uint32_t base;
    uint32_t size;
    mmio_read_cb read;
    mmio_write_cb write;
    void *opaque;
    const char *name;
    bool active;
} mmio_region_t;

typedef struct mem_subsys {
    bbb_sim_t *sim;
    mmio_region_t regions[MAX_MMIO_REGIONS];
    int num_regions;
} mem_subsys_t;

/* Lifecycle */
mem_subsys_t *mem_subsys_create(bbb_sim_t *sim);
void mem_subsys_destroy(mem_subsys_t *mem);
int mem_subsys_init(mem_subsys_t *mem, cpu_emu_t *cpu);

/* Register MMIO region */
int mem_subsys_register(mem_subsys_t *mem, uint32_t base, uint32_t size,
                        mmio_read_cb read_cb, mmio_write_cb write_cb,
                        void *opaque, const char *name);

/* Dispatch MMIO access (called from Unicorn hooks) */
uint32_t mem_subsys_read(mem_subsys_t *mem, uint32_t addr, int size);
void mem_subsys_write(mem_subsys_t *mem, uint32_t addr, int size, uint32_t value);

/* Lookup region by address */
mmio_region_t *mem_subsys_find_region(mem_subsys_t *mem, uint32_t addr);

#endif /* MEM_SUBSYS_H */
