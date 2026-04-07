/*
 * spi.h - McSPI peripheral model
 */
#ifndef PERIPH_SPI_H
#define PERIPH_SPI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;

typedef struct spi_periph {
    int spi_id;                 /* 0-1 */
    uint32_t base_addr;
    uint32_t regs[0x200 / 4];  /* Register file */
    bool clock_enabled;

    /* Per-channel state */
    uint32_t tx_data[4];       /* Last TX data per channel */
    uint32_t rx_data[4];       /* Current RX data per channel */
    bool channel_enabled[4];

    bbb_sim_t *sim;
} spi_periph_t;

/* Lifecycle */
spi_periph_t *spi_create(bbb_sim_t *sim, int spi_id, uint32_t base_addr);
void spi_destroy(spi_periph_t *spi);
void spi_reset(spi_periph_t *spi);

/* MMIO callbacks */
uint32_t spi_read(void *opaque, uint32_t addr, uint32_t offset);
void spi_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

#endif /* PERIPH_SPI_H */
