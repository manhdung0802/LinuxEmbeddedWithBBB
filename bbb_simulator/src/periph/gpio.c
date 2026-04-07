/*
 * gpio.c - GPIO peripheral model (4 modules, 32 pins each)
 *
 * Implements AM335x GPIO register-level behavior including:
 * - Output enable (direction) control
 * - DATAOUT with SET/CLEAR semantics
 * - DATAIN reflecting output state and external inputs
 * - Interrupt detection (rising/falling edge, level)
 */
#include "periph/gpio.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

gpio_periph_t *gpio_create(bbb_sim_t *sim, int module_id, uint32_t base_addr)
{
    gpio_periph_t *gpio = calloc(1, sizeof(gpio_periph_t));
    if (!gpio) return NULL;
    gpio->sim = sim;
    gpio->module_id = module_id;
    gpio->base_addr = base_addr;
    gpio_reset(gpio);
    return gpio;
}

void gpio_destroy(gpio_periph_t *gpio)
{
    if (gpio) free(gpio);
}

void gpio_reset(gpio_periph_t *gpio)
{
    memset(gpio->regs, 0, sizeof(gpio->regs));
    gpio->input_pins = 0;
    gpio->clock_enabled = false;

    /* Default reset values */
    gpio->regs[GPIO_REVISION / 4] = GPIO_REVISION_VALUE;
    gpio->regs[GPIO_OE / 4] = 0xFFFFFFFF;  /* All pins input by default */
    gpio->regs[GPIO_SYSSTATUS / 4] = 1;     /* Module ready */
}

uint32_t gpio_read(void *opaque, uint32_t addr, uint32_t offset)
{
    gpio_periph_t *gpio = (gpio_periph_t *)opaque;

    if (!gpio->clock_enabled) {
        /* Return 0 if clock not enabled (warning in debug) */
        return 0;
    }

    switch (offset) {
    case GPIO_REVISION:
        return GPIO_REVISION_VALUE;

    case GPIO_SYSSTATUS:
        return 1; /* Reset complete */

    case GPIO_DATAIN: {
        /* For output pins, reflect DATAOUT. For input pins, reflect input_pins */
        uint32_t oe = gpio->regs[GPIO_OE / 4];
        uint32_t dataout = gpio->regs[GPIO_DATAOUT / 4];
        /* OE=0 means output: return DATAOUT; OE=1 means input: return input_pins */
        return (dataout & ~oe) | (gpio->input_pins & oe);
    }

    case GPIO_DATAOUT:
        return gpio->regs[GPIO_DATAOUT / 4];

    case GPIO_OE:
        return gpio->regs[GPIO_OE / 4];

    case GPIO_SETDATAOUT:
    case GPIO_CLEARDATAOUT:
        /* These are write-only; reading returns DATAOUT */
        return gpio->regs[GPIO_DATAOUT / 4];

    default:
        if (offset / 4 < sizeof(gpio->regs) / 4) {
            return gpio->regs[offset / 4];
        }
        return 0;
    }
}

