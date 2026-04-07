/*
 * i2c.c - I2C peripheral model
 */
#include "periph/i2c.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

i2c_periph_t *i2c_create(bbb_sim_t *sim, int i2c_id, uint32_t base_addr)
{
    i2c_periph_t *i2c = calloc(1, sizeof(i2c_periph_t));
    if (!i2c) return NULL;
    i2c->sim = sim;
    i2c->i2c_id = i2c_id;
    i2c->base_addr = base_addr;
    i2c_reset(i2c);
    return i2c;
}

void i2c_destroy(i2c_periph_t *i2c)
{
    if (i2c) free(i2c);
}

void i2c_reset(i2c_periph_t *i2c)
{
    memset(i2c->regs, 0, sizeof(i2c->regs));
    i2c->clock_enabled = false;
    i2c->num_slaves = 0;
    i2c->tx_count = i2c->rx_count = 0;
    i2c->tx_idx = i2c->rx_idx = 0;

    /* Default values */
    i2c->regs[I2C_REVNB_LO / 4] = 0x0036;
    i2c->regs[I2C_REVNB_HI / 4] = 0x0050;
    i2c->regs[I2C_SYSS / 4] = 1; /* Reset done */
}

/* Find slave device by address */
static i2c_slave_t *i2c_find_slave(i2c_periph_t *i2c, uint8_t addr)
{
    for (int i = 0; i < i2c->num_slaves; i++) {
        if (i2c->slaves[i].addr == addr)
            return &i2c->slaves[i];
    }
    return NULL;
}

uint32_t i2c_read(void *opaque, uint32_t addr, uint32_t offset)
{
    i2c_periph_t *i2c = (i2c_periph_t *)opaque;

    switch (offset) {
    case I2C_REVNB_LO:  return i2c->regs[I2C_REVNB_LO / 4];
    case I2C_REVNB_HI:  return i2c->regs[I2C_REVNB_HI / 4];
    case I2C_SYSS:       return 1;

    case I2C_IRQSTATUS:
        return i2c->regs[I2C_IRQSTATUS / 4];

    case I2C_DATA:
        if (i2c->rx_idx < i2c->rx_count) {
            return i2c->rx_buf[i2c->rx_idx++];
        }
        return 0;

    case I2C_CON:
        return i2c->regs[I2C_CON / 4];

    case I2C_BUFSTAT: {
        int rx_avail = i2c->rx_count - i2c->rx_idx;
        if (rx_avail < 0) rx_avail = 0;
        return (rx_avail & 0x3F) | ((0 & 0x3F) << 8);
    }

    default:
        if (offset / 4 < sizeof(i2c->regs) / 4)
            return i2c->regs[offset / 4];
        return 0;
    }
}

void i2c_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    i2c_periph_t *i2c = (i2c_periph_t *)opaque;

    switch (offset) {
    case I2C_IRQSTATUS:
        /* Write-1-to-clear */
        i2c->regs[I2C_IRQSTATUS / 4] &= ~value;
        break;

    case I2C_IRQENABLE_SET:
        i2c->regs[I2C_IRQENABLE_SET / 4] |= value;
        break;

    case I2C_IRQENABLE_CLR:
        i2c->regs[I2C_IRQENABLE_SET / 4] &= ~value;
        break;

    case I2C_SA:
        i2c->regs[I2C_SA / 4] = value & 0x3FF;
        break;

    case I2C_CNT:
        i2c->regs[I2C_CNT / 4] = value & 0xFFFF;
        break;

    case I2C_DATA:
        if (i2c->tx_count < (int)sizeof(i2c->tx_buf)) {
            i2c->tx_buf[i2c->tx_count++] = (uint8_t)(value & 0xFF);
        }
        /* Set XRDY */
        i2c->regs[I2C_IRQSTATUS / 4] |= I2C_IRQ_XRDY;
        break;

    case I2C_CON: {
        i2c->regs[I2C_CON / 4] = value;

        if (!(value & I2C_CON_EN)) break;

        /* Start condition */
        if (value & I2C_CON_STT) {
            uint8_t slave_addr = i2c->regs[I2C_SA / 4] & 0x7F;
            i2c_slave_t *slave = i2c_find_slave(i2c, slave_addr);

            i2c->tx_count = 0;
            i2c->tx_idx = 0;
            i2c->rx_count = 0;
            i2c->rx_idx = 0;

            printf("[I2C%d] START addr=0x%02X %s\n",
                   i2c->i2c_id, slave_addr,
                   (value & I2C_CON_TRX) ? "TX" : "RX");

            if (!slave) {
                /* NACK */
                i2c->regs[I2C_IRQSTATUS / 4] |= I2C_IRQ_NACK;
            } else if (value & I2C_CON_TRX) {
                /* Master TX mode - ready to accept data */
                i2c->regs[I2C_IRQSTATUS / 4] |= I2C_IRQ_XRDY;
            } else {
                /* Master RX mode - simulate reading from slave */
                int cnt = i2c->regs[I2C_CNT / 4];
                if (cnt > (int)sizeof(i2c->rx_buf)) cnt = sizeof(i2c->rx_buf);
                for (int i = 0; i < cnt; i++) {
                    int reg = (slave->reg_ptr + i) & 0xFF;
                    i2c->rx_buf[i] = slave->regs[reg];
                }
                i2c->rx_count = cnt;
                i2c->rx_idx = 0;
                i2c->regs[I2C_IRQSTATUS / 4] |= I2C_IRQ_RRDY;
            }
        }

        /* Stop condition */
        if (value & I2C_CON_STP) {
            /* If we were transmitting, apply TX data to slave */
            uint8_t slave_addr = i2c->regs[I2C_SA / 4] & 0x7F;
            i2c_slave_t *slave = i2c_find_slave(i2c, slave_addr);

            if (slave && i2c->tx_count > 0) {
                /* First TX byte is typically the register address */
                slave->reg_ptr = i2c->tx_buf[0];
                for (int i = 1; i < i2c->tx_count; i++) {
                    int reg = (slave->reg_ptr + i - 1) & 0xFF;
                    slave->regs[reg] = i2c->tx_buf[i];
                }
            }

            i2c->regs[I2C_IRQSTATUS / 4] |= I2C_IRQ_ARDY;
            /* Clear BB (bus busy) */
            i2c->regs[I2C_IRQSTATUS / 4] &= ~I2C_IRQ_BB;
            /* Clear STT and STP bits */
            i2c->regs[I2C_CON / 4] &= ~(I2C_CON_STT | I2C_CON_STP);
        }
        break;
    }

    case I2C_PSC:
    case I2C_SCLL:
    case I2C_SCLH:
        i2c->regs[offset / 4] = value;
        break;

    default:
        if (offset / 4 < sizeof(i2c->regs) / 4)
            i2c->regs[offset / 4] = value;
        break;
    }
}

int i2c_add_slave(i2c_periph_t *i2c, uint8_t addr, const char *name)
{
    if (i2c->num_slaves >= I2C_MAX_SLAVES) return -1;

    i2c_slave_t *slave = &i2c->slaves[i2c->num_slaves++];
    memset(slave, 0, sizeof(*slave));
    slave->addr = addr;
    slave->name = name;

    printf("[I2C%d] Registered slave '%s' at addr 0x%02X\n",
           i2c->i2c_id, name, addr);
    return 0;
}
