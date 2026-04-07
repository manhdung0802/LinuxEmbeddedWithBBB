/*
 * gpio.h - GPIO peripheral model
 */
#ifndef PERIPH_GPIO_H
#define PERIPH_GPIO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;

/* GPIO state change callback (for GUI updates) */
typedef void (*gpio_change_cb)(int module, uint32_t dataout, uint32_t oe, void *user_data);

typedef struct gpio_periph {
    int module_id;                /* 0-3 */
    uint32_t base_addr;
    uint32_t regs[0x200 / 4];    /* Register file */
    uint32_t input_pins;          /* Simulated external input values */
    bool clock_enabled;
    gpio_change_cb on_change;
    void *cb_user_data;
    bbb_sim_t *sim;
} gpio_periph_t;

/* Lifecycle */
gpio_periph_t *gpio_create(bbb_sim_t *sim, int module_id, uint32_t base_addr);
void gpio_destroy(gpio_periph_t *gpio);
void gpio_reset(gpio_periph_t *gpio);

/* MMIO access (called from mem_subsys dispatch) */
uint32_t gpio_read(void *opaque, uint32_t addr, uint32_t offset);
void gpio_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

/* External input simulation (from GUI clicks) */
void gpio_set_input_pin(gpio_periph_t *gpio, int pin, bool high);
void gpio_toggle_input_pin(gpio_periph_t *gpio, int pin);

/* Query state */
uint32_t gpio_get_dataout(gpio_periph_t *gpio);
uint32_t gpio_get_oe(gpio_periph_t *gpio);
uint32_t gpio_get_datain(gpio_periph_t *gpio);
bool gpio_pin_is_output(gpio_periph_t *gpio, int pin);
bool gpio_pin_is_high(gpio_periph_t *gpio, int pin);

/* Set change callback */
void gpio_set_change_callback(gpio_periph_t *gpio, gpio_change_cb cb, void *user_data);

#endif /* PERIPH_GPIO_H */
