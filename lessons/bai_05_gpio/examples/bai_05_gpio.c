#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#define GPIO1_PHY_ADD 0x4804C000
#define CM_PER_PHY_ADD 0x44E00000
#define CONTROL_MODULE_PHY_ADD 0x44E10000
#define CONF_GPMC_A5 0x854
#define GPIO1_OE 0x134
#define GPIO1_SETDATAOUT 0x194
#define GPIO1_CLEARDATAOUT 0x190

#define CM_PER_GPIO1_CLKCTRL 0xAC

#define PAGE_SIZE 4096 // map de chua du offset
#define LED21 (1 << 24)

int main(void)
{
    printf("start\n");
    int fd;
    volatile uint32_t* clk_base;
    volatile uint32_t* ctrl_base;
    volatile uint32_t* gpio1_base;



    fd = open("/dev/mem", O_RDWR);
    if(fd < 0){
        printf("Open mem failed\n");
        return 1;
    }

    clk_base = (volatile uint32_t*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CM_PER_PHY_ADD);
    if(clk_base == MAP_FAILED){
        perror("mmap clk_base failed");
        close(fd);
        return 1;
    }

    ctrl_base = (volatile uint32_t*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CONTROL_MODULE_PHY_ADD);
    if(ctrl_base == MAP_FAILED){
        perror("mmap ctrl_base failed");
        close(fd);
        munmap((void*)clk_base, PAGE_SIZE);
        return 1;
    }

    gpio1_base = (volatile uint32_t*)mmap(NULL, PAGE_SIZE, PROT_WRITE | PROT_WRITE, MAP_SHARED, fd, GPIO1_PHY_ADD);
    if(gpio1_base == MAP_FAILED){
        perror("mmap gpio1_base failed\n");
        munmap((void*)clk_base, PAGE_SIZE);
        munmap((void*)ctrl_base, PAGE_SIZE);
        close(fd);
        return 1;
    }

    // config clock 
    clk_base[CM_PER_GPIO1_CLKCTRL / 4] = (1 << 18) | 0x2;

    // config pinmux
    ctrl_base[CONF_GPMC_A5 / 4] = 0x07;

    // config gpio1
    gpio1_base[GPIO1_OE / 4] &= ~LED21; // 0000 0000 0010 0000 0000 0000 0000 0000
                                        // 1111 1111 1101 1111 1111 1111 1111 1111 

    while (1)
    {    
        // On led
        gpio1_base[GPIO1_SETDATAOUT / 4] = LED21;
        sleep(1);

        // Off led
        gpio1_base[GPIO1_CLEARDATAOUT / 4] = LED21;
        sleep(1);
    }
    munmap((void *)gpio1_base, PAGE_SIZE);
    munmap((void *)clk_base, PAGE_SIZE);
    munmap((void *)ctrl_base, PAGE_SIZE);
    close(fd);

    return 0;
}