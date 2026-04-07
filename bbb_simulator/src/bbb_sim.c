/*
 * bbb_sim.c - Main simulator context: creates and wires all subsystems
 */
#include "bbb_sim.h"
#include "cpu_emu.h"
#include "mem_subsys.h"
#include "elf_loader.h"
#include "syscall_emu.h"
#include "ko_loader.h"
#include "gui.h"
#include "am335x_map.h"

#include "periph/gpio.h"
#include "periph/prcm.h"
#include "periph/control.h"
#include "periph/uart.h"
#include "periph/i2c.h"
#include "periph/spi.h"
#include "periph/timer.h"
#include "periph/adc.h"
#include "periph/intc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Console output callback for UART TX */
console_output_cb g_console_cb = NULL;
void *g_console_userdata = NULL;

/* UART TX callback -> forward to console */
static void uart_tx_callback(int uart_id, char ch, void *user_data)
{
    (void)uart_id;
    if (g_console_cb) {
        g_console_cb(&ch, 1, g_console_userdata);
    }
    putchar(ch);
    fflush(stdout);
}

void bbb_sim_set_console_callback(bbb_sim_t *sim, console_output_cb cb, void *user_data)
{
    g_console_cb = cb;
    g_console_userdata = user_data;
}

bbb_sim_t *bbb_sim_create(void)
{
    bbb_sim_t *sim = calloc(1, sizeof(bbb_sim_t));
    if (!sim) return NULL;
    sim->state = SIM_STATE_IDLE;
    sim->devmem_fd = -1;
    sim->brk_current = 0x81000000;
    pthread_mutex_init(&sim->lock, NULL);
    return sim;
}

static const uint32_t gpio_bases[AM335X_GPIO_COUNT] = {
    AM335X_GPIO0_BASE, AM335X_GPIO1_BASE, AM335X_GPIO2_BASE, AM335X_GPIO3_BASE
};
static const uint32_t uart_bases[AM335X_UART_COUNT] = {
    AM335X_UART0_BASE, AM335X_UART1_BASE, AM335X_UART2_BASE,
    AM335X_UART3_BASE, AM335X_UART4_BASE, AM335X_UART5_BASE
};
static const uint32_t i2c_bases[AM335X_I2C_COUNT] = {
    AM335X_I2C0_BASE, AM335X_I2C1_BASE, AM335X_I2C2_BASE
};
static const uint32_t spi_bases[AM335X_MCSPI_COUNT] = {
    AM335X_MCSPI0_BASE, AM335X_MCSPI1_BASE
};
static const uint32_t timer_bases[AM335X_TIMER_COUNT] = {
    AM335X_DMTIMER0_BASE, AM335X_DMTIMER1_1MS_BASE,
    AM335X_DMTIMER2_BASE, AM335X_DMTIMER3_BASE,
    AM335X_DMTIMER4_BASE, AM335X_DMTIMER5_BASE,
    AM335X_DMTIMER6_BASE, AM335X_DMTIMER7_BASE
};

