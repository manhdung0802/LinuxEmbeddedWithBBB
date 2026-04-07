/*
 * i2c.h - I2C peripheral model
 */
#ifndef PERIPH_I2C_H
#define PERIPH_I2C_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;

/* Virtual I2C slave device */
typedef struct i2c_slave {
    uint8_t addr;               /* 7-bit slave address */
    uint8_t regs[256];          /* Slave register file */
    int reg_ptr;                /* Current register pointer */
    const char *name;
} i2c_slave_t;

#define I2C_MAX_SLAVES 8

typedef struct i2c_periph {
    int i2c_id;                 /* 0-2 */
    uint32_t base_addr;
    uint32_t regs[0xE0 / 4];   /* Register file */
    bool clock_enabled;

    /* Virtual slave bus */
    i2c_slave_t slaves[I2C_MAX_SLAVES];
    int num_slaves;

    /* Transfer state */
    uint8_t tx_buf[64];
    uint8_t rx_buf[64];
    int tx_count, rx_count;
    int tx_idx, rx_idx;

    bbb_sim_t *sim;
} i2c_periph_t;

/* Lifecycle */
i2c_periph_t *i2c_create(bbb_sim_t *sim, int i2c_id, uint32_t base_addr);
void i2c_destroy(i2c_periph_t *i2c);
void i2c_reset(i2c_periph_t *i2c);

/* MMIO callbacks */
uint32_t i2c_read(void *opaque, uint32_t addr, uint32_t offset);
void i2c_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

/* Register virtual slave device */
int i2c_add_slave(i2c_periph_t *i2c, uint8_t addr, const char *name);

#endif /* PERIPH_I2C_H */
