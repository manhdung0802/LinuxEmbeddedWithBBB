/*
 * mem_subsys.c - Memory subsystem with MMIO dispatch
 */
#include "mem_subsys.h"
#include "bbb_sim.h"
#include "cpu_emu.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

mem_subsys_t *mem_subsys_create(bbb_sim_t *sim)
{
    mem_subsys_t *mem = calloc(1, sizeof(mem_subsys_t));
    if (!mem) return NULL;
    mem->sim = sim;
    return mem;
}

void mem_subsys_destroy(mem_subsys_t *mem)
{
    if (mem) free(mem);
}

int mem_subsys_register(mem_subsys_t *mem, uint32_t base, uint32_t size,
                        mmio_read_cb read_cb, mmio_write_cb write_cb,
                        void *opaque, const char *name)
{
    if (mem->num_regions >= MAX_MMIO_REGIONS) {
        fprintf(stderr, "[MEM] Too many MMIO regions\n");
        return -1;
    }

    mmio_region_t *r = &mem->regions[mem->num_regions++];
    r->base = base;
    r->size = size;
    r->read = read_cb;
    r->write = write_cb;
    r->opaque = opaque;
    r->name = name;
    r->active = true;

    return 0;
}

mmio_region_t *mem_subsys_find_region(mem_subsys_t *mem, uint32_t addr)
{
    for (int i = 0; i < mem->num_regions; i++) {
        mmio_region_t *r = &mem->regions[i];
        if (r->active && addr >= r->base && addr < r->base + r->size) {
            return r;
        }
    }
    return NULL;
}

uint32_t mem_subsys_read(mem_subsys_t *mem, uint32_t addr, int size)
{
    mmio_region_t *r = mem_subsys_find_region(mem, addr);
    if (r && r->read) {
        uint32_t offset = addr - r->base;
        /* Align to 32-bit */
        offset &= ~3u;
        uint32_t val = r->read(r->opaque, addr, offset);
        return val;
    }
    return 0;
}

void mem_subsys_write(mem_subsys_t *mem, uint32_t addr, int size, uint32_t value)
{
    mmio_region_t *r = mem_subsys_find_region(mem, addr);
    if (r && r->write) {
        uint32_t offset = addr - r->base;
        offset &= ~3u;
        r->write(r->opaque, addr, offset, value);
    }
}

/* Unicorn MMIO hook callbacks - forward to mem_subsys dispatch */
static void uc_mmio_read_hook(uc_engine *uc, uc_mem_type type,
                               uint64_t address, int size,
                               int64_t value, void *user_data)
{
    /* This hook is UC_HOOK_MEM_READ - we need to write the value back */
    mem_subsys_t *mem = (mem_subsys_t *)user_data;
    uint32_t val = mem_subsys_read(mem, (uint32_t)address, size);

    /* Write the MMIO read value into the memory so the CPU sees it */
    uc_mem_write(uc, address, &val, 4);
}

static void uc_mmio_write_hook(uc_engine *uc, uc_mem_type type,
                                uint64_t address, int size,
                                int64_t value, void *user_data)
{
    mem_subsys_t *mem = (mem_subsys_t *)user_data;
    mem_subsys_write(mem, (uint32_t)address, size, (uint32_t)value);
}

int mem_subsys_init(mem_subsys_t *mem, cpu_emu_t *cpu)
{
    uc_err err;

    /* Map DDR region */
    err = uc_mem_map(cpu->uc, AM335X_DDR_BASE, AM335X_DDR_SIZE, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to map DDR: %s\n", uc_strerror(err));
        return -1;
    }

    /* Map OCMC SRAM */
    err = uc_mem_map(cpu->uc, AM335X_OCMC_SRAM_BASE, AM335X_OCMC_SRAM_SIZE, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to map SRAM: %s\n", uc_strerror(err));
        return -1;
    }

    /* Map L4_WKUP MMIO region (covers CM_PER, GPIO0, UART0, I2C0, Control Module, etc.)
     * 0x44C00000 - 0x44FFFFFF (4MB) */
    err = uc_mem_map(cpu->uc, 0x44C00000, 0x400000, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to map L4_WKUP: %s\n", uc_strerror(err));
        return -1;
    }

    /* Map L4_PER MMIO region
     * 0x48000000 - 0x48FFFFFF (16MB) */
    err = uc_mem_map(cpu->uc, 0x48000000, 0x1000000, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to map L4_PER: %s\n", uc_strerror(err));
        return -1;
    }

    /* Map L4_PER extended (GPIO2/3, UART3-5, SPI1)
     * 0x481A0000 - 0x481BFFFF */
    /* Already covered by L4_PER 16MB mapping above */

    /* Map KO/Mock kernel region */
    err = uc_mem_map(cpu->uc, KO_LOAD_BASE, KO_LOAD_MAX - KO_LOAD_BASE, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to map KO region: %s\n", uc_strerror(err));
        return -1;
    }
    err = uc_mem_map(cpu->uc, MOCK_KERNEL_BASE, MOCK_KERNEL_MAX - MOCK_KERNEL_BASE, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to map mock kernel region: %s\n", uc_strerror(err));
        return -1;
    }

    /* Add MMIO hooks for L4_WKUP region */
    err = uc_hook_add(cpu->uc, &cpu->hook_mmio_r, UC_HOOK_MEM_READ,
                      uc_mmio_read_hook, mem,
                      0x44C00000, 0x44FFFFFF);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to add L4_WKUP read hook: %s\n", uc_strerror(err));
        return -1;
    }

    err = uc_hook_add(cpu->uc, &cpu->hook_mmio_w, UC_HOOK_MEM_WRITE,
                      uc_mmio_write_hook, mem,
                      0x44C00000, 0x44FFFFFF);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to add L4_WKUP write hook: %s\n", uc_strerror(err));
        return -1;
    }

    /* Add MMIO hooks for L4_PER region */
    uc_hook h_per_r, h_per_w;
    err = uc_hook_add(cpu->uc, &h_per_r, UC_HOOK_MEM_READ,
                      uc_mmio_read_hook, mem,
                      0x48000000, 0x48FFFFFF);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to add L4_PER read hook: %s\n", uc_strerror(err));
        return -1;
    }

    err = uc_hook_add(cpu->uc, &h_per_w, UC_HOOK_MEM_WRITE,
                      uc_mmio_write_hook, mem,
                      0x48000000, 0x48FFFFFF);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[MEM] Failed to add L4_PER write hook: %s\n", uc_strerror(err));
        return -1;
    }

    printf("[MEM] Memory subsystem initialized\n");
    return 0;
}
