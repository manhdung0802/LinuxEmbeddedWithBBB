/*
 * am335x_map.h - AM335x SoC Memory Map and Register Definitions
 *
 * Based on AM335x Technical Reference Manual (spruh73q)
 * For use with the BBB Simulator
 */
#ifndef AM335X_MAP_H
#define AM335X_MAP_H

#include <stdint.h>

/* ========================================================================= */
/*                        PERIPHERAL BASE ADDRESSES                          */
/* ========================================================================= */

/* L4_WKUP Domain (Always-On) */
#define AM335X_CM_PER_BASE          0x44E00000
#define AM335X_CM_WKUP_BASE         0x44E00400
#define AM335X_CM_DPLL_BASE         0x44E00500
#define AM335X_CM_MPU_BASE          0x44E00600
#define AM335X_DMTIMER0_BASE        0x44E05000
#define AM335X_GPIO0_BASE           0x44E07000
#define AM335X_UART0_BASE           0x44E09000
#define AM335X_I2C0_BASE            0x44E0B000
#define AM335X_ADC_TSC_BASE         0x44E0D000
#define AM335X_CONTROL_MODULE_BASE  0x44E10000
#define AM335X_DMTIMER1_1MS_BASE    0x44E31000
#define AM335X_WDT1_BASE            0x44E35000
#define AM335X_RTC_BASE             0x44E3E000

/* L4_PER Domain */
#define AM335X_UART1_BASE           0x48022000
#define AM335X_UART2_BASE           0x48024000
#define AM335X_I2C1_BASE            0x4802A000
#define AM335X_MCSPI0_BASE          0x48030000
#define AM335X_DMTIMER2_BASE        0x48040000
#define AM335X_DMTIMER3_BASE        0x48042000
#define AM335X_DMTIMER4_BASE        0x48044000
#define AM335X_DMTIMER5_BASE        0x48046000
#define AM335X_DMTIMER6_BASE        0x48048000
#define AM335X_DMTIMER7_BASE        0x4804A000
#define AM335X_GPIO1_BASE           0x4804C000
#define AM335X_MCSPI1_BASE          0x481A0000
#define AM335X_UART3_BASE           0x481A6000
#define AM335X_UART4_BASE           0x481A8000
#define AM335X_UART5_BASE           0x481AA000
#define AM335X_GPIO2_BASE           0x481AC000
#define AM335X_GPIO3_BASE           0x481AE000
#define AM335X_I2C2_BASE            0x4819C000

/* Interrupt Controller */
#define AM335X_INTC_BASE            0x48200000

/* Peripheral sizes */
#define AM335X_GPIO_SIZE            0x1000
#define AM335X_UART_SIZE            0x1000
#define AM335X_I2C_SIZE             0x1000
#define AM335X_MCSPI_SIZE           0x1000
#define AM335X_TIMER_SIZE           0x1000
#define AM335X_ADC_TSC_SIZE         0x2000
#define AM335X_INTC_SIZE            0x1000
#define AM335X_CM_PER_SIZE          0x0400
#define AM335X_CM_WKUP_SIZE         0x0100
#define AM335X_CONTROL_MODULE_SIZE  0x20000

/* Memory regions */
#define AM335X_BOOT_ROM_BASE        0x40000000
#define AM335X_BOOT_ROM_SIZE        0x20000     /* 128 KB */
#define AM335X_OCMC_SRAM_BASE       0x40300000
#define AM335X_OCMC_SRAM_SIZE       0x10000     /* 64 KB */
#define AM335X_DDR_BASE             0x80000000
#define AM335X_DDR_SIZE             0x10000000  /* 256 MB */
#define AM335X_STACK_TOP            0x8F000000

/* Peripheral count */
#define AM335X_GPIO_COUNT           4
#define AM335X_UART_COUNT           6
#define AM335X_I2C_COUNT            3
#define AM335X_MCSPI_COUNT          2
#define AM335X_TIMER_COUNT          8

/* ========================================================================= */
/*                        GPIO REGISTER OFFSETS                               */
/* ========================================================================= */