int bbb_sim_init(bbb_sim_t *sim)
{
    /* 1. CPU Emulation */
    sim->cpu = cpu_emu_create(sim);
    if (!sim->cpu || cpu_emu_init(sim->cpu) != 0) {
        fprintf(stderr, "[SIM] CPU init failed\n");
        return -1;
    }

    /* 2. Memory Subsystem */
    sim->mem = mem_subsys_create(sim);
    if (!sim->mem || mem_subsys_init(sim->mem, sim->cpu) != 0) {
        fprintf(stderr, "[SIM] Memory subsystem init failed\n");
        return -1;
    }

    /* 3. INTC */
    sim->intc = intc_create(sim);
    mem_subsys_register(sim->mem, AM335X_INTC_BASE, AM335X_INTC_SIZE,
                        intc_read, intc_write, sim->intc, "INTC");

    /* 4. PRCM */
    sim->prcm = prcm_create(sim);
    mem_subsys_register(sim->mem, AM335X_CM_PER_BASE, AM335X_CM_PER_SIZE,
                        prcm_cm_per_read, prcm_cm_per_write, sim->prcm, "CM_PER");
    mem_subsys_register(sim->mem, AM335X_CM_WKUP_BASE, AM335X_CM_WKUP_SIZE,
                        prcm_cm_wkup_read, prcm_cm_wkup_write, sim->prcm, "CM_WKUP");

    /* 5. Control Module */
    sim->ctrl = control_create(sim);
    mem_subsys_register(sim->mem, AM335X_CONTROL_MODULE_BASE, AM335X_CONTROL_MODULE_SIZE,
                        control_read, control_write, sim->ctrl, "CONTROL");

    /* 6. GPIO0-3 */
    for (int i = 0; i < AM335X_GPIO_COUNT; i++) {
        sim->gpio[i] = gpio_create(sim, i, gpio_bases[i]);
        char name[32];
        snprintf(name, sizeof(name), "GPIO%d", i);
        mem_subsys_register(sim->mem, gpio_bases[i], AM335X_GPIO_SIZE,
                            gpio_read, gpio_write, sim->gpio[i], name);
    }

    /* 7. UART0-5 */
    for (int i = 0; i < AM335X_UART_COUNT; i++) {
        sim->uart[i] = uart_create(sim, i, uart_bases[i]);
        if (i == 0) {
            /* Connect UART0 TX to console */
            uart_set_tx_callback(sim->uart[i], uart_tx_callback, sim);
        }
        char name[32];
        snprintf(name, sizeof(name), "UART%d", i);
        mem_subsys_register(sim->mem, uart_bases[i], AM335X_UART_SIZE,
                            uart_read, uart_write, sim->uart[i], name);
    }

    /* 8. I2C0-2 */
    for (int i = 0; i < AM335X_I2C_COUNT; i++) {
        sim->i2c[i] = i2c_create(sim, i, i2c_bases[i]);
        char name[32];
        snprintf(name, sizeof(name), "I2C%d", i);
        mem_subsys_register(sim->mem, i2c_bases[i], AM335X_I2C_SIZE,
                            i2c_read, i2c_write, sim->i2c[i], name);
    }

    /* 9. SPI0-1 */
    for (int i = 0; i < AM335X_MCSPI_COUNT; i++) {
        sim->spi[i] = spi_create(sim, i, spi_bases[i]);
        char name[32];
        snprintf(name, sizeof(name), "SPI%d", i);
        mem_subsys_register(sim->mem, spi_bases[i], AM335X_MCSPI_SIZE,
                            spi_read, spi_write, sim->spi[i], name);
    }

    /* 10. Timers 0-7 */
    for (int i = 0; i < AM335X_TIMER_COUNT; i++) {
        sim->timer[i] = timer_create(sim, i, timer_bases[i]);
        char name[32];
        snprintf(name, sizeof(name), "TIMER%d", i);
        mem_subsys_register(sim->mem, timer_bases[i], AM335X_TIMER_SIZE,
                            timer_read, timer_write, sim->timer[i], name);
    }

    /* 11. ADC/TSC */
    sim->adc = adc_create(sim);
    mem_subsys_register(sim->mem, AM335X_ADC_TSC_BASE, AM335X_ADC_TSC_SIZE,
                        adc_read, adc_write, sim->adc, "ADC_TSC");

    printf("[SIM] Initialization complete: %d MMIO regions registered\n",
           sim->mem->num_regions);
    return 0;
}

void bbb_sim_reset(bbb_sim_t *sim)
{
    pthread_mutex_lock(&sim->lock);

    cpu_emu_reset(sim->cpu);

    for (int i = 0; i < AM335X_GPIO_COUNT; i++)
        if (sim->gpio[i]) gpio_reset(sim->gpio[i]);
    for (int i = 0; i < AM335X_UART_COUNT; i++)
        if (sim->uart[i]) uart_reset(sim->uart[i]);
    for (int i = 0; i < AM335X_I2C_COUNT; i++)
        if (sim->i2c[i]) i2c_reset(sim->i2c[i]);
    for (int i = 0; i < AM335X_MCSPI_COUNT; i++)
        if (sim->spi[i]) spi_reset(sim->spi[i]);
    for (int i = 0; i < AM335X_TIMER_COUNT; i++)
        if (sim->timer[i]) timer_reset(sim->timer[i]);
    if (sim->adc) adc_reset(sim->adc);
    if (sim->intc) intc_reset(sim->intc);
    if (sim->prcm) prcm_reset(sim->prcm);
    if (sim->ctrl) control_reset(sim->ctrl);

    sim->state = SIM_STATE_IDLE;
    sim->firmware_loaded = false;
    sim->ko_loaded = false;
    sim->instr_count = 0;
    sim->brk_current = 0x81000000;
    sim->devmem_fd = -1;

    pthread_mutex_unlock(&sim->lock);
    printf("[SIM] Reset complete\n");
}

