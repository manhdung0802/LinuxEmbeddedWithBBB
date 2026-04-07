/*
 * control.c - Control Module (pinmux) model
 */
#include "periph/control.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

control_periph_t *control_create(bbb_sim_t *sim)
{
    control_periph_t *ctrl = calloc(1, sizeof(control_periph_t));
    if (!ctrl) return NULL;
    ctrl->sim = sim;
    control_reset(ctrl);
    return ctrl;
}

void control_destroy(control_periph_t *ctrl)
{
    if (ctrl) free(ctrl);
}

void control_reset(control_periph_t *ctrl)
{
    memset(ctrl->regs, 0, sizeof(ctrl->regs));
    /* Device ID */
    ctrl->regs[CTRL_DEVICE_ID / 4] = CTRL_DEVICE_ID_VALUE;
}

uint32_t control_read(void *opaque, uint32_t addr, uint32_t offset)
{
    control_periph_t *ctrl = (control_periph_t *)opaque;

    if (offset / 4 < sizeof(ctrl->regs) / 4) {
        return ctrl->regs[offset / 4];
    }
    return 0;
}

void control_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    control_periph_t *ctrl = (control_periph_t *)opaque;

    if (offset == CTRL_DEVICE_ID) {
        return; /* Read-only */
    }

    if (offset / 4 < sizeof(ctrl->regs) / 4) {
        ctrl->regs[offset / 4] = value;

        /* Log pinmux changes */
        if (offset >= 0x800 && offset < 0x900) {
            printf("[CTRL] Pinmux[0x%03X] = 0x%02X (mode=%d, %s, %s)\n",
                   offset, value,
                   value & CTRL_PINMUX_MODE_MASK,
                   (value & CTRL_PINMUX_PUDEN) ? "pullup/down disabled" : "pull enabled",
                   (value & CTRL_PINMUX_RXACTIVE) ? "rx_active" : "rx_inactive");
        }
    }
}

uint32_t control_get_pinmux(control_periph_t *ctrl, uint32_t offset)
{
    if (offset / 4 < sizeof(ctrl->regs) / 4)
        return ctrl->regs[offset / 4];
    return 0;
}

int control_get_pin_mode(control_periph_t *ctrl, uint32_t offset)
{
    return control_get_pinmux(ctrl, offset) & CTRL_PINMUX_MODE_MASK;
}

bool control_is_gpio_mode(control_periph_t *ctrl, uint32_t offset)
{
    return control_get_pin_mode(ctrl, offset) == CTRL_PINMUX_MODE7_GPIO;
}