#define GPIO_REVISION               0x000
#define GPIO_SYSCONFIG              0x010
#define GPIO_EOI                    0x020
#define GPIO_IRQSTATUS_RAW_0        0x024
#define GPIO_IRQSTATUS_RAW_1        0x028
#define GPIO_IRQSTATUS_0            0x02C
#define GPIO_IRQSTATUS_1            0x030
#define GPIO_IRQSTATUS_SET_0        0x034
#define GPIO_IRQSTATUS_SET_1        0x038
#define GPIO_IRQSTATUS_CLR_0        0x03C
#define GPIO_IRQSTATUS_CLR_1        0x040
#define GPIO_IRQWAKEN_0             0x044
#define GPIO_IRQWAKEN_1             0x048
#define GPIO_SYSSTATUS              0x114
#define GPIO_CTRL                   0x130
#define GPIO_OE                     0x134
#define GPIO_DATAIN                 0x138
#define GPIO_DATAOUT                0x13C
#define GPIO_LEVELDETECT0           0x140
#define GPIO_LEVELDETECT1           0x144
#define GPIO_RISINGDETECT           0x148
#define GPIO_FALLINGDETECT          0x14C
#define GPIO_DEBOUNCENABLE          0x150
#define GPIO_DEBOUNCINGTIME         0x154
#define GPIO_CLEARDATAOUT           0x190
#define GPIO_SETDATAOUT             0x194

#define GPIO_REG_COUNT              ((GPIO_SETDATAOUT / 4) + 1)
#define GPIO_REVISION_VALUE         0x50600801

/* ========================================================================= */
/*                    CM_PER CLOCK MODULE REGISTER OFFSETS                    */
/* ========================================================================= */

#define CM_PER_L4LS_CLKSTCTRL       0x000
#define CM_PER_L3S_CLKSTCTRL        0x004
#define CM_PER_L3_CLKSTCTRL         0x00C
#define CM_PER_CPGMAC0_CLKCTRL      0x014
#define CM_PER_LCDC_CLKCTRL         0x018
#define CM_PER_USB0_CLKCTRL         0x01C
#define CM_PER_TPTC0_CLKCTRL        0x024
#define CM_PER_EMIF_CLKCTRL         0x028
#define CM_PER_OCMCRAM_CLKCTRL      0x02C
#define CM_PER_GPMC_CLKCTRL         0x030
#define CM_PER_MCASP0_CLKCTRL       0x034
#define CM_PER_UART5_CLKCTRL        0x038
#define CM_PER_MMC0_CLKCTRL         0x03C
#define CM_PER_ELM_CLKCTRL          0x040
#define CM_PER_I2C2_CLKCTRL         0x044
#define CM_PER_I2C1_CLKCTRL         0x048
#define CM_PER_SPI0_CLKCTRL         0x04C
#define CM_PER_SPI1_CLKCTRL         0x050
#define CM_PER_L4LS_CLKCTRL         0x060
#define CM_PER_MCASP1_CLKCTRL       0x068
#define CM_PER_UART1_CLKCTRL        0x06C
#define CM_PER_UART2_CLKCTRL        0x070
#define CM_PER_UART3_CLKCTRL        0x074
#define CM_PER_UART4_CLKCTRL        0x078
#define CM_PER_TIMER7_CLKCTRL       0x07C
#define CM_PER_TIMER2_CLKCTRL       0x080
#define CM_PER_TIMER3_CLKCTRL       0x084
#define CM_PER_TIMER4_CLKCTRL       0x088
#define CM_PER_GPIO1_CLKCTRL        0x0AC
#define CM_PER_GPIO2_CLKCTRL        0x0B0
#define CM_PER_GPIO3_CLKCTRL        0x0B4
#define CM_PER_TPCC_CLKCTRL         0x0BC
#define CM_PER_DCAN0_CLKCTRL        0x0C0
#define CM_PER_DCAN1_CLKCTRL        0x0C4
#define CM_PER_EPWMSS1_CLKCTRL      0x0CC
#define CM_PER_EPWMSS0_CLKCTRL      0x0D4
#define CM_PER_EPWMSS2_CLKCTRL      0x0D8
#define CM_PER_L3_INSTR_CLKCTRL     0x0DC
#define CM_PER_L3_CLKCTRL           0x0E0
#define CM_PER_PRU_ICSS_CLKCTRL     0x0E8
#define CM_PER_TIMER5_CLKCTRL       0x0EC
#define CM_PER_TIMER6_CLKCTRL       0x0F0
#define CM_PER_MMC1_CLKCTRL         0x0F4
#define CM_PER_MMC2_CLKCTRL         0x0F8
#define CM_PER_TPTC1_CLKCTRL        0x0FC
#define CM_PER_TPTC2_CLKCTRL        0x100
#define CM_PER_SPINLOCK_CLKCTRL     0x10C
#define CM_PER_MAILBOX0_CLKCTRL     0x110

