#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <signal.h>

// GPIO1 base address
#define GPIO1_BASE 0x4804C000
#define GPIO1_SIZE 0x1000 // 4KB

// Offsets cho GPIO1 registers (từ TRM spruh73q.pdf, Chapter 25)
#define GPIO_OE 0x134           // Output Enable (0=output, 1=input)
#define GPIO_DATAOUT 0x13C      // Output data register
#define GPIO_SETDATAOUT 0x194   // Set output bit (write 1 to set)
#define GPIO_CLEARDATAOUT 0x190 // Clear output bit (write 1 to clear)

// GPIO1_21 là bit 21
#define GPIO_BIT21 21
#define GPIO_MASK21 (1 << GPIO_BIT21)

// GPIO1_22 là bit 22
#define GPIO_BIT22 22
#define GPIO_MASK22 (1 << GPIO_BIT22)

// GPIO1_23 là bit 23
#define GPIO_BIT23 23
#define GPIO_MASK23 (1 << GPIO_BIT23)

// GPIO1_24 là bit 24
#define GPIO_BIT24 24
#define GPIO_MASK24 (1 << GPIO_BIT24)

// Globals used for cleanup
static volatile uint32_t *gpio_base = NULL;
static int dev_fd = -1;
static volatile sig_atomic_t stop = 0;

static void handle_sigint(int signum)
{
    (void)signum;
    stop = 1;
}

int main()
{
    int fd;

    // Bước 1: Mở /dev/mem
    fd = open("/dev/mem", O_RDWR);
    if (fd < 0)
    {
        perror("open /dev/mem");
        return 1;
    }
    printf("Opened /dev/mem\n");
    dev_fd = fd;

    /* Install SIGINT handler so Ctrl+C triggers cleanup */
    signal(SIGINT, handle_sigint);

    // Bước 2: Ánh xạ vùng GPIO1 từ vật lý → ảo
    gpio_base = (volatile uint32_t *)mmap(
        NULL,                   // Để kernel chọn virtual address
        GPIO1_SIZE,             // Size: 4KB
        PROT_READ | PROT_WRITE, // Quyền đọc+ghi
        MAP_SHARED,             // Chia sẻ với kernel
        fd,                     // File descriptor
        GPIO1_BASE              // Physical address
    );

    if (gpio_base == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return 1;
    }
    printf("Mapped GPIO1 to virtual address: 0x%p\n", (void *)gpio_base);

    // Bước 3: Cấu hình GPIO1_21 là output
    // Đọc GPIO_OE
    volatile uint32_t *gpio_oe = gpio_base + (GPIO_OE / 4);
    uint32_t oe_value = *gpio_oe;
    printf("GPIO_OE before: 0x%08X\n", oe_value);

    // Ghi GPIO_OE: set bit 21 = 0 (output)
    oe_value &= ~GPIO_MASK21; // Clear bit 21
    *gpio_oe = oe_value;
    printf("GPIO_OE after: 0x%08X\n", *gpio_oe);

    // Ghi GPIO_OE: set bit 22 = 0 (output)
    oe_value &= ~GPIO_MASK22; // Clear bit 22
    *gpio_oe = oe_value;
    printf("GPIO_OE after: 0x%08X\n", *gpio_oe);

    // Ghi GPIO_OE: set bit 23 = 0 (output)
    oe_value &= ~GPIO_MASK23; // Clear bit 23
    *gpio_oe = oe_value;
    printf("GPIO_OE after: 0x%08X\n", *gpio_oe);

    // Ghi GPIO_OE: set bit 24 = 0 (output)
    oe_value &= ~GPIO_MASK24; // Clear bit 24
    *gpio_oe = oe_value;
    printf("GPIO_OE after: 0x%08X\n", *gpio_oe);

    sleep(1);

    while (!stop)
    {
        // Bước 4: Bật LED (set GPIO1_21 HIGH)
        printf("\nTurning LED ON...\n");
        volatile uint32_t *gpio_setdataout = gpio_base + (GPIO_SETDATAOUT / 4);
        *gpio_setdataout = GPIO_MASK21; // Write 1 to bit 21 → output goes HIGH

        // Bước 4: Bật LED (set GPIO1_22 HIGH)
        printf("\nTurning LED ON...\n");
        *gpio_setdataout = GPIO_MASK22; // Write 1 to bit 22 → output goes HIGH

        // Bước 4: Bật LED (set GPIO1_23 HIGH)
        printf("\nTurning LED ON...\n");
        *gpio_setdataout = GPIO_MASK23; // Write 1 to bit 23 → output goes HIGH

        // Bước 4: Bật LED (set GPIO1_24 HIGH)
        printf("\nTurning LED ON...\n");
        *gpio_setdataout = GPIO_MASK24; // Write 1 to bit 24 → output goes HIGH

        usleep(500000);
        printf("LED should be ON now\n");

        // Bước 5: Tắt LED (set GPIO1_21 LOW)
        printf("\nTurning LED OFF...\n");
        volatile uint32_t *gpio_cleardataout = gpio_base + (GPIO_CLEARDATAOUT / 4);
        *gpio_cleardataout = GPIO_MASK21; // Write 1 to bit 21 → output goes LOW

        // Bước 5: Tắt LED (set GPIO1_22 LOW)
        printf("\nTurning LED OFF...\n");
        *gpio_cleardataout = GPIO_MASK22; // Write 1 to bit 22 → output goes LOW

        // Bước 5: Tắt LED (set GPIO1_23 LOW)
        printf("\nTurning LED OFF...\n");
        *gpio_cleardataout = GPIO_MASK23; // Write 1 to bit 23 → output goes LOW

        // Bước 5: Tắt LED (set GPIO1_24 LOW)
        printf("\nTurning LED OFF...\n");
        *gpio_cleardataout = GPIO_MASK24; // Write 1 to bit 24 → output goes LOW

        usleep(500000);
        printf("LED should be OFF now\n");
    }
    /* Cleanup after loop (e.g., user pressed Ctrl+C) */
    printf("\nCaught interrupt, cleaning up...\n");
    if (gpio_base != NULL) {
        munmap((void *)gpio_base, GPIO1_SIZE);
        gpio_base = NULL;
    }
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    } else if (fd >= 0) {
        close(fd);
    }
    printf("\nCleaned up\n");

    return 0;
}