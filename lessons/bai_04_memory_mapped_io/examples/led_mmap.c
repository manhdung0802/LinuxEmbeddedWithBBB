#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#define GPIO1_SIZE 0x1000
#define GPIO1_BASE 0x4804C000

#define GPIO_OE 0x134
#define GPIO_DATAOUT 0x13C
#define GPIO_SETDATAOUT 0x194
#define GPIO_CLEARDATAOUT 0x190

#define GPIO1_24 24
#define GPIO1_24_MASK (1 << 24) | (1 << 22) | (1 << 21) | (1 << 23)

typedef struct
{
    int fd;
    volatile uint32_t *gpio_base;
    uint32_t gpio_size;
    uint32_t phy_add;
} gpio_mmap;

int init_gpio(gpio_mmap *gpio, uint32_t phy_add, uint32_t gpio_size)
{
    gpio->phy_add = phy_add;
    gpio->gpio_size = gpio_size;
    gpio->fd = open("/dev/mem", O_RDWR);
    if (gpio->fd < 0)
    {
        printf("open mem failed");
        return -1;
    }
    printf("open mem success");
    gpio->gpio_base = (volatile uint32_t *)mmap(
        NULL, gpio_size, PROT_READ | PROT_WRITE, MAP_SHARED, gpio->fd, phy_add);

    if (gpio->gpio_base == MAP_FAILED)
    {
        perror("mmap");
        close(gpio->fd);
        return -1;
    }

    return 0;
}

void write_clear_gpio(gpio_mmap *gpio, uint32_t gpio_setdataout, uint32_t mask)
{
    volatile uint32_t *temp = gpio->gpio_base + (gpio_setdataout / 4);
    *temp = mask;
}

void cleanup(gpio_mmap *gpio)
{
    munmap((void *)gpio->gpio_base, gpio->gpio_size);
    close(gpio->fd);
}

int main()
{

    gpio_mmap gpio;
    printf("1\n");
    (void)init_gpio(&gpio, GPIO1_BASE, GPIO1_SIZE);
    printf("1.1\n");

    uint32_t oe = *(&gpio.gpio_base + (GPIO_OE / 4));
    write_clear_gpio(&gpio, GPIO_OE, oe &= ~GPIO1_24_MASK);
    printf("2\n");
    sleep(1);

    int n = 0;

    while (1)
    {
        write_clear_gpio(&gpio, GPIO_SETDATAOUT, GPIO1_24_MASK);
        printf("3\n");
        usleep(500000);
        write_clear_gpio(&gpio, GPIO_CLEARDATAOUT, GPIO1_24_MASK);
        printf("4\n");
        usleep(500000);
        n++;
        if(n == 10) break;
    }

    cleanup(&gpio);

    return 0;
}