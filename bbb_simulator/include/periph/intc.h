/*
 * intc.h - Interrupt Controller model
 */
#ifndef PERIPH_INTC_H
#define PERIPH_INTC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;

typedef struct intc_periph {
    uint32_t base_addr;
    uint32_t regs[0x400 / 4]; /* Register file (includes ILR[0..127]) */

    /* Interrupt state */
    uint32_t itr[4];          /* Input (raw pending) */
    uint32_t mir[4];          /* Mask (1=masked) */
    uint32_t isr[4];          /* Software set */
    uint32_t ilr[128];        /* Priority/routing per line */

    bool irq_pending;
    int current_irq;          /* Currently active IRQ number */

    bbb_sim_t *sim;
} intc_periph_t;

/* Lifecycle */
intc_periph_t *intc_create(bbb_sim_t *sim);
void intc_destroy(intc_periph_t *intc);
void intc_reset(intc_periph_t *intc);

/* MMIO callbacks */
uint32_t intc_read(void *opaque, uint32_t addr, uint32_t offset);
void intc_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

/* Raise/clear an interrupt line */
void intc_set_irq(intc_periph_t *intc, int irq_num, bool active);

/* Check if any unmasked interrupt is pending */
bool intc_has_pending(intc_periph_t *intc);
int intc_get_active_irq(intc_periph_t *intc);

#endif /* PERIPH_INTC_H */
