# BBB Simulator - Session Context

> **Purpose**: This file captures the full project state so a new AI session can continue development without losing context. Read this file first before making changes.

## 1. Project Goal

Build a **C + GTK3 GUI application** on Ubuntu that simulates BeagleBone Black (AM335x SoC) hardware using **Unicorn Engine** for ARM Cortex-A8 CPU emulation. It loads statically-compiled ARM ELF firmware and .ko kernel modules with a mocked kernel API.

**User compiles firmware with**: `arm-linux-gnueabihf-gcc -static -o firmware firmware.c`

## 2. Build & Dependencies

```bash
# Ubuntu install
sudo apt install build-essential cmake pkg-config libgtk-3-dev libunicorn-dev gcc-arm-linux-gnueabihf

# Build
cd bbb_simulator && mkdir build && cd build && cmake .. && make -j$(nproc)

# Run
./bbb_simulator                     # GUI only
./bbb_simulator firmware.elf        # Load firmware
./bbb_simulator --ko module.ko      # Load kernel module
```

## 3. Architecture

```
main.c → bbb_sim.c (creates all subsystems)
           ├─ cpu_emu.c         (Unicorn ARM engine, SVC hook)
           ├─ mem_subsys.c      (MMIO region dispatch via UC hooks)
           ├─ elf_loader.c      (ELF32 ARM: ET_EXEC + ET_REL)
           ├─ syscall_emu.c     (~15 Linux ARM syscalls)
           ├─ ko_loader.c       (mock kernel: printk, kmalloc, ioremap, etc.)
           ├─ periph/           (9 peripheral models)
           │   ├─ gpio.c        (GPIO0-3, 32 pins each, OE/DATAIN/DATAOUT)
           │   ├─ uart.c        (UART0-5, 64-byte FIFOs, 16C750-compat)
           │   ├─ i2c.c         (I2C0-2, virtual slaves, START/STOP)
           │   ├─ spi.c         (SPI0-1, 4 channels, loopback)
           │   ├─ timer.c       (DMTimer0-7, host-clock based)
           │   ├─ adc.c         (ADC/TSC, step-based, 2 FIFOs)
           │   ├─ intc.c        (INTC, 128 IRQs, MIR/ILR priority)
           │   ├─ prcm.c        (CM_PER + CM_WKUP clock control)
           │   └─ control.c     (Pinmux registers)
           └─ gui/              (GTK3 GUI)
               ├─ gui_main.c    (Window, toolbar, 2×2 grid, status bar)
               ├─ gui_gpio.c    (Cairo LED circles for 4×32 pins)
               ├─ gui_uart.c    (UART0 console textview + entry)
               ├─ gui_registers.c (Treeview: CPU/GPIO/UART registers)
               └─ gui_memory.c  (Hex dump viewer, 256 bytes)
```

## 4. Memory Map (AM335x)

| Region | Address | Size | Unicorn Mapping |
|--------|---------|------|-----------------|
| OCMC SRAM | 0x40300000 | 64 KB | UC_PROT_ALL |
| L4_WKUP MMIO | 0x44C00000 | 4 MB | UC hooks → periph dispatch |
| L4_PER MMIO | 0x48000000 | 16 MB | UC hooks → periph dispatch |
| DDR | 0x80000000 | 256 MB | UC_PROT_ALL |
| Heap (brk) | 0x81000000 | 16 MB | Part of DDR |
| KO region | 0x82000000 | 16 MB | Mapped by ko_loader |
| Mock kernel | 0x83000000 | 1 MB | Mapped by ko_loader |
| kmalloc heap | 0x84000000 | 16 MB | Mapped by ko_loader |
| Stack top | 0x8F000000 | - | Initial SP |

## 5. File Inventory (39 files, ~7300 lines)

### Headers (include/)
| File | Lines | Purpose |
|------|-------|---------|
| am335x_map.h | ~850 | ALL register addresses, offsets, bit fields |
| bbb_sim.h | 95 | Main sim context struct (bbb_sim_t) |
| cpu_emu.h | 65 | Unicorn wrapper (cpu_emu_t) |
| elf_loader.h | 150 | ELF32 structs, loader API |
| ko_loader.h | 40 | .ko loader + mock kernel API |
| gui.h | 85 | bbb_gui_t struct, panel functions |
| mem_subsys.h | 45 | MMIO region registration |
| syscall_emu.h | 50 | Syscall numbers and API |
| periph/gpio.h | 60 | gpio_periph_t |
| periph/uart.h | 75 | uart_periph_t, TX callback |
| periph/i2c.h | 70 | i2c_periph_t, i2c_slave_t |
| periph/spi.h | 50 | spi_periph_t |
| periph/timer.h | 60 | timer_periph_t |
| periph/adc.h | 60 | adc_periph_t, channel values |
| periph/intc.h | 70 | intc_periph_t, 128 IRQs |
| periph/prcm.h | 50 | prcm_periph_t |
| periph/control.h | 40 | control_periph_t (pinmux) |

