# BBB Simulator - BeagleBone Black Hardware Simulator

A GTK3-based simulator for the BeagleBone Black (AM335x SoC) that can load and execute ARM firmware binaries and kernel modules (.ko) using the Unicorn CPU emulation engine.

## Features

- **ARM Cortex-A8 CPU emulation** via Unicorn Engine
- **Full peripheral model**: GPIO0-3, UART0-5, I2C0-2, SPI0-1, DMTimer0-7, ADC/TSC, INTC, PRCM, Control Module
- **ELF loader**: Load statically-linked ARM binaries (ET_EXEC) and relocatable kernel modules (ET_REL / .ko)
- **Linux syscall emulation**: ~15 key syscalls (read, write, mmap2, brk, nanosleep, etc.)
- **Mock kernel API**: printk, kmalloc/kfree, ioremap, readl/writel, gpio_* functions
- **GTK3 GUI** with 4 panels:
  - GPIO visualization (4 banks × 32 LEDs)
  - UART0 console (input/output)
  - Register viewer (CPU, GPIO, UART, PRCM)
  - Memory hex viewer

## Prerequisites (Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
    libgtk-3-dev libunicorn-dev \
    gcc-arm-linux-gnueabihf
```

## Build

```bash
cd bbb_simulator
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

```bash
# Launch GUI only
./bbb_simulator

# Load firmware and launch
./bbb_simulator path/to/firmware.elf

# Load kernel module
./bbb_simulator --ko path/to/module.ko

# Show help
./bbb_simulator --help
```

## Compiling Firmware for the Simulator

Firmware must be **statically linked** ARM ELF binaries:

```bash
arm-linux-gnueabihf-gcc -static -o firmware firmware.c
```

Example firmware that blinks GPIO1_21 (USR3 LED):

```c
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define GPIO1_BASE 0x4804C000
#define GPIO_OE           0x134
#define GPIO_SETDATAOUT   0x194
#define GPIO_CLEARDATAOUT 0x190

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    volatile uint32_t *gpio1 = mmap(NULL, 0x1000,
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO1_BASE);

    // Set pin 21 as output
    gpio1[GPIO_OE / 4] &= ~(1 << 21);

    while (1) {
        gpio1[GPIO_SETDATAOUT / 4] = (1 << 21);
        usleep(500000);
        gpio1[GPIO_CLEARDATAOUT / 4] = (1 << 21);
        usleep(500000);
    }
    return 0;
}
```

## Compiling Kernel Modules

```bash
# Use ARM cross-compiler with a suitable kernel headers directory
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C /path/to/kernel M=$(pwd) modules
```

The simulator provides mock implementations for common kernel functions:
- `printk()` → UART0 console output
- `kmalloc()` / `kfree()` → simulated heap
- `ioremap()` → identity-mapped (returns physical address)
- `readl()` / `writel()` → triggers MMIO hooks
- `gpio_request/free/direction/set/get` → logged

## Architecture

```
┌──────────────┐
│    main.c    │  Entry point + argument parsing
└──────┬───────┘
       │
┌──────▼───────┐
│  bbb_sim.c   │  Creates and wires all subsystems
└──────┬───────┘
       │
  ┌────┴────────────────────────────────────┐
  │                                         │
┌─▼──────────┐  ┌──────────────┐  ┌────────▼────────┐
│  cpu_emu.c  │  │ mem_subsys.c │  │  gui/*.c        │
│  (Unicorn)  │  │ (MMIO hooks) │  │  (GTK3 panels)  │
└─────────────┘  └──────┬───────┘  └─────────────────┘
                        │
    ┌───────────────────┼───────────────────┐
    │                   │                   │
┌───▼─────┐  ┌─────────▼──────┐  ┌─────────▼──────┐
│ periph/ │  │ elf_loader.c   │  │ ko_loader.c    │
│ gpio.c  │  │ syscall_emu.c  │  │ (mock kernel)  │
│ uart.c  │  └────────────────┘  └────────────────┘
│ i2c.c   │
│ spi.c   │
│ timer.c │
│ adc.c   │
│ intc.c  │
│ prcm.c  │
│ control │
└─────────┘
```

## AM335x Register Map

All register addresses and offsets are defined in `include/am335x_map.h`, verified against the AM335x Technical Reference Manual (spruh73q).

## License

Educational use. Based on TI AM335x TRM documentation.