#define CM_PER_REG_COUNT            0x160  /* total byte size / 4 */

/* CLKCTRL register fields */
#define CLKCTRL_MODULEMODE_MASK     0x3
#define CLKCTRL_MODULEMODE_DISABLED 0x0
#define CLKCTRL_MODULEMODE_ENABLE   0x2
#define CLKCTRL_IDLEST_SHIFT        16
#define CLKCTRL_IDLEST_MASK         (0x3 << 16)
#define CLKCTRL_IDLEST_FUNCTIONAL   (0x0 << 16)
#define CLKCTRL_IDLEST_TRANSITION   (0x1 << 16)
#define CLKCTRL_IDLEST_IDLE         (0x2 << 16)
#define CLKCTRL_IDLEST_DISABLED     (0x3 << 16)

/* ========================================================================= */
/*                    CM_WKUP CLOCK MODULE REGISTER OFFSETS                  */
/* ========================================================================= */

#define CM_WKUP_CLKSTCTRL           0x00
#define CM_WKUP_CONTROL_CLKCTRL     0x04
#define CM_WKUP_GPIO0_CLKCTRL       0x08
#define CM_WKUP_L4WKUP_CLKCTRL      0x0C
#define CM_WKUP_TIMER0_CLKCTRL      0x10
#define CM_WKUP_DEBUGSS_CLKCTRL     0x14
#define CM_WKUP_L3_AON_CLKSTCTRL    0x18
#define CM_WKUP_UART0_CLKCTRL       0xB4
#define CM_WKUP_I2C0_CLKCTRL        0xB8
#define CM_WKUP_ADC_TSC_CLKCTRL     0xBC
#define CM_WKUP_TIMER1_CLKCTRL      0xC4

#define CM_WKUP_REG_COUNT           0x100  /* byte size / 4 */

/* ========================================================================= */
/*                        UART REGISTER OFFSETS                              */
/* ========================================================================= */

/* Operational mode registers */
#define UART_RHR                    0x00  /* Receive Holding Register (read) */
#define UART_THR                    0x00  /* Transmit Holding Register (write) */
#define UART_DLL                    0x00  /* Divisor Latch Low (config mode A) */
#define UART_IER                    0x04  /* Interrupt Enable Register */
#define UART_DLH                    0x04  /* Divisor Latch High (config mode A) */
#define UART_IIR                    0x08  /* Interrupt Identification (read) */
#define UART_FCR                    0x08  /* FIFO Control Register (write) */
#define UART_EFR                    0x08  /* Enhanced Feature Register (config B) */
#define UART_LCR                    0x0C  /* Line Control Register */
#define UART_MCR                    0x10  /* Modem Control Register */
#define UART_LSR                    0x14  /* Line Status Register */
#define UART_MSR                    0x18  /* Modem Status Register */
#define UART_TCR                    0x18  /* Transmission Control (TCR_TLR mode) */
#define UART_SPR                    0x1C  /* Scratch Pad Register */
#define UART_TLR                    0x1C  /* Trigger Level Register */
#define UART_MDR1                   0x20  /* Mode Definition Register 1 */
#define UART_MDR2                   0x24  /* Mode Definition Register 2 */
#define UART_SCR                    0x40  /* Supplementary Control Register */
#define UART_SSR                    0x44  /* Supplementary Status Register */
#define UART_SYSC                   0x54  /* System Configuration */
#define UART_SYSS                   0x58  /* System Status */

