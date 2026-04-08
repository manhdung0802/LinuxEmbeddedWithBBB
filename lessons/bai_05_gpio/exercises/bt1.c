#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>

#define PAGE_SIZE 0x1000
#define GPIO2_PHY_ADD 0x481AC000
#define GPIO_0E 0x134
#define GPIO_DATAIN 0x138

#define CM_PER_PHY_ADD 0x44E00000
#define CM_PER_GPIO2_CLKCTRL 0xB0

#define CONTROL_MODULE_PHY_ADD 0x44E10000
#define conf_lcd_data2 0x8A8


int main(){
    int fd = open("/dev/mem", O_RDWR);
    if(fd < 0){
        printf("Open mem failed\n");
        return -1;
    }

    // config clock
    volatile uint32_t* clk_base = (volatile uint32_t*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CM_PER_PHY_ADD);
    if(clk_base == MAP_FAILED){
        perror("mmap");
        close(fd);
        return -1;
    }
    printf("Config clock done\n");
    clk_base[(CM_PER_GPIO2_CLKCTRL) / 4] = 0x2 | (1 << 18);


    // config pinmux
    volatile uint32_t* ctrl_base = (volatile uint32_t*)mmap(NULL, PAGE_SIZE * 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CONTROL_MODULE_PHY_ADD);
    if(ctrl_base == MAP_FAILED){
        perror("mmap");
        munmap((void *)clk_base, PAGE_SIZE);
        close(fd);
        return -1;
    }
    printf("Config pimux done\n");
    /*
     * MUX_MODE7 | INPUT_EN (bit5) | PULL_DISABLE (bit3)
     * Board đã có R93 (100kΩ pull-up ngoài), phải tắt pull-down nội để
     * không tranh với pull-up ngoài. Nếu để 0x27 (thiếu bit3), internal
     * pull-down ~100kΩ sẽ kéo đường về 0 dù chưa nhấn nút.
     */
    ctrl_base[conf_lcd_data2 / 4] = 0x7 | (1 << 5) | (1 << 3);  /* = 0x2F */

    /* Debug: đọc lại giá trị thực tế sau khi ghi, in từng bit 0-7 */
    uint32_t pad_val = ctrl_base[conf_lcd_data2 / 4];
    printf("conf_lcd_data2 = 0x%08X\n", pad_val);
    printf("  bit[2:0] MUXMODE   = %d  (expect 7)\n", (pad_val >> 0) & 0x7);
    printf("  bit[3]   PUDEN     = %d  (expect 1 = pull disabled)\n", (pad_val >> 3) & 1);
    printf("  bit[4]   PUTYPESEL = %d  (0=pull-down, 1=pull-up)\n", (pad_val >> 4) & 1);
    printf("  bit[5]   RXACTIVE  = %d  (expect 1 = input enabled)\n", (pad_val >> 5) & 1);
    printf("  bit[6]   SLEWCTRL  = %d\n", (pad_val >> 6) & 1);


    // config gpio0
    volatile uint32_t* gpio0_base = (volatile uint32_t*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO2_PHY_ADD);
    if(gpio0_base == MAP_FAILED){
        perror("mmap");
        munmap((void *)ctrl_base, PAGE_SIZE * 32);
        munmap((void *)clk_base, PAGE_SIZE);
        close(fd);
        return -1;
    }
    // gpio0_base[GPIO_0E / 4] |= (1 << 7);
    uint32_t oe = gpio0_base[GPIO_0E / 4];
    oe |= (1 << 8);          // 1 = input trên AM335x OE
    gpio0_base[GPIO_0E / 4] = oe;

    int time = 0;
    while (time != 10000)
    {
        printf("S2 status %d\n", ((gpio0_base[GPIO_DATAIN / 4] >> 8) & 1));
        ++time;
        usleep(2000);
    }
    


    munmap((void *)gpio0_base, PAGE_SIZE);
    munmap((void *)ctrl_base, PAGE_SIZE * 32);
    munmap((void *)clk_base, PAGE_SIZE);
    close(fd);


    return 0;
}