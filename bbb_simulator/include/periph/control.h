/*
 * control.h - Control Module (pinmux) model
 */
#ifndef PERIPH_CONTROL_H
#define PERIPH_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;

typedef struct control_periph {
    uint32_t regs[0x2000 / 4]; /* Control module register space (up to 128KB, we simulate 8KB) */
    bbb_sim_t *sim;
} control_periph_t;

/* Lifecycle */
control_periph_t *control_create(bbb_sim_t *sim);
void control_destroy(control_periph_t *ctrl);
void control_reset(control_periph_t *ctrl);

/* MMIO callbacks */
uint32_t control_read(void *opaque, uint32_t addr, uint32_t offset);
void control_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

/* Query pin configuration */
uint32_t control_get_pinmux(control_periph_t *ctrl, uint32_t offset);
int control_get_pin_mode(control_periph_t *ctrl, uint32_t offset);
bool control_is_gpio_mode(control_periph_t *ctrl, uint32_t offset);

#endif /* PERIPH_CONTROL_H */