#define UART_REG_COUNT              ((UART_SYSS / 4) + 1)

/* LSR bit definitions */
#define UART_LSR_DR                 (1 << 0)  /* Data Ready */
#define UART_LSR_OE                 (1 << 1)  /* Overrun Error */
#define UART_LSR_PE                 (1 << 2)  /* Parity Error */
#define UART_LSR_FE                 (1 << 3)  /* Framing Error */
#define UART_LSR_BI                 (1 << 4)  /* Break Interrupt */
#define UART_LSR_THRE               (1 << 5)  /* THR Empty */
#define UART_LSR_TEMT               (1 << 6)  /* Transmitter Empty */

/* IER bit definitions */
#define UART_IER_RHR                (1 << 0)
#define UART_IER_THR                (1 << 1)
#define UART_IER_LINE_STATUS        (1 << 2)
#define UART_IER_MODEM_STATUS       (1 << 3)

/* FCR bit definitions */
#define UART_FCR_FIFO_EN            (1 << 0)
#define UART_FCR_RX_CLR             (1 << 1)
#define UART_FCR_TX_CLR             (1 << 2)

/* LCR values */
#define UART_LCR_CONFIG_MODE_A      0x80
#define UART_LCR_CONFIG_MODE_B      0xBF

/* MDR1 modes */
#define UART_MDR1_MODE_UART16X      0x00
#define UART_MDR1_MODE_DISABLE      0x07

/* FIFO size */
#define UART_FIFO_SIZE              64

/* ========================================================================= */
/*                        I2C REGISTER OFFSETS                               */
/* ========================================================================= */

#define I2C_REVNB_LO                0x00
#define I2C_REVNB_HI                0x04
#define I2C_SYSC                    0x10
#define I2C_IRQSTATUS_RAW           0x24
#define I2C_IRQSTATUS               0x28
#define I2C_IRQENABLE_SET           0x2C
#define I2C_IRQENABLE_CLR           0x30
#define I2C_WE                      0x34
#define I2C_DMARXENABLE_SET         0x38
#define I2C_DMATXENABLE_SET         0x3C
#define I2C_DMARXENABLE_CLR         0x40
#define I2C_DMATXENABLE_CLR         0x44
#define I2C_SYSS                    0x90
#define I2C_BUF                     0x94
#define I2C_CNT                     0x98
#define I2C_DATA                    0x9C
#define I2C_CON                     0xA4
#define I2C_OA                      0xA8
#define I2C_SA                      0xAC
#define I2C_PSC                     0xB0
#define I2C_SCLL                    0xB4
#define I2C_SCLH                    0xB8
#define I2C_SYSTEST                 0xBC
#define I2C_BUFSTAT                 0xC0
#define I2C_OA1                     0xC4
#define I2C_OA2                     0xC8
#define I2C_OA3                     0xCC
#define I2C_ACTOA                   0xD0
#define I2C_SBLOCK                  0xD4

#define I2C_REG_COUNT               ((I2C_SBLOCK / 4) + 1)

/* I2C_CON bit definitions */
#define I2C_CON_EN                  (1 << 15)
#define I2C_CON_MST                 (1 << 10)
#define I2C_CON_TRX                 (1 << 9)
#define I2C_CON_STP                 (1 << 1)
#define I2C_CON_STT                 (1 << 0)

/* I2C_IRQSTATUS bits */
#define I2C_IRQ_AL                  (1 << 0)
#define I2C_IRQ_NACK                (1 << 1)
#define I2C_IRQ_ARDY                (1 << 2)
#define I2C_IRQ_RRDY                (1 << 3)
#define I2C_IRQ_XRDY                (1 << 4)
#define I2C_IRQ_BB                  (1 << 12)

/* ========================================================================= */
/*                      McSPI REGISTER OFFSETS                               */
/* ========================================================================= */

