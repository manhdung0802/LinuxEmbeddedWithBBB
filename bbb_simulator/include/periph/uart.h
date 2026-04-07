/*
 * uart.h - UART peripheral model (16C750 compatible)
 */
#ifndef PERIPH_UART_H
#define PERIPH_UART_H

#include <stdint.h>
#include <stdbool.h>

typedef struct bbb_sim bbb_sim_t;

/* UART TX callback (for GUI console) */
typedef void (*uart_tx_cb)(int uart_id, char ch, void *user_data);

typedef struct uart_periph {
    int uart_id;                  /* 0-5 */
    uint32_t base_addr;
    uint32_t regs[0x80 / 4];     /* Register file */

    /* FIFOs */
    uint8_t rx_fifo[64];
    int rx_head, rx_tail, rx_count;
    uint8_t tx_fifo[64];
    int tx_head, tx_tail, tx_count;
    bool fifo_enabled;

    /* Configuration state */
    uint8_t lcr;                  /* Current LCR value for mode switching */
    uint16_t divisor;             /* Baud rate divisor */
    bool clock_enabled;

    /* Callbacks */
    uart_tx_cb on_tx;
    void *cb_user_data;
    bbb_sim_t *sim;
} uart_periph_t;

/* Lifecycle */
uart_periph_t *uart_create(bbb_sim_t *sim, int uart_id, uint32_t base_addr);
void uart_destroy(uart_periph_t *uart);
void uart_reset(uart_periph_t *uart);

/* MMIO callbacks */
uint32_t uart_read(void *opaque, uint32_t addr, uint32_t offset);
void uart_write(void *opaque, uint32_t addr, uint32_t offset, uint32_t value);

/* External RX (from GUI input) */
void uart_receive_char(uart_periph_t *uart, uint8_t ch);
void uart_receive_string(uart_periph_t *uart, const char *str);

/* Set TX callback */
void uart_set_tx_callback(uart_periph_t *uart, uart_tx_cb cb, void *user_data);

#endif /* PERIPH_UART_H */
