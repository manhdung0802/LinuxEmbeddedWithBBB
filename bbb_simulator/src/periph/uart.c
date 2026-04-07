/*
 * uart.c - UART peripheral model (16C750 compatible)
 */
#include "periph/uart.h"
#include "bbb_sim.h"
#include "am335x_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uart_periph_t *uart_create(bbb_sim_t *sim, int uart_id, uint32_t base_addr)
{
    uart_periph_t *uart = calloc(1, sizeof(uart_periph_t));
    if (!uart) return NULL;
    uart->sim = sim;
    uart->uart_id = uart_id;
    uart->base_addr = base_addr;
    uart_reset(uart);
    return uart;
}

void uart_destroy(uart_periph_t *uart)
{
    if (uart) free(uart);
}

void uart_reset(uart_periph_t *uart)
{
    memset(uart->regs, 0, sizeof(uart->regs));
    uart->rx_head = uart->rx_tail = uart->rx_count = 0;
    uart->tx_head = uart->tx_tail = uart->tx_count = 0;
    uart->fifo_enabled = false;
    uart->lcr = 0;
    uart->divisor = 0;
    uart->clock_enabled = false;

    /* Default register values */
    uart->regs[UART_LSR / 4] = UART_LSR_THRE | UART_LSR_TEMT;  /* TX empty */
    uart->regs[UART_MDR1 / 4] = UART_MDR1_MODE_DISABLE;
    uart->regs[UART_SYSS / 4] = 1;  /* Module ready */
}

/* Determine operational mode from LCR */
static int uart_get_mode(uart_periph_t *uart)
{
    uint8_t lcr = uart->regs[UART_LCR / 4] & 0xFF;
    if (lcr == UART_LCR_CONFIG_MODE_B) return 2; /* Config mode B */
    if (lcr & 0x80) return 1; /* Config mode A */
    return 0; /* Operational mode */
}

uint32_t uart_read(void *opaque, uint32_t addr, uint32_t offset)
{
    uart_periph_t *uart = (uart_periph_t *)opaque;
    int mode = uart_get_mode(uart);

    switch (offset) {
    case 0x00: /* RHR / DLL */
        if (mode == 1) {
            /* Config mode A: DLL */
            return uart->divisor & 0xFF;
        }
        /* Operational: RHR - read from RX FIFO */
        if (uart->rx_count > 0) {
            uint8_t ch = uart->rx_fifo[uart->rx_tail];
            uart->rx_tail = (uart->rx_tail + 1) % UART_FIFO_SIZE;
            uart->rx_count--;
            if (uart->rx_count == 0) {
                uart->regs[UART_LSR / 4] &= ~UART_LSR_DR;
            }
            return ch;
        }
        return 0;

    case 0x04: /* IER / DLH */
        if (mode == 1) {
            return (uart->divisor >> 8) & 0xFF;
        }
        return uart->regs[UART_IER / 4];

    case 0x08: /* IIR / FCR / EFR */
        if (mode == 2) {
            return uart->regs[UART_EFR / 4]; /* EFR in config mode B */
        }
        /* IIR: Determine interrupt type */
        return 0x01; /* No interrupt pending */

    case 0x0C: /* LCR */
        return uart->regs[UART_LCR / 4];

    case 0x10: /* MCR */
        return uart->regs[UART_MCR / 4];

    case 0x14: /* LSR */
        return uart->regs[UART_LSR / 4];

    case 0x18: /* MSR */
        return uart->regs[UART_MSR / 4];

    case 0x1C: /* SPR */
        return uart->regs[UART_SPR / 4];

    case UART_MDR1:
        return uart->regs[UART_MDR1 / 4];

    case UART_SYSC:
        return uart->regs[UART_SYSC / 4];

    case UART_SYSS:
        return 1; /* Reset done */

    default:
        if (offset / 4 < sizeof(uart->regs) / 4)
            return uart->regs[offset / 4];
        return 0;
    }
}

void uart_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value)
{
    uart_periph_t *uart = (uart_periph_t *)opaque;
    int mode = uart_get_mode(uart);

    switch (offset) {
    case 0x00: /* THR / DLL */
        if (mode == 1) {
            /* Config mode A: DLL */
            uart->divisor = (uart->divisor & 0xFF00) | (value & 0xFF);
            return;
        }
        /* Operational: THR - write to TX */
        {
            char ch = (char)(value & 0xFF);
            /* Output to console */
            if (uart->on_tx) {
                uart->on_tx(uart->uart_id, ch, uart->cb_user_data);
            }
            /* Keep THR always empty (instant transmit) */
            uart->regs[UART_LSR / 4] |= UART_LSR_THRE | UART_LSR_TEMT;
        }
        break;

    case 0x04: /* IER / DLH */
        if (mode == 1) {
            uart->divisor = (uart->divisor & 0x00FF) | ((value & 0xFF) << 8);
            return;
        }
        uart->regs[UART_IER / 4] = value;
        break;

    case 0x08: /* FCR / EFR */
        if (mode == 2) {
            uart->regs[UART_EFR / 4] = value;
            return;
        }
        /* FCR */
        uart->fifo_enabled = (value & UART_FCR_FIFO_EN) != 0;
        if (value & UART_FCR_RX_CLR) {
            uart->rx_head = uart->rx_tail = uart->rx_count = 0;
            uart->regs[UART_LSR / 4] &= ~UART_LSR_DR;
        }
        if (value & UART_FCR_TX_CLR) {
            uart->tx_head = uart->tx_tail = uart->tx_count = 0;
            uart->regs[UART_LSR / 4] |= UART_LSR_THRE | UART_LSR_TEMT;
        }
        break;

    case 0x0C: /* LCR */
        uart->regs[UART_LCR / 4] = value;
        break;

    case 0x10: /* MCR */
        uart->regs[UART_MCR / 4] = value;
        break;

    case UART_MDR1:
        uart->regs[UART_MDR1 / 4] = value;
        if (value == UART_MDR1_MODE_UART16X) {
            printf("[UART%d] Enabled (16x mode, divisor=%d)\n",
                   uart->uart_id, uart->divisor);
        }
        break;

    case UART_SCR:
        uart->regs[UART_SCR / 4] = value;
        break;

    case UART_SYSC:
        uart->regs[UART_SYSC / 4] = value;
        if (value & 0x2) { /* Soft reset */
            uart_reset(uart);
        }
        break;

    default:
        if (offset / 4 < sizeof(uart->regs) / 4)
            uart->regs[offset / 4] = value;
        break;
    }
}

void uart_receive_char(uart_periph_t *uart, uint8_t ch)
{
    if (uart->rx_count >= UART_FIFO_SIZE) {
        uart->regs[UART_LSR / 4] |= UART_LSR_OE; /* Overrun */
        return;
    }
    uart->rx_fifo[uart->rx_head] = ch;
    uart->rx_head = (uart->rx_head + 1) % UART_FIFO_SIZE;
    uart->rx_count++;
    uart->regs[UART_LSR / 4] |= UART_LSR_DR;
}

void uart_receive_string(uart_periph_t *uart, const char *str)
{
    while (*str) {
        uart_receive_char(uart, (uint8_t)*str++);
    }
}

void uart_set_tx_callback(uart_periph_t *uart, uart_tx_cb cb, void *user_data)
{
    uart->on_tx = cb;
    uart->cb_user_data = user_data;
}
