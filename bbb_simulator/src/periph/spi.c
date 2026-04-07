/*
 * spi.c - McSPI peripheral model
 */
#include "periph/spi.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

spi_periph_t *spi_create(bbb_sim_t *sim, int spi_id, uint32_t base_addr)
{
    spi_periph_t *spi = calloc(1, sizeof(spi_periph_t));
    if (!spi) return NULL;
    spi->sim = sim;
    spi->spi_id = spi_id;
    spi->base_addr = base_addr;
    spi_reset(spi);
    return spi;
}

void spi_destroy(spi_periph_t *spi)
{
    if (spi) free(spi);
}

void spi_reset(spi_periph_t *spi)
{
    memset(spi->regs, 0, sizeof(spi->regs));
    memset(spi->tx_data, 0, sizeof(spi->tx_data));
    memset(spi->rx_data, 0, sizeof(spi->rx_data));
    memset(spi->channel_enabled, 0, sizeof(spi->channel_enabled));
    spi->clock_enabled = false;

    spi->regs[MCSPI_REVISION / 4] = 0x40300000; /* Revision */
    spi->regs[MCSPI_SYSSTATUS / 4] = 1;         /* Reset done */
}

uint32_t spi_read(void *opaque, uint32_t addr, uint32_t offset)
{
    spi_periph_t *spi = (spi_periph_t *)opaque;

    /* Check for per-channel registers */
    if (offset >= 0x100 && offset < 0x200) {
        int ch = (offset - 0x100) / 0x14;
        int ch_off = (offset - 0x100) % 0x14;

        if (ch < MCSPI_MAX_CHANNELS) {
            switch (ch_off) {
            case 0x04: /* CH_STAT */
                /* RXS=1 (data available), TXS=1 (TX ready), EOT=1 */
                return MCSPI_STAT_RXS | MCSPI_STAT_TXS | MCSPI_STAT_EOT;

            case 0x10: /* CH_RX */
                return spi->rx_data[ch];

            default:
                break;
            }
        }
    }

    if (offset / 4 < sizeof(spi->regs) / 4)
        return spi->regs[offset / 4];
    return 0;
}

void spi_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    spi_periph_t *spi = (spi_periph_t *)opaque;

    /* Check for per-channel registers */
    if (offset >= 0x100 && offset < 0x200) {
        int ch = (offset - 0x100) / 0x14;
        int ch_off = (offset - 0x100) % 0x14;

        if (ch < MCSPI_MAX_CHANNELS) {
            switch (ch_off) {
            case 0x08: /* CH_CTRL */
                spi->channel_enabled[ch] = (value & 1) != 0;
                printf("[SPI%d] Channel %d %s\n", spi->spi_id, ch,
                       spi->channel_enabled[ch] ? "enabled" : "disabled");
                break;

            case 0x0C: /* CH_TX */
                spi->tx_data[ch] = value;
                /* Loopback: TX data appears in RX */
                spi->rx_data[ch] = value;
                printf("[SPI%d] CH%d TX: 0x%08X\n", spi->spi_id, ch, value);
                break;

            default:
                break;
            }
        }
    }

    if (offset / 4 < sizeof(spi->regs) / 4)
        spi->regs[offset / 4] = value;

    /* Handle SYSCONFIG soft reset */
    if (offset == MCSPI_SYSCONFIG && (value & 0x2)) {
        spi_reset(spi);
    }
}
