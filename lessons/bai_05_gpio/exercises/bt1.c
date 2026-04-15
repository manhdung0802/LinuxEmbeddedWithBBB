#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>

#define PAGE_SIZE 0x1000
#define GPIO1_PHY_ADD 0x4804C000
#define GPIO2_PHY_ADD 0x481AC000
#define GPIO_0E 0x134
#define GPIO_DATAIN 0x138

#define CM_PER_PHY_ADD 0x44E00000
#define CM_PER_GPIO1_CLKCTRL 0xAC
#define CM_PER_GPIO2_CLKCTRL 0xB0

#define CONTROL_MODULE_PHY_ADD 0x44E10000
#define conf_lcd_data2 0x8A8
#define conf_lcd_data3 0x8AC
#define gpmc_clk_mux0 0x88C

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
    clk_base[(CM_PER_GPIO2_CLKCTRL) / 4] = 0x2 | (1 << 18);
    // uint32_t tempClk = clk_base[(CM_PER_GPIO1_CLKCTRL) / 4];
    while (((clk_base[(CM_PER_GPIO2_CLKCTRL) / 4] >> 16) & 0x11) != 0x0)
    {
        printf("Setting up clock\n");
    }
    printf("Finish clock\n");
    


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
     * MUX_MODE7 | INPUT_EN (bit5) | PULL_UP (bit4) | PULL_ENABLE (bit3=0)
     * Đối với nút nhấn ngoài gắn vào P8_11 (không có điện trở kéo ngoài),
     * ta phải BẬT điện trở kéo lên nội (internal pull-up).
     * Cấu hình: 0x37
     */
    ctrl_base[conf_lcd_data2 / 4] = 0x7 | (1<<4) | (1 << 5);
    ctrl_base[gpmc_clk_mux0 / 4] = 0x7 | (1<<4) | (1 << 5);

    /* Debug: đọc lại giá trị thực tế sau khi ghi, in từng bit 0-7 */
    uint32_t pad_val = ctrl_base[conf_lcd_data2 / 4];
    printf("conf_lcd_data2 = 0x%08X\n", pad_val);
    printf("  bit[2:0] MUXMODE   = %d  (expect 7)\n", (pad_val >> 0) & 0x7);
    printf("  bit[3]   PUDEN     = %d  (expect 1 = pull disabled)\n", (pad_val >> 3) & 1);
    printf("  bit[4]   PUTYPESEL = %d  (0=pull-down, 1=pull-up)\n", (pad_val >> 4) & 1);
    printf("  bit[5]   RXACTIVE  = %d  (expect 1 = input enabled)\n", (pad_val >> 5) & 1);
    printf("  bit[6]   SLEWCTRL  = %d\n", (pad_val >> 6) & 1);

    pad_val = ctrl_base[gpmc_clk_mux0 / 4];
    printf("gpmc_clk_mux0 = 0x%08X\n", pad_val);
    printf("  bit[2:0] MUXMODE   = %d  (expect 7)\n", (pad_val >> 0) & 0x7);
    printf("  bit[3]   PUDEN     = %d  (expect 1 = pull disabled)\n", (pad_val >> 3) & 1);
    printf("  bit[4]   PUTYPESEL = %d  (0=pull-down, 1=pull-up)\n", (pad_val >> 4) & 1);
    printf("  bit[5]   RXACTIVE  = %d  (expect 1 = input enabled)\n", (pad_val >> 5) & 1);
    printf("  bit[6]   SLEWCTRL  = %d\n", (pad_val >> 6) & 1);


    volatile uint32_t* gpio0_base = (volatile uint32_t*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO2_PHY_ADD);
    if(gpio0_base == MAP_FAILED){
        perror("mmap");
        munmap((void *)ctrl_base, PAGE_SIZE * 32);
        munmap((void *)clk_base, PAGE_SIZE);
        close(fd);
        return -1;
    }
    uint32_t oe = gpio0_base[GPIO_0E / 4];
    oe |= (1 << 8);          // 1 = input trên AM335x OE cho bit 13
    oe |= (1 << 1);
    gpio0_base[GPIO_0E / 4] = oe;
    printf("P8_43 oe %d\n", (gpio0_base[GPIO_0E / 4] >> 8) & 0x01);
    printf("P8_18 oe %d\n", (gpio0_base[GPIO_0E / 4] >> 1) & 0x01);

    int time = 0;
    int last_status = -1;
    int last_status44 = -1;
    while (time != 10000)
    {
        int current_status = (gpio0_base[GPIO_DATAIN / 4] >> 8) & 1;
        if (current_status != last_status) {
            printf("P8_43 status %d\n", current_status);
            last_status = current_status;
        }
        int current_status44 = (gpio0_base[GPIO_DATAIN / 4] >> 1) & 1;
        if (current_status44 != last_status44) {
            printf("P8_18 status %d\n", current_status44);
            last_status44 = current_status44;
        }
        ++time;
        usleep(2000);
    }
    


    munmap((void *)gpio0_base, PAGE_SIZE);
    munmap((void *)ctrl_base, PAGE_SIZE * 32);
    munmap((void *)clk_base, PAGE_SIZE);
    close(fd);


    return 0;
}