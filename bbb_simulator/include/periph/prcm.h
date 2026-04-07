/*
 * prcm.h - Power, Reset and Clock Management model
 */
#ifndef PERIPH_PRCM_H
#define PERIPH_PRCM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;

typedef struct prcm_periph {
    uint32_t cm_per_regs[0x200 / 4];   /* CM_PER registers */
    uint32_t cm_wkup_regs[0x100 / 4];  /* CM_WKUP registers */
    bbb_sim_t *sim;
} prcm_periph_t;

/* Lifecycle */
prcm_periph_t *prcm_create(bbb_sim_t *sim);
void prcm_destroy(prcm_periph_t *prcm);
void prcm_reset(prcm_periph_t *prcm);

/* MMIO callbacks */
uint32_t prcm_cm_per_read(void *opaque, uint32_t addr, uint32_t offset);
void prcm_cm_per_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);
uint32_t prcm_cm_wkup_read(void *opaque, uint32_t addr, uint32_t offset);
void prcm_cm_wkup_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

/* Query clock status per peripheral */
bool prcm_is_module_enabled(prcm_periph_t *prcm, uint32_t periph_base_addr);

#endif /* PERIPH_PRCM_H */