void gpio_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    gpio_periph_t *gpio = (gpio_periph_t *)opaque;

    if (!gpio->clock_enabled) {
        return;
    }

    uint32_t old_dataout = gpio->regs[GPIO_DATAOUT / 4];

    switch (offset) {
    case GPIO_REVISION:
    case GPIO_SYSSTATUS:
    case GPIO_DATAIN:
        /* Read-only registers */
        break;

    case GPIO_SETDATAOUT:
        /* Write-1-to-set: OR value into DATAOUT */
        gpio->regs[GPIO_DATAOUT / 4] |= value;
        printf("[GPIO%d] SETDATAOUT: 0x%08X -> DATAOUT=0x%08X\n",
               gpio->module_id, value, gpio->regs[GPIO_DATAOUT / 4]);
        break;

    case GPIO_CLEARDATAOUT:
        /* Write-1-to-clear: AND-NOT value from DATAOUT */
        gpio->regs[GPIO_DATAOUT / 4] &= ~value;
        printf("[GPIO%d] CLEARDATAOUT: 0x%08X -> DATAOUT=0x%08X\n",
               gpio->module_id, value, gpio->regs[GPIO_DATAOUT / 4]);
        break;

    case GPIO_OE:
        gpio->regs[GPIO_OE / 4] = value;
        printf("[GPIO%d] OE = 0x%08X\n", gpio->module_id, value);
        break;

    case GPIO_DATAOUT:
        gpio->regs[GPIO_DATAOUT / 4] = value;
        break;

    case GPIO_IRQSTATUS_0:
    case GPIO_IRQSTATUS_1:
        /* Write-1-to-clear for interrupt status */
        gpio->regs[offset / 4] &= ~value;
        break;

    case GPIO_IRQSTATUS_SET_0:
    case GPIO_IRQSTATUS_SET_1: {
        /* Set interrupt enable bits */
        int base_off = (offset == GPIO_IRQSTATUS_SET_0) ?
                       GPIO_IRQSTATUS_SET_0 : GPIO_IRQSTATUS_SET_1;
        gpio->regs[base_off / 4] |= value;
        break;
    }

    case GPIO_IRQSTATUS_CLR_0:
    case GPIO_IRQSTATUS_CLR_1: {
        int set_off = (offset == GPIO_IRQSTATUS_CLR_0) ?
                      GPIO_IRQSTATUS_SET_0 : GPIO_IRQSTATUS_SET_1;
        gpio->regs[set_off / 4] &= ~value;
        break;
    }

    default:
        if (offset / 4 < sizeof(gpio->regs) / 4) {
            gpio->regs[offset / 4] = value;
        }
        break;
    }

    /* Notify GUI if DATAOUT changed */
    uint32_t new_dataout = gpio->regs[GPIO_DATAOUT / 4];
    if (new_dataout != old_dataout && gpio->on_change) {
        gpio->on_change(gpio->module_id, new_dataout,
                       gpio->regs[GPIO_OE / 4], gpio->cb_user_data);
    }
}

void gpio_set_input_pin(gpio_periph_t *gpio, int pin, bool high)
{
    if (pin < 0 || pin > 31) return;

    uint32_t old = gpio->input_pins;
    if (high)
        gpio->input_pins |= (1u << pin);
    else
        gpio->input_pins &= ~(1u << pin);

    /* Check for edge-triggered interrupts */
    uint32_t changed = old ^ gpio->input_pins;
    if (changed) {
        uint32_t rising = gpio->input_pins & changed;
        uint32_t falling = ~gpio->input_pins & changed;

        uint32_t irq0 = 0;
        irq0 |= rising & gpio->regs[GPIO_RISINGDETECT / 4];
        irq0 |= falling & gpio->regs[GPIO_FALLINGDETECT / 4];
        irq0 |= gpio->input_pins & gpio->regs[GPIO_LEVELDETECT1 / 4];
        irq0 |= ~gpio->input_pins & gpio->regs[GPIO_LEVELDETECT0 / 4];

        if (irq0) {
            gpio->regs[GPIO_IRQSTATUS_RAW_0 / 4] |= irq0;
            /* If enabled, set status */
            uint32_t enabled = gpio->regs[GPIO_IRQSTATUS_SET_0 / 4];
            gpio->regs[GPIO_IRQSTATUS_0 / 4] |= (irq0 & enabled);
        }
    }
}

void gpio_toggle_input_pin(gpio_periph_t *gpio, int pin)
{
    if (pin < 0 || pin > 31) return;
    bool current = (gpio->input_pins >> pin) & 1;
    gpio_set_input_pin(gpio, pin, !current);
}

uint32_t gpio_get_dataout(gpio_periph_t *gpio) { return gpio->regs[GPIO_DATAOUT / 4]; }
uint32_t gpio_get_oe(gpio_periph_t *gpio) { return gpio->regs[GPIO_OE / 4]; }

uint32_t gpio_get_datain(gpio_periph_t *gpio)
{
    uint32_t oe = gpio->regs[GPIO_OE / 4];
    uint32_t dataout = gpio->regs[GPIO_DATAOUT / 4];
    return (dataout & ~oe) | (gpio->input_pins & oe);
}

bool gpio_pin_is_output(gpio_periph_t *gpio, int pin)
{
    return !((gpio->regs[GPIO_OE / 4] >> pin) & 1);
}

bool gpio_pin_is_high(gpio_periph_t *gpio, int pin)
{
    if (gpio_pin_is_output(gpio, pin))
        return (gpio->regs[GPIO_DATAOUT / 4] >> pin) & 1;
    else
        return (gpio->input_pins >> pin) & 1;
}

void gpio_set_change_callback(gpio_periph_t *gpio, gpio_change_cb cb, void *user_data)
{
    gpio->on_change = cb;
    gpio->cb_user_data = user_data;
}