#define MCSPI_REVISION              0x000
#define MCSPI_SYSCONFIG             0x010
#define MCSPI_SYSSTATUS             0x014
#define MCSPI_IRQSTATUS             0x018
#define MCSPI_IRQENABLE             0x01C
#define MCSPI_WAKEUPENABLE          0x020
#define MCSPI_SYST                  0x024
#define MCSPI_MODULCTRL             0x028
#define MCSPI_XFERLEVEL             0x02C

/* Per-channel registers: base = 0x100 + (ch * 0x14) */
#define MCSPI_CH_BASE(ch)           (0x100 + (ch) * 0x14)
#define MCSPI_CH_CONF(ch)           (MCSPI_CH_BASE(ch) + 0x00)
#define MCSPI_CH_STAT(ch)           (MCSPI_CH_BASE(ch) + 0x04)
#define MCSPI_CH_CTRL(ch)           (MCSPI_CH_BASE(ch) + 0x08)
#define MCSPI_CH_TX(ch)             (MCSPI_CH_BASE(ch) + 0x0C)
#define MCSPI_CH_RX(ch)             (MCSPI_CH_BASE(ch) + 0x10)

#define MCSPI_MAX_CHANNELS          4
#define MCSPI_REG_COUNT             0x60  /* rough size in uint32 units */

/* MCSPI_CH_STAT bits */
#define MCSPI_STAT_RXS              (1 << 0)
#define MCSPI_STAT_TXS              (1 << 1)
#define MCSPI_STAT_EOT              (1 << 2)
#define MCSPI_STAT_TXFFE            (1 << 3)
#define MCSPI_STAT_TXFFF            (1 << 4)
#define MCSPI_STAT_RXFFE            (1 << 5)
#define MCSPI_STAT_RXFFF            (1 << 6)

/* ========================================================================= */
/*                      DMTIMER REGISTER OFFSETS                             */
/* ========================================================================= */

#define TIMER_TIDR                  0x00
#define TIMER_TIOCP_CFG             0x10
#define TIMER_IRQ_EOI               0x20
#define TIMER_IRQSTATUS_RAW         0x24
#define TIMER_IRQSTATUS             0x28
#define TIMER_IRQENABLE_SET         0x2C
#define TIMER_IRQENABLE_CLR         0x30
#define TIMER_IRQWAKEEN             0x34
#define TIMER_TCLR                  0x38
#define TIMER_TCRR                  0x3C
#define TIMER_TLDR                  0x40
#define TIMER_TTGR                  0x44
#define TIMER_TWPS                  0x48
#define TIMER_TMAR                  0x4C
#define TIMER_TCAR1                 0x50
#define TIMER_TSICR                 0x54
#define TIMER_TCAR2                 0x58

#define TIMER_REG_COUNT             ((TIMER_TCAR2 / 4) + 1)

/* TCLR bit definitions */
#define TIMER_TCLR_ST               (1 << 0)  /* Start/stop timer */
#define TIMER_TCLR_AR               (1 << 1)  /* Auto-reload */
#define TIMER_TCLR_CE               (1 << 6)  /* Compare enable */
#define TIMER_TCLR_SCPWM            (1 << 7)  /* PWM default value */
#define TIMER_TCLR_PT               (1 << 12) /* Pulse/toggle mode */
#define TIMER_TCLR_TRG_SHIFT        10
#define TIMER_TCLR_TRG_MASK         (0x3 << 10)

/* IRQ bits */
#define TIMER_IRQ_MAT               (1 << 0)  /* Match */
#define TIMER_IRQ_OVF               (1 << 1)  /* Overflow */
#define TIMER_IRQ_TCAR              (1 << 2)  /* Capture */

/* ========================================================================= */
/*                      ADC/TSC REGISTER OFFSETS                             */
/* ========================================================================= */