### Sources (src/)
| File | Lines | Purpose |
|------|-------|---------|
| main.c | 106 | Entry point, CLI args |
| bbb_sim.c | 350 | Create/wire all subsystems, MMIO registration |
| cpu_emu.c | 260 | Unicorn ARM init, SVC hook dispatch, worker thread |
| mem_subsys.c | 200 | Memory mapping, MMIO read/write hooks |
| elf_loader.c | 400 | ELF parse, PT_LOAD, relocations |
| ko_loader.c | 450 | Mock kernel stubs, ko_load/unload, mock SVC handler |
| syscall_emu.c | 380 | ARM syscall dispatch (R7-based) |
| periph/gpio.c | 250 | SET/CLEAR DATAOUT, OE, edge detection |
| periph/uart.c | 280 | Circular FIFO, mode switching, TX callback |
| periph/i2c.c | 280 | Virtual slave bus, CON start/stop |
| periph/spi.c | 120 | Channel enable, TX→RX loopback |
| periph/timer.c | 280 | CLOCK_MONOTONIC-based counter, prescaler |
| periph/adc.c | 260 | Step config, FIFO, channel values |
| periph/intc.c | 300 | MIR masking, ILR priority, SIR_IRQ |
| periph/prcm.c | 180 | CLKCTRL module enable, IDLEST |
| periph/control.c | 80 | Pinmux array, mode logging |
| gui/gui_main.c | 350 | GTK app, toolbar, grid, timer |
| gui/gui_gpio.c | 150 | Cairo drawing, LED visualization |
| gui/gui_uart.c | 160 | Text view, entry, gdk_threads_add_idle |
| gui/gui_registers.c | 220 | Combo + treeview, CPU/GPIO/UART |
| gui/gui_memory.c | 160 | Hex dump, address entry |

## 6. Key Design Details

### MMIO Dispatch Chain
```
ARM code accesses 0x4804C134 (GPIO1_OE)
  → Unicorn UC_HOOK_MEM_WRITE fires
  → mem_subsys_write() searches regions
  → Finds GPIO1 region (base=0x4804C000)
  → Calls gpio_write(opaque, addr, offset=0x134, value)
  → GPIO model updates OE register
```

### SVC Hook (cpu_emu.c hook_intr)
```
SVC instruction intercepted:
  - Read SVC# from instruction word (imm24)
  - If SVC#0xFF → ko_handle_mock_svc(R12=mock_id)
  - Else → syscall_emu_handle() using R7 syscall number
```

### mmap("/dev/mem") Emulation
```
open("/dev/mem") → returns fake fd=100
mmap2(fd=100, offset=phys_addr) → returns phys_addr directly (identity map)
  → firmware uses returned pointer as MMIO address
  → Unicorn MMIO hooks fire → peripheral models respond
```

### Console Output Path
```
UART0 TX write → uart_tx_callback() in bbb_sim.c
  → g_console_cb → gui_uart.c uart_console_cb()
  → gdk_threads_add_idle(idle_append_text)
  → GTK text buffer append
```

### Global Variables
- `g_console_cb` / `g_console_userdata` — defined in `bbb_sim.c`, extern'd in `syscall_emu.c`

## 7. Bugs Fixed

1. **prcm_read/prcm_write undefined** → Fixed to `prcm_cm_per_read/write` and `prcm_cm_wkup_read/write` in bbb_sim.c
2. **Redundant comparison** in cpu_emu_step() → Fixed `err != UC_ERR_OK && err != UC_ERR_OK` to single check
3. **UART TX callback signature mismatch** → Fixed to `(int uart_id, char ch, void *user_data)`
4. **Duplicate bbb_sim_set_console_callback** → Removed from syscall_emu.c, kept in bbb_sim.c
5. **gui_memory button wiring** → Fixed double-bound signal to single callback
6. **uart_push_rx** → Fixed to correct function name `uart_receive_char`
7. **Missing unistd.h** in ko_loader.c → Added for usleep()

## 8. Current Status: IMPLEMENTATION COMPLETE

All 39 files (17 headers + 22 source) are written. The project has NOT been compiled yet (requires Ubuntu with libunicorn-dev and libgtk-3-dev).

### What's NOT Implemented (Known Limitations)
- DMA/eDMA, PRU-ICSS, Watchdog, RTC, GPMC
- No MMU emulation (identity mapping assumed)
- GPIO edge detection not wired to INTC
- UART console: UART0 only (other UARTs not forwarded to GUI)
- `bbb_sim_is_clock_enabled()` always returns true (stub)
- No persistent file I/O (stubs only)
- kmalloc is a linear bump allocator (no free)

### Next Steps (if continuing)
1. **Compile on Ubuntu** and fix any remaining compile errors
2. **Create test firmware** (GPIO blink) and verify end-to-end
3. **Wire GPIO interrupts to INTC** for proper interrupt simulation
4. **Add ADC sliders to GUI** for interactive analog input
5. **Add breakpoint support** (address-based stop in code hook)
6. **Add .ko test module** and verify mock kernel dispatch

## 9. AM335x Register Reference

All register offsets are in `include/am335x_map.h`. Key verified offsets:
- `CM_PER_GPIO1_CLKCTRL = 0xAC` (verified against TRM and user's existing code)
- GPIO register block: OE=0x134, DATAIN=0x138, DATAOUT=0x13C, SET=0x194, CLEAR=0x190
- UART: THR/RHR=0x00, LCR=0x0C, LSR=0x14, MDR1=0x20
- I2C: CON=0xA4, SA=0xAC, DATA=0x9C, IRQSTATUS=0x28
- Timer: TCLR=0x38, TCRR=0x3C, TLDR=0x40, TMAR=0x4C

## 10. User's Existing Code Examples

The user has ARM firmware examples in `lessons/bai_05_gpio/examples/` and `lessons/bai_04_memory_mapped_io/examples/` that use `/dev/mem` + mmap to access GPIO registers. These are the primary test targets for the simulator.

The user also has kernel module examples in `lessons/bai_17_kernel_module/` that use standard kernel APIs (printk, module_init/exit).
