/*
 * adc.h - ADC/Touchscreen Controller model
 */
#ifndef PERIPH_ADC_H
#define PERIPH_ADC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;

/* ADC input change callback */
typedef void (*adc_update_cb)(int channel, uint32_t value, void *user_data);

typedef struct adc_periph {
    uint32_t base_addr;
    uint32_t regs[0x300 / 4];  /* Register file */
    bool clock_enabled;

    /* Simulated analog inputs (0-4095 per channel) */
    uint32_t channel_values[8];

    /* FIFOs */
    uint32_t fifo0[64];
    int fifo0_count;
    uint32_t fifo1[64];
    int fifo1_count;

    bbb_sim_t *sim;
} adc_periph_t;

/* Lifecycle */
adc_periph_t *adc_create(bbb_sim_t *sim);
void adc_destroy(adc_periph_t *adc);
void adc_reset(adc_periph_t *adc);

/* MMIO callbacks */
uint32_t adc_read(void *opaque, uint32_t addr, uint32_t offset);
void adc_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

/* Set simulated analog value (from GUI slider) */
void adc_set_channel_value(adc_periph_t *adc, int channel, uint32_t value);

#endif /* PERIPH_ADC_H */
