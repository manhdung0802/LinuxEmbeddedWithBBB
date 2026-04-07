/*
 * timer.h - DMTimer peripheral model
 */
#ifndef PERIPH_TIMER_H
#define PERIPH_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef struct bbb_sim bbb_sim_t;

typedef struct timer_periph {
    int timer_id;               /* 0-7 */
    uint32_t base_addr;
    uint32_t regs[0x80 / 4];   /* Register file */
    bool clock_enabled;

    /* Timer state */
    bool running;
    struct timespec start_time; /* Host time when timer started */
    uint32_t start_count;       /* Counter value when timer started */
    uint32_t prescale_ratio;    /* Prescaler value */

    bbb_sim_t *sim;
} timer_periph_t;

/* Lifecycle */
timer_periph_t *timer_create(bbb_sim_t *sim, int timer_id, uint32_t base_addr);
void timer_destroy(timer_periph_t *timer);
void timer_reset(timer_periph_t *timer);

/* MMIO callbacks */
uint32_t timer_read(void *opaque, uint32_t addr, uint32_t offset);
void timer_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

/* Update timer (called periodically or on read) */
void timer_update(timer_periph_t *timer);

#endif /* PERIPH_TIMER_H */
