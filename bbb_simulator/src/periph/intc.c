/*
 * intc.c - AM335x Interrupt Controller (INTC) model
 *
 * 128 interrupt lines organized in 4 banks of 32.
 * Supports MIR masking, software set/clear (ISR), priority routing (ILR),
 * SIR_IRQ (sorted priority), and CONTROL (IRQ ack/new-IRQ).
 */
#include "periph/intc.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

intc_periph_t *intc_create(bbb_sim_t *sim)
{
    intc_periph_t *intc = calloc(1, sizeof(intc_periph_t));
    if (!intc) return NULL;
    intc->sim = sim;
    intc->base_addr = AM335X_INTC_BASE;
    intc_reset(intc);
    return intc;
}

void intc_destroy(intc_periph_t *intc)
{
    if (intc) free(intc);
}

void intc_reset(intc_periph_t *intc)
{
    memset(intc->regs, 0, sizeof(intc->regs));
    memset(intc->itr, 0, sizeof(intc->itr));
    memset(intc->isr, 0, sizeof(intc->isr));
    memset(intc->ilr, 0, sizeof(intc->ilr));

    /* All interrupts masked by default */
    for (int i = 0; i < INTC_NUM_BANKS; i++)
        intc->mir[i] = 0xFFFFFFFF;

    intc->irq_pending = false;
    intc->current_irq = -1;

    intc->regs[INTC_REVISION / 4] = 0x50000001;
    intc->regs[INTC_SYSSTATUS / 4] = 1; /* Reset done */
}

/* Recalculate pending state and find highest-priority IRQ */
static void intc_update_pending(intc_periph_t *intc)
{
    int best_irq = -1;
    int best_priority = 0x7F; /* Lower number = higher priority */

    for (int bank = 0; bank < INTC_NUM_BANKS; bank++) {
        uint32_t pending = (intc->itr[bank] | intc->isr[bank]) & ~intc->mir[bank];
        intc->regs[INTC_PENDING_IRQ(bank) / 4] = pending;

        /* Scan this bank for highest-priority pending IRQ */
        while (pending) {
            int bit = __builtin_ctz(pending);
            int irq_num = bank * 32 + bit;

            int priority = (intc->ilr[irq_num] >> 2) & 0x3F;
            if (priority < best_priority || best_irq < 0) {
                best_priority = priority;
                best_irq = irq_num;
            }

            pending &= ~(1U << bit);
        }
    }

    if (best_irq >= 0) {
        intc->irq_pending = true;
        intc->current_irq = best_irq;
        /* SIR_IRQ: active IRQ number in bits 6:0, priority in bits 7+ (simplified) */
        intc->regs[INTC_SIR_IRQ / 4] = (uint32_t)best_irq | (best_priority << 7);
    } else {
        intc->irq_pending = false;
        intc->current_irq = -1;
        intc->regs[INTC_SIR_IRQ / 4] = 0;
    }
}

void intc_set_irq(intc_periph_t *intc, int irq_num, bool active)
{
    if (irq_num < 0 || irq_num >= INTC_NUM_IRQS) return;

    int bank = irq_num / 32;
    uint32_t bit = 1U << (irq_num % 32);

    if (active)
        intc->itr[bank] |= bit;
    else
        intc->itr[bank] &= ~bit;

    intc_update_pending(intc);
}

bool intc_has_pending(intc_periph_t *intc)
{
    return intc->irq_pending;
}

int intc_get_active_irq(intc_periph_t *intc)
{
    return intc->current_irq;
}

uint32_t intc_read(void *opaque, uint32_t addr, uint32_t offset)
{
    intc_periph_t *intc = (intc_periph_t *)opaque;

    /* ILR registers: 0x100 + n*4 */
    if (offset >= 0x100 && offset < 0x100 + INTC_NUM_IRQS * 4) {
        int n = (offset - 0x100) / 4;
        return intc->ilr[n];
    }

    /* Bank registers: each bank at 0x080 + bank*0x20 */
    if (offset >= 0x080 && offset < 0x100) {
        int bank = (offset - 0x080) / 0x20;
        int reg_off = (offset - 0x080) % 0x20;
        if (bank < INTC_NUM_BANKS) {
            switch (reg_off) {
            case 0x00: return intc->itr[bank];
            case 0x04: return intc->mir[bank];
            case 0x10: return intc->isr[bank];
            case 0x18: /* PENDING_IRQ */
                intc_update_pending(intc);
                return intc->regs[INTC_PENDING_IRQ(bank) / 4];
            }
        }
    }

    if (offset == INTC_SIR_IRQ) {
        intc_update_pending(intc);
    }

    if (offset / 4 < sizeof(intc->regs) / 4)
        return intc->regs[offset / 4];
    return 0;
}

void intc_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    intc_periph_t *intc = (intc_periph_t *)opaque;

    /* ILR registers */
    if (offset >= 0x100 && offset < 0x100 + INTC_NUM_IRQS * 4) {
        int n = (offset - 0x100) / 4;
        intc->ilr[n] = value;
        intc_update_pending(intc);
        return;
    }

    /* Bank registers */
    if (offset >= 0x080 && offset < 0x100) {
        int bank = (offset - 0x080) / 0x20;
        int reg_off = (offset - 0x080) % 0x20;
        if (bank < INTC_NUM_BANKS) {
            switch (reg_off) {
            case 0x04: /* MIR direct write */
                intc->mir[bank] = value;
                break;
            case 0x08: /* MIR_CLEAR - clear mask bits */
                intc->mir[bank] &= ~value;
                printf("[INTC] MIR_CLEAR bank%d: 0x%08X (MIR now 0x%08X)\n",
                       bank, value, intc->mir[bank]);
                break;
            case 0x0C: /* MIR_SET - set mask bits */
                intc->mir[bank] |= value;
                break;
            case 0x10: /* ISR_SET */
                intc->isr[bank] |= value;
                break;
            case 0x14: /* ISR_CLEAR */
                intc->isr[bank] &= ~value;
                break;
            default:
                break;
            }
            intc_update_pending(intc);
            return;
        }
    }

    switch (offset) {
    case INTC_SYSCONFIG:
        if (value & 0x2) {
            intc_reset(intc);
        }
        intc->regs[offset / 4] = value;
        break;

    case INTC_CONTROL:
        /* Writing NEWIRQAGR (bit 0) acks current IRQ and starts sorting */
        if (value & 0x1) {
            /* Clear the active IRQ's input */
            if (intc->current_irq >= 0) {
                int bank = intc->current_irq / 32;
                uint32_t bit = 1U << (intc->current_irq % 32);
                intc->itr[bank] &= ~bit;
                intc->isr[bank] &= ~bit;
            }
            intc_update_pending(intc);
        }
        break;

    case INTC_THRESHOLD:
        intc->regs[offset / 4] = value;
        break;

    default:
        if (offset / 4 < sizeof(intc->regs) / 4)
            intc->regs[offset / 4] = value;
        break;
    }
}
