/*
 * adc.c - ADC/Touchscreen Controller model
 *
 * Step-based ADC: firmware configures step registers (channel select, mode),
 * enables steps in STEPENABLE, and reads results from FIFO0/FIFO1.
 * Channel values come from GUI sliders (adc_set_channel_value).
 */
#include "periph/adc.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

adc_periph_t *adc_create(bbb_sim_t *sim)
{
    adc_periph_t *adc = calloc(1, sizeof(adc_periph_t));
    if (!adc) return NULL;
    adc->sim = sim;
    adc->base_addr = AM335X_ADC_TSC_BASE;
    adc_reset(adc);
    return adc;
}

void adc_destroy(adc_periph_t *adc)
{
    if (adc) free(adc);
}

void adc_reset(adc_periph_t *adc)
{
    memset(adc->regs, 0, sizeof(adc->regs));
    memset(adc->fifo0, 0, sizeof(adc->fifo0));
    memset(adc->fifo1, 0, sizeof(adc->fifo1));
    adc->fifo0_count = 0;
    adc->fifo1_count = 0;
    adc->clock_enabled = false;

    adc->regs[ADC_REVISION / 4] = 0x40000001;

    /* Default channel values: mid-range */
    for (int i = 0; i < ADC_MAX_CHANNELS; i++)
        adc->channel_values[i] = 2048;
}

void adc_set_channel_value(adc_periph_t *adc, int channel, uint32_t value)
{
    if (channel >= 0 && channel < ADC_MAX_CHANNELS)
        adc->channel_values[channel] = value & ADC_MAX_VALUE;
}

/* Execute enabled steps and fill FIFOs */
static void adc_do_conversion(adc_periph_t *adc)
{
    uint32_t step_en = adc->regs[ADC_STEPENABLE / 4];

    adc->fifo0_count = 0;
    adc->fifo1_count = 0;

    for (int step = 0; step < ADC_MAX_STEPS; step++) {
        if (!(step_en & (1 << (step + 1)))) /* bit 0 is TS charge step */
            continue;

        uint32_t cfg = adc->regs[ADC_STEPCONFIG(step) / 4];
        int channel = (cfg >> 19) & 0xF; /* SEL_INP_SWC_3_0 */
        if (channel >= ADC_MAX_CHANNELS)
            channel = 0;

        int fifo_sel = (cfg >> 26) & 0x1; /* FIFO_select */

        uint32_t data = adc->channel_values[channel];
        /* Tag with channel ID (bits 19:16) */
        data |= (channel << 16);

        if (fifo_sel == 0 && adc->fifo0_count < ADC_FIFO_SIZE) {
            adc->fifo0[adc->fifo0_count++] = data;
        } else if (fifo_sel == 1 && adc->fifo1_count < ADC_FIFO_SIZE) {
            adc->fifo1[adc->fifo1_count++] = data;
        }
    }

    /* Set end-of-sequence IRQ */
    if (adc->fifo0_count > 0 || adc->fifo1_count > 0) {
        adc->regs[ADC_IRQSTATUS_RAW / 4] |= (1 << 1); /* END_OF_SEQUENCE */
        adc->regs[ADC_IRQSTATUS / 4] |= (1 << 1);
    }

    printf("[ADC] Conversion done: FIFO0=%d, FIFO1=%d samples\n",
           adc->fifo0_count, adc->fifo1_count);
}

uint32_t adc_read(void *opaque, uint32_t addr, uint32_t offset)
{
    adc_periph_t *adc = (adc_periph_t *)opaque;

    switch (offset) {
    case ADC_FIFO0COUNT:
        return adc->fifo0_count;

    case ADC_FIFO1COUNT:
        return adc->fifo1_count;

    case ADC_FIFO0DATA:
        if (adc->fifo0_count > 0) {
            uint32_t val = adc->fifo0[0];
            /* Shift FIFO up */
            for (int i = 1; i < adc->fifo0_count; i++)
                adc->fifo0[i - 1] = adc->fifo0[i];
            adc->fifo0_count--;
            return val;
        }
        return 0;

    case ADC_FIFO1DATA:
        if (adc->fifo1_count > 0) {
            uint32_t val = adc->fifo1[0];
            for (int i = 1; i < adc->fifo1_count; i++)
                adc->fifo1[i - 1] = adc->fifo1[i];
            adc->fifo1_count--;
            return val;
        }
        return 0;

    default:
        if (offset / 4 < sizeof(adc->regs) / 4)
            return adc->regs[offset / 4];
        return 0;
    }
}

void adc_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    adc_periph_t *adc = (adc_periph_t *)opaque;

    switch (offset) {
    case ADC_SYSCONFIG:
        if (value & 0x2) { /* Soft reset */
            bool clk = adc->clock_enabled;
            adc_reset(adc);
            adc->clock_enabled = clk;
        }
        adc->regs[offset / 4] = value;
        break;

    case ADC_IRQSTATUS:
        /* W1C */
        adc->regs[ADC_IRQSTATUS / 4] &= ~value;
        adc->regs[ADC_IRQSTATUS_RAW / 4] &= ~value;
        break;

    case ADC_IRQENABLE_SET:
        adc->regs[ADC_IRQENABLE_SET / 4] |= value;
        break;

    case ADC_IRQENABLE_CLR:
        adc->regs[ADC_IRQENABLE_SET / 4] &= ~value;
        break;

    case ADC_CTRL:
        adc->regs[offset / 4] = value;
        if (value & ADC_CTRL_ENABLE) {
            printf("[ADC] Enabled\n");
        }
        break;

    case ADC_STEPENABLE:
        adc->regs[offset / 4] = value;
        /* Trigger conversion when steps are enabled and ADC is on */
        if ((adc->regs[ADC_CTRL / 4] & ADC_CTRL_ENABLE) && value)
            adc_do_conversion(adc);
        break;

    default:
        if (offset / 4 < sizeof(adc->regs) / 4)
            adc->regs[offset / 4] = value;
        break;
    }
}
