/*
 * timer.c - DMTimer peripheral model
 *
 * Simulates AM335x DMTimer using host monotonic clock.
 * Timer counter advances based on elapsed host time and prescaler setting.
 */
#include "periph/timer.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Input clock frequency for simulation (24 MHz assumed) */
#define TIMER_INPUT_CLK_HZ  24000000

timer_periph_t *timer_create(bbb_sim_t *sim, int timer_id, uint32_t base_addr)
{
    timer_periph_t *timer = calloc(1, sizeof(timer_periph_t));
    if (!timer) return NULL;
    timer->sim = sim;
    timer->timer_id = timer_id;
    timer->base_addr = base_addr;
    timer_reset(timer);
    return timer;
}

void timer_destroy(timer_periph_t *timer)
{
    if (timer) free(timer);
}

void timer_reset(timer_periph_t *timer)
{
    memset(timer->regs, 0, sizeof(timer->regs));
    timer->running = false;
    timer->prescale_ratio = 1;
    timer->start_count = 0;
    memset(&timer->start_time, 0, sizeof(timer->start_time));
    timer->clock_enabled = false;

    timer->regs[TIMER_TIDR / 4] = 0x40000100; /* Revision */
}

/* Get elapsed ticks since timer started */
static uint64_t timer_elapsed_ticks(timer_periph_t *timer)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    int64_t ns = (now.tv_sec - timer->start_time.tv_sec) * 1000000000LL
               + (now.tv_nsec - timer->start_time.tv_nsec);
    if (ns < 0) ns = 0;

    /* Ticks = ns * (clk_hz / prescale) / 1e9 */
    uint64_t ticks = (uint64_t)ns * (TIMER_INPUT_CLK_HZ / timer->prescale_ratio) / 1000000000ULL;
    return ticks;
}

/* Compute current counter value and check for overflow/match */
void timer_update(timer_periph_t *timer)
{
    if (!timer->running) return;

    uint32_t tclr = timer->regs[TIMER_TCLR / 4];
    uint32_t tldr = timer->regs[TIMER_TLDR / 4];
    uint32_t tmar = timer->regs[TIMER_TMAR / 4];

    uint64_t elapsed = timer_elapsed_ticks(timer);
    uint64_t raw = (uint64_t)timer->start_count + elapsed;

    /* Check overflow */
    if (raw > 0xFFFFFFFF) {
        timer->regs[TIMER_IRQSTATUS_RAW / 4] |= TIMER_IRQ_OVF;
        timer->regs[TIMER_IRQSTATUS / 4] |= TIMER_IRQ_OVF;

        if (tclr & TIMER_TCLR_AR) {
            /* Auto-reload: wrap from TLDR */
            uint32_t range = 0xFFFFFFFF - tldr + 1;
            if (range == 0) range = 1;
            uint32_t excess = (uint32_t)((raw - 0x100000000ULL) % range);
            timer->regs[TIMER_TCRR / 4] = tldr + excess;
        } else {
            /* One-shot: stop */
            timer->regs[TIMER_TCRR / 4] = 0;
            timer->running = false;
        }
    } else {
        timer->regs[TIMER_TCRR / 4] = (uint32_t)raw;
    }

    /* Check match */
    if (tclr & TIMER_TCLR_CE) {
        uint32_t tcrr = timer->regs[TIMER_TCRR / 4];
        if (tcrr >= tmar) {
            timer->regs[TIMER_IRQSTATUS_RAW / 4] |= TIMER_IRQ_MAT;
            timer->regs[TIMER_IRQSTATUS / 4] |= TIMER_IRQ_MAT;
        }
    }
}

uint32_t timer_read(void *opaque, uint32_t addr, uint32_t offset)
{
    timer_periph_t *timer = (timer_periph_t *)opaque;

    /* Update counter before reading */
    if (offset == TIMER_TCRR)
        timer_update(timer);

    if (offset / 4 < sizeof(timer->regs) / 4)
        return timer->regs[offset / 4];
    return 0;
}

void timer_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    timer_periph_t *timer = (timer_periph_t *)opaque;

    switch (offset) {
    case TIMER_TIOCP_CFG:
        if (value & 0x1) {
            /* Soft reset */
            bool clk = timer->clock_enabled;
            timer_reset(timer);
            timer->clock_enabled = clk;
        }
        timer->regs[offset / 4] = value;
        break;

    case TIMER_IRQSTATUS:
        /* W1C - write 1 to clear */
        timer->regs[TIMER_IRQSTATUS / 4] &= ~value;
        timer->regs[TIMER_IRQSTATUS_RAW / 4] &= ~value;
        break;

    case TIMER_IRQENABLE_SET:
        timer->regs[TIMER_IRQENABLE_SET / 4] |= value;
        break;

    case TIMER_IRQENABLE_CLR:
        timer->regs[TIMER_IRQENABLE_SET / 4] &= ~value;
        break;

    case TIMER_TCLR: {
        bool was_running = timer->running;
        bool start = (value & TIMER_TCLR_ST) != 0;

        /* Prescaler: PTV field (bits 4:2), enabled by PRE (bit 5) */
        if (value & (1 << 5)) {
            int ptv = (value >> 2) & 0x7;
            timer->prescale_ratio = 1 << (ptv + 1);
        } else {
            timer->prescale_ratio = 1;
        }

        if (start && !was_running) {
            /* Starting timer */
            timer->running = true;
            timer->start_count = timer->regs[TIMER_TCRR / 4];
            clock_gettime(CLOCK_MONOTONIC, &timer->start_time);
            printf("[TIMER%d] Started, TCRR=0x%08X TLDR=0x%08X prescale=%u\n",
                   timer->timer_id, timer->start_count,
                   timer->regs[TIMER_TLDR / 4], timer->prescale_ratio);
        } else if (!start && was_running) {
            /* Stopping timer */
            timer_update(timer);
            timer->running = false;
            printf("[TIMER%d] Stopped, TCRR=0x%08X\n",
                   timer->timer_id, timer->regs[TIMER_TCRR / 4]);
        }

        timer->regs[offset / 4] = value;
        break;
    }

    case TIMER_TCRR:
        timer->regs[offset / 4] = value;
        if (timer->running) {
            timer->start_count = value;
            clock_gettime(CLOCK_MONOTONIC, &timer->start_time);
        }
        break;

    case TIMER_TLDR:
    case TIMER_TMAR:
        timer->regs[offset / 4] = value;
        break;

    case TIMER_TTGR:
        /* Trigger reload: load TLDR into TCRR */
        timer->regs[TIMER_TCRR / 4] = timer->regs[TIMER_TLDR / 4];
        if (timer->running) {
            timer->start_count = timer->regs[TIMER_TCRR / 4];
            clock_gettime(CLOCK_MONOTONIC, &timer->start_time);
        }
        break;

    default:
        if (offset / 4 < sizeof(timer->regs) / 4)
            timer->regs[offset / 4] = value;
        break;
    }
}