#define ADC_REVISION                0x000
#define ADC_SYSCONFIG               0x010
#define ADC_IRQSTATUS_RAW           0x024
#define ADC_IRQSTATUS               0x028
#define ADC_IRQENABLE_SET           0x02C
#define ADC_IRQENABLE_CLR           0x030
#define ADC_IRQWAKEUP               0x034
#define ADC_DMAENABLE_SET           0x038
#define ADC_DMAENABLE_CLR           0x03C
#define ADC_CTRL                    0x040
#define ADC_ADCSTAT                 0x044
#define ADC_ADCRANGE                0x048
#define ADC_ADC_CLKDIV              0x04C
#define ADC_ADC_MISC                0x050
#define ADC_STEPENABLE              0x054
#define ADC_IDLECONFIG              0x058
#define ADC_TS_CHARGE_STEPCONFIG    0x05C
#define ADC_TS_CHARGE_DELAY         0x060

/* Step config/delay: STEPCONFIG(n) at 0x64 + n*8, STEPDELAY(n) at 0x68 + n*8 */
#define ADC_STEPCONFIG(n)           (0x064 + (n) * 8)
#define ADC_STEPDELAY(n)            (0x068 + (n) * 8)

#define ADC_FIFO0COUNT              0x0E4
#define ADC_FIFO0THRESHOLD          0x0E8
#define ADC_DMA0REQ                 0x0EC
#define ADC_FIFO1COUNT              0x0F0
#define ADC_FIFO1THRESHOLD          0x0F4
#define ADC_DMA1REQ                 0x0F8
#define ADC_FIFO0DATA               0x100
#define ADC_FIFO1DATA               0x200

#define ADC_MAX_STEPS               16
#define ADC_MAX_CHANNELS            8
#define ADC_FIFO_SIZE               64
#define ADC_MAX_VALUE               4095

#define ADC_REG_COUNT               0x100  /* rough size in uint32 units */

/* ADC_CTRL bits */
#define ADC_CTRL_ENABLE             (1 << 0)
#define ADC_CTRL_STEPCONFIG_WP      (1 << 2)

/* ========================================================================= */
/*                  INTERRUPT CONTROLLER REGISTER OFFSETS                    */
/* ========================================================================= */

#define INTC_REVISION               0x000
#define INTC_SYSCONFIG              0x010
#define INTC_SYSSTATUS              0x014
#define INTC_SIR_IRQ                0x040
#define INTC_SIR_FIQ                0x044
#define INTC_CONTROL                0x048
#define INTC_PROTECTION             0x04C
#define INTC_IDLE                   0x050
#define INTC_IRQ_PRIORITY           0x060
#define INTC_FIQ_PRIORITY           0x064
#define INTC_THRESHOLD              0x068

/* Bank registers: 4 banks of 32 interrupts each */
#define INTC_ITR(n)                 (0x080 + (n) * 0x20)
#define INTC_MIR(n)                 (0x084 + (n) * 0x20)
#define INTC_MIR_CLEAR(n)           (0x088 + (n) * 0x20)
#define INTC_MIR_SET(n)             (0x08C + (n) * 0x20)
#define INTC_ISR_SET(n)             (0x090 + (n) * 0x20)
#define INTC_ISR_CLEAR(n)           (0x094 + (n) * 0x20)
#define INTC_PENDING_IRQ(n)         (0x098 + (n) * 0x20)
#define INTC_PENDING_FIQ(n)         (0x09C + (n) * 0x20)

/* ILR: 128 interrupt priority/routing registers */
#define INTC_ILR(n)                 (0x100 + (n) * 4)

#define INTC_NUM_IRQS               128
#define INTC_NUM_BANKS              4
#define INTC_REG_COUNT              0x180  /* rough size in uint32 units */

/* INTC interrupt numbers for peripherals */
#define INTC_GPIO0A_IRQ             96
#define INTC_GPIO0B_IRQ             97
#define INTC_GPIO1A_IRQ             98
#define INTC_GPIO1B_IRQ             99
#define INTC_GPIO2A_IRQ             32
#define INTC_GPIO2B_IRQ             33
#define INTC_GPIO3A_IRQ             62
#define INTC_GPIO3B_IRQ             63
#define INTC_UART0_IRQ              72
#define INTC_UART1_IRQ              73
#define INTC_UART2_IRQ              74
#define INTC_I2C0_IRQ               70
#define INTC_I2C1_IRQ               71
#define INTC_I2C2_IRQ               30
#define INTC_SPI0_IRQ               65
#define INTC_SPI1_IRQ               125
#define INTC_TIMER2_IRQ             68
#define INTC_TIMER3_IRQ             69
#define INTC_TIMER4_IRQ             92
#define INTC_TIMER5_IRQ             93
#define INTC_TIMER6_IRQ             94
#define INTC_TIMER7_IRQ             95
#define INTC_ADC_TSC_IRQ            16

