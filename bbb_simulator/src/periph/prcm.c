/*
 * prcm.c - Power, Reset and Clock Management model
 *
 * Implements CM_PER and CM_WKUP clock control registers.
 * Tracks module enable/disable status for peripheral gating.
 */
#include "periph/prcm.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include "periph/gpio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

prcm_periph_t *prcm_create(bbb_sim_t *sim)
{
    prcm_periph_t *prcm = calloc(1, sizeof(prcm_periph_t));
    if (!prcm) return NULL;
    prcm->sim = sim;
    prcm_reset(prcm);
    return prcm;
}

void prcm_destroy(prcm_periph_t *prcm)
{
    if (prcm) free(prcm);
}

void prcm_reset(prcm_periph_t *prcm)
{
    memset(prcm->cm_per_regs, 0, sizeof(prcm->cm_per_regs));
    memset(prcm->cm_wkup_regs, 0, sizeof(prcm->cm_wkup_regs));

    /* Default: some modules enabled at reset (L4, L3) */
    prcm->cm_per_regs[CM_PER_L4LS_CLKCTRL / 4] = CLKCTRL_MODULEMODE_ENABLE;
    prcm->cm_per_regs[CM_PER_L3_CLKCTRL / 4] = CLKCTRL_MODULEMODE_ENABLE;
    prcm->cm_per_regs[CM_PER_EMIF_CLKCTRL / 4] = CLKCTRL_MODULEMODE_ENABLE;

    /* Wakeup domain: control module always on */
    prcm->cm_wkup_regs[CM_WKUP_CONTROL_CLKCTRL / 4] =
        CLKCTRL_MODULEMODE_ENABLE | CLKCTRL_IDLEST_FUNCTIONAL;
}

/* Update dependent peripheral clock enable flags */
static void prcm_update_clocks(prcm_periph_t *prcm)
{
    bbb_sim_t *sim = prcm->sim;

    /* GPIO1-3 clocks (CM_PER) */
    if (sim->gpio[1])
        sim->gpio[1]->clock_enabled =
            (prcm->cm_per_regs[CM_PER_GPIO1_CLKCTRL / 4] & CLKCTRL_MODULEMODE_MASK) ==
            CLKCTRL_MODULEMODE_ENABLE;
    if (sim->gpio[2])
        sim->gpio[2]->clock_enabled =
            (prcm->cm_per_regs[CM_PER_GPIO2_CLKCTRL / 4] & CLKCTRL_MODULEMODE_MASK) ==
            CLKCTRL_MODULEMODE_ENABLE;
    if (sim->gpio[3])
        sim->gpio[3]->clock_enabled =
            (prcm->cm_per_regs[CM_PER_GPIO3_CLKCTRL / 4] & CLKCTRL_MODULEMODE_MASK) ==
            CLKCTRL_MODULEMODE_ENABLE;

    /* GPIO0 clock (CM_WKUP) */
    if (sim->gpio[0])
        sim->gpio[0]->clock_enabled =
            (prcm->cm_wkup_regs[CM_WKUP_GPIO0_CLKCTRL / 4] & CLKCTRL_MODULEMODE_MASK) ==
            CLKCTRL_MODULEMODE_ENABLE;
}

uint32_t prcm_cm_per_read(void *opaque, uint32_t addr, uint32_t offset)
{
    prcm_periph_t *prcm = (prcm_periph_t *)opaque;

    if (offset / 4 < sizeof(prcm->cm_per_regs) / 4) {
        uint32_t val = prcm->cm_per_regs[offset / 4];

        /* For CLKCTRL registers: if MODULEMODE is enabled, return IDLEST=functional */
        if ((val & CLKCTRL_MODULEMODE_MASK) == CLKCTRL_MODULEMODE_ENABLE) {
            val = (val & ~CLKCTRL_IDLEST_MASK) | CLKCTRL_IDLEST_FUNCTIONAL;
        } else {
            val = (val & ~CLKCTRL_IDLEST_MASK) | CLKCTRL_IDLEST_DISABLED;
        }
        return val;
    }
    return 0;
}

void prcm_cm_per_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    prcm_periph_t *prcm = (prcm_periph_t *)opaque;

    if (offset / 4 < sizeof(prcm->cm_per_regs) / 4) {
        prcm->cm_per_regs[offset / 4] = value;
        printf("[PRCM] CM_PER[0x%03X] = 0x%08X\n", offset, value);
        prcm_update_clocks(prcm);
    }
}

uint32_t prcm_cm_wkup_read(void *opaque, uint32_t addr, uint32_t offset)
{
    prcm_periph_t *prcm = (prcm_periph_t *)opaque;

    if (offset / 4 < sizeof(prcm->cm_wkup_regs) / 4) {
        uint32_t val = prcm->cm_wkup_regs[offset / 4];
        if ((val & CLKCTRL_MODULEMODE_MASK) == CLKCTRL_MODULEMODE_ENABLE) {
            val = (val & ~CLKCTRL_IDLEST_MASK) | CLKCTRL_IDLEST_FUNCTIONAL;
        }
        return val;
    }
    return 0;
}

void prcm_cm_wkup_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    prcm_periph_t *prcm = (prcm_periph_t *)opaque;

    if (offset / 4 < sizeof(prcm->cm_wkup_regs) / 4) {
        prcm->cm_wkup_regs[offset / 4] = value;
        printf("[PRCM] CM_WKUP[0x%03X] = 0x%08X\n", offset, value);
        prcm_update_clocks(prcm);
    }
}

bool prcm_is_module_enabled(prcm_periph_t *prcm, uint32_t periph_base_addr)
{
    /* Map peripheral base address to its CM_PER/CM_WKUP CLKCTRL offset */
    switch (periph_base_addr) {
    case AM335X_GPIO0_BASE:
        return (prcm->cm_wkup_regs[CM_WKUP_GPIO0_CLKCTRL / 4] &
                CLKCTRL_MODULEMODE_MASK) == CLKCTRL_MODULEMODE_ENABLE;
    case AM335X_GPIO1_BASE:
        return (prcm->cm_per_regs[CM_PER_GPIO1_CLKCTRL / 4] &
                CLKCTRL_MODULEMODE_MASK) == CLKCTRL_MODULEMODE_ENABLE;
    case AM335X_GPIO2_BASE:
        return (prcm->cm_per_regs[CM_PER_GPIO2_CLKCTRL / 4] &
                CLKCTRL_MODULEMODE_MASK) == CLKCTRL_MODULEMODE_ENABLE;
    case AM335X_GPIO3_BASE:
        return (prcm->cm_per_regs[CM_PER_GPIO3_CLKCTRL / 4] &
                CLKCTRL_MODULEMODE_MASK) == CLKCTRL_MODULEMODE_ENABLE;
    case AM335X_UART0_BASE:
        return (prcm->cm_wkup_regs[CM_WKUP_UART0_CLKCTRL / 4] &
                CLKCTRL_MODULEMODE_MASK) == CLKCTRL_MODULEMODE_ENABLE;
    case AM335X_UART1_BASE:
        return (prcm->cm_per_regs[CM_PER_UART1_CLKCTRL / 4] &
                CLKCTRL_MODULEMODE_MASK) == CLKCTRL_MODULEMODE_ENABLE;
    default:
        return true; /* Default to enabled for unmapped peripherals */
    }
}