void bbb_sim_destroy(bbb_sim_t *sim)
{
    if (!sim) return;

    for (int i = 0; i < AM335X_GPIO_COUNT; i++)
        gpio_destroy(sim->gpio[i]);
    for (int i = 0; i < AM335X_UART_COUNT; i++)
        uart_destroy(sim->uart[i]);
    for (int i = 0; i < AM335X_I2C_COUNT; i++)
        i2c_destroy(sim->i2c[i]);
    for (int i = 0; i < AM335X_MCSPI_COUNT; i++)
        spi_destroy(sim->spi[i]);
    for (int i = 0; i < AM335X_TIMER_COUNT; i++)
        timer_destroy(sim->timer[i]);
    adc_destroy(sim->adc);
    intc_destroy(sim->intc);
    prcm_destroy(sim->prcm);
    control_destroy(sim->ctrl);
    mem_subsys_destroy(sim->mem);
    cpu_emu_destroy(sim->cpu);

    pthread_mutex_destroy(&sim->lock);
    free(sim);
}

int bbb_sim_load_firmware(bbb_sim_t *sim, const char *elf_path)
{
    elf_info_t info;
    memset(&info, 0, sizeof(info));

    int ret = elf_load_exec(sim->cpu, elf_path, &info);
    if (ret != 0) {
        fprintf(stderr, "[SIM] Failed to load firmware: %s\n", elf_path);
        return -1;
    }

    cpu_emu_set_reg(sim->cpu, UC_ARM_REG_PC, info.entry_point);
    cpu_emu_set_reg(sim->cpu, UC_ARM_REG_SP, AM335X_STACK_TOP);

    strncpy(sim->firmware_path, elf_path, sizeof(sim->firmware_path) - 1);
    sim->firmware_loaded = true;
    sim->state = SIM_STATE_IDLE;

    printf("[SIM] Firmware loaded: %s (entry=0x%08X)\n", elf_path, info.entry_point);
    elf_info_free(&info);
    return 0;
}

int bbb_sim_load_ko(bbb_sim_t *sim, const char *ko_path)
{
    return ko_load(sim, ko_path);
}

int bbb_sim_run(bbb_sim_t *sim)
{
    if (!sim->firmware_loaded && !sim->ko_loaded) {
        fprintf(stderr, "[SIM] No firmware loaded\n");
        return -1;
    }

    sim->state = SIM_STATE_RUNNING;
    uint32_t pc = cpu_emu_get_pc(sim->cpu);
    printf("[SIM] Running from PC=0x%08X\n", pc);
    return cpu_emu_start(sim->cpu, pc);
}

void bbb_sim_pause(bbb_sim_t *sim)
{
    cpu_emu_stop(sim->cpu);
    sim->state = SIM_STATE_PAUSED;
    printf("[SIM] Paused at PC=0x%08X\n", cpu_emu_get_pc(sim->cpu));
}

void bbb_sim_step(bbb_sim_t *sim)
{
    cpu_emu_step(sim->cpu);
    printf("[SIM] Step: PC=0x%08X\n", cpu_emu_get_pc(sim->cpu));
}

void bbb_sim_stop(bbb_sim_t *sim)
{
    cpu_emu_stop(sim->cpu);
    sim->state = SIM_STATE_STOPPED;
    printf("[SIM] Stopped\n");
}

sim_state_t bbb_sim_get_state(bbb_sim_t *sim)
{
    return sim->state;
}

uint32_t bbb_sim_get_pc(bbb_sim_t *sim)
{
    return cpu_emu_get_pc(sim->cpu);
}

uint64_t bbb_sim_get_instr_count(bbb_sim_t *sim)
{
    if (sim->cpu)
        return sim->cpu->instr_count;
    return 0;
}

bool bbb_sim_is_clock_enabled(bbb_sim_t *sim, uint32_t periph_base)
{
    /* For now, always return true (simplified) */
    (void)sim;
    (void)periph_base;
    return true;
}
