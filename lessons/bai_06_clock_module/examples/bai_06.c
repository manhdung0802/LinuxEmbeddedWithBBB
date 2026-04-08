#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>

// Set clock for UART1 register in CM_PER domain
#define CM_PER_PHY_ADD 0x44E00000
#define CM_PER_UART1_CLKCTRL 0x6C
#define MODULEMODE_ENABLE 0x2
#define IDLEST_FUNC 0x0
#define IDLEST (0x3 << 16)

#define PAGE_SIZE 0x1000

int fd;
volatile uint32_t* cm_per_base = NULL;

void prcm_cleanup(volatile uint32_t *base){
    munmap((void *)base, PAGE_SIZE);
    close(fd);
}

int prcm_init(){
    fd = open("/dev/mem", O_RDWR);
    if(fd < 0){
        printf("Open mem failed\n");
        return -1;
    }
    cm_per_base = (volatile uint32_t*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CM_PER_PHY_ADD);
    if(cm_per_base == MAP_FAILED){
        perror("mmap prcm");
        close(fd);
        return -1;
    }
    return 0;
}

int prcm_enable(uint32_t offset){
    int timeout = 1000;
    volatile uint32_t *reg = &cm_per_base[offset / 4]; // cm_par_base + (offset / 4)
    *reg = MODULEMODE_ENABLE & 0x00;
    printf("Current IDLEST status: %d\n", (*reg & IDLEST) >> 16);
    *reg = MODULEMODE_ENABLE;
    while(((*reg & IDLEST) >> 16) != IDLEST_FUNC){
        printf("Current IDLEST status: %d\n", (*reg & IDLEST) >> 16);
        if(--timeout == 0){
            printf("Timeout, cannot setup clock\n");
            return -1;
        }
        usleep(1000);
    }
    printf("Enable clock UART1 successfully\n");
    return 0;
}

int main(){

    if(prcm_init() == -1){
        return -1;
    }
    if(prcm_enable(CM_PER_UART1_CLKCTRL) == -1){
        return -1;
    }
    prcm_cleanup(cm_per_base);
    return 0;
}