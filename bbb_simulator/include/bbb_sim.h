/*
 * bbb_sim.h - Main simulator context and lifecycle
 */
#ifndef BBB_SIM_H
#define BBB_SIM_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/* Forward declarations */
typedef struct cpu_emu cpu_emu_t;
typedef struct mem_subsys mem_subsys_t;
typedef struct gpio_periph gpio_periph_t;
typedef struct prcm_periph prcm_periph_t;
typedef struct control_periph control_periph_t;
typedef struct uart_periph uart_periph_t;
typedef struct i2c_periph i2c_periph_t;
typedef struct spi_periph spi_periph_t;
typedef struct timer_periph timer_periph_t;
typedef struct adc_periph adc_periph_t;
typedef struct intc_periph intc_periph_t;
typedef struct bbb_gui bbb_gui_t;

typedef enum {
    SIM_STATE_IDLE,
    SIM_STATE_RUNNING,
    SIM_STATE_PAUSED,
    SIM_STATE_STOPPED,
    SIM_STATE_ERROR
} sim_state_t;

typedef struct bbb_sim {
    /* State */
    sim_state_t state;
    bool firmware_loaded;
    bool ko_loaded;
    char firmware_path[512];

    /* CPU Emulation */
    cpu_emu_t *cpu;

    /* Memory subsystem */
    mem_subsys_t *mem;

    /* Peripherals */
    gpio_periph_t *gpio[4];       /* GPIO0-3 */
    prcm_periph_t *prcm;
    control_periph_t *ctrl;
    uart_periph_t *uart[6];       /* UART0-5 */
    i2c_periph_t *i2c[3];        /* I2C0-2 */
    spi_periph_t *spi[2];        /* SPI0-1 */
    timer_periph_t *timer[8];    /* DMTIMER0-7 */
    adc_periph_t *adc;
    intc_periph_t *intc;

    /* GUI */
    bbb_gui_t *gui;

    /* Synchronization */
    pthread_mutex_t lock;

    /* Statistics */
    uint64_t instr_count;
    uint64_t cycle_count;

    /* Syscall emulation state */
    int devmem_fd;          /* Fake fd for /dev/mem */
    uint32_t brk_current;   /* Current brk address for heap */
} bbb_sim_t;

/* Lifecycle */
bbb_sim_t *bbb_sim_create(void);
void bbb_sim_destroy(bbb_sim_t *sim);
int bbb_sim_init(bbb_sim_t *sim);
void bbb_sim_reset(bbb_sim_t *sim);

/* Control */
int bbb_sim_load_firmware(bbb_sim_t *sim, const char *elf_path);
int bbb_sim_load_ko(bbb_sim_t *sim, const char *ko_path);
int bbb_sim_run(bbb_sim_t *sim);
void bbb_sim_pause(bbb_sim_t *sim);
void bbb_sim_step(bbb_sim_t *sim);
void bbb_sim_stop(bbb_sim_t *sim);

/* State queries */
sim_state_t bbb_sim_get_state(bbb_sim_t *sim);
uint32_t bbb_sim_get_pc(bbb_sim_t *sim);
uint64_t bbb_sim_get_instr_count(bbb_sim_t *sim);

/* Peripheral clock status */
bool bbb_sim_is_clock_enabled(bbb_sim_t *sim, uint32_t periph_base);

/* Console output callback (for GUI) */
typedef void (*console_output_cb)(const char *text, int len, void *user_data);
void bbb_sim_set_console_callback(bbb_sim_t *sim, console_output_cb cb, void *user_data);

#endif /* BBB_SIM_H */