/* ========================================================================= */
/*                   CONTROL MODULE PINMUX OFFSETS                           */
/* ========================================================================= */

/* Pinmux register offsets from Control Module base (0x44E10000) */
/* GPIO-capable pins on P8/P9 headers */
#define CTRL_CONF_GPMC_AD0          0x800
#define CTRL_CONF_GPMC_AD1          0x804
#define CTRL_CONF_GPMC_AD2          0x808
#define CTRL_CONF_GPMC_AD3          0x80C
#define CTRL_CONF_GPMC_AD4          0x810
#define CTRL_CONF_GPMC_AD5          0x814
#define CTRL_CONF_GPMC_AD6          0x818
#define CTRL_CONF_GPMC_AD7          0x81C
#define CTRL_CONF_GPMC_AD8          0x820
#define CTRL_CONF_GPMC_AD9          0x824
#define CTRL_CONF_GPMC_AD10         0x828
#define CTRL_CONF_GPMC_AD11         0x82C
#define CTRL_CONF_GPMC_AD12         0x830
#define CTRL_CONF_GPMC_AD13         0x834
#define CTRL_CONF_GPMC_AD14         0x838
#define CTRL_CONF_GPMC_AD15         0x83C
#define CTRL_CONF_GPMC_A0           0x840
#define CTRL_CONF_GPMC_A1           0x844
#define CTRL_CONF_GPMC_A2           0x848
#define CTRL_CONF_GPMC_A3           0x84C
#define CTRL_CONF_GPMC_A4           0x850
#define CTRL_CONF_GPMC_A5           0x854
#define CTRL_CONF_GPMC_A6           0x858
#define CTRL_CONF_GPMC_A7           0x85C
#define CTRL_CONF_GPMC_A8           0x860
#define CTRL_CONF_GPMC_A9           0x864
#define CTRL_CONF_GPMC_A10          0x868
#define CTRL_CONF_GPMC_A11          0x86C
#define CTRL_CONF_GPMC_WAIT0        0x870
#define CTRL_CONF_GPMC_WPN          0x874
#define CTRL_CONF_GPMC_BEN1         0x878
#define CTRL_CONF_GPMC_CSN0         0x87C
#define CTRL_CONF_GPMC_CSN1         0x880
#define CTRL_CONF_GPMC_CSN2         0x884
#define CTRL_CONF_GPMC_CSN3         0x888
#define CTRL_CONF_GPMC_CLK          0x88C
#define CTRL_CONF_GPMC_ADVN_ALE     0x890
#define CTRL_CONF_GPMC_OEN_REN      0x894
#define CTRL_CONF_GPMC_WEN          0x898
#define CTRL_CONF_GPMC_BEN0_CLE     0x89C

/* Pinmux field definitions */
#define CTRL_PINMUX_MODE_MASK       0x07
#define CTRL_PINMUX_PUDEN           (1 << 3)  /* Pull-up/down enable */
#define CTRL_PINMUX_PUTYPESEL       (1 << 4)  /* Pull-up type select */
#define CTRL_PINMUX_RXACTIVE        (1 << 5)  /* Receiver active */
#define CTRL_PINMUX_SLEWCTRL        (1 << 6)  /* Slew rate control */
#define CTRL_PINMUX_MODE7_GPIO      0x07

#define CTRL_REG_COUNT              0x2000  /* rough size in uint32 units */

/* Control Module device ID */
#define CTRL_DEVICE_ID              0x000
#define CTRL_DEVICE_ID_VALUE        0x2B94402E  /* AM335x */

#endif /* AM335X_MAP_H */
