# Bài 13 - Interrupt Controller (INTC): ARM Interrupt Model và ISR trong Linux

## 1. Mục tiêu bài học

- Hiểu mô hình xử lý ngắt (interrupt) trên ARM Cortex-A8
- Nắm vững kiến trúc INTC (Interrupt Controller) trên AM335x
- Hiểu cách Linux kernel quản lý interrupt: IRQ number, interrupt descriptor
- Biết cách đăng ký ISR trong kernel module với `request_irq()`
- Hiểu khái niệm top half / bottom half trong Linux interrupt handling

---

## 2. Mô hình Interrupt trên ARM Cortex-A8

ARM Cortex-A8 có hai loại ngoại lệ liên quan đến interrupt:

| Exception | Mode | Vector | Dùng cho |
|-----------|------|--------|---------|
| **IRQ**   | IRQ mode | `0x18` | Interrupt thông thường (hầu hết thiết bị) |
| **FIQ**   | FIQ mode | `0x1C` | Fast Interrupt (ưu tiên cao, ít dùng trên Linux) |

```
Thiết bị                INTC                  ARM CPU
GPIO, UART, ... → Level/Edge detect → Prioritize → IRQ/FIQ → Exception
                        │
                  128 interrupt channels
                  Priority 0..127
```

> **Nguồn**: spruh73q.pdf, Chapter 6 (INTC — Interrupt Controller)

---

## 3. INTC trên AM335x

### 3.1 Tổng quan

- **128 interrupt lines** vào INTC
- Mỗi interrupt có **priority** (0 = cao nhất, 63 = thấp nhất)
- Hỗ trợ **preemption** (interrupt có priority cao hơn có thể ngắt interrupt đang xử lý)
- Hai ngõ ra: **IRQ** và **FIQ**

**Base address INTC**: `0x48200000`

> **Nguồn**: spruh73q.pdf, Table 2-1 (INTC at L4_MAIN)

### 3.2 Bảng Interrupt số quan trọng

| IRQ # | Nguồn |
|-------|-------|
| 32    | GPIO0_IRQn (bank 0) |
| 33    | GPIO0_IRQn (bank 1) |
| 62    | GPIO1_IRQn |
| 32+N  | Xem bảng đầy đủ tại spruh73q.pdf |
| 72    | UART0 |
| 73    | UART1 |
| 70    | I2C0 |
| 71    | I2C1 |
| 65    | SPI0 |
| 45    | DMTIMER2 |
| 46    | DMTIMER3 |
| 13    | eHRPWM0 |

> **Nguồn**: spruh73q.pdf, Table 6-3 (Interrupt Mapping)

### 3.3 Thanh ghi INTC chính

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `INTC_SYSCONFIG`    | `0x010` | Idle mode |
| `INTC_SYSSTATUS`    | `0x014` | Reset done |
| `INTC_SIR_IRQ`      | `0x040` | Active IRQ number hiện tại |
| `INTC_SIR_FIQ`      | `0x044` | Active FIQ number |
| `INTC_CONTROL`      | `0x048` | NewIRQAgr / NewFIQAgr (clear IRQ) |
| `INTC_PROTECTION`   | `0x04C` | Protection mode |
| `INTC_IDLE`         | `0x050` | Clock auto-idle |
| `INTC_IRQ_PRIORITY` | `0x060` | Priority của active IRQ |
| `INTC_ILRn`         | `0x100+4n` | Routing và priority cho interrupt n (n=0..127) |
| `INTC_MIRn`         | `0x080+...` | Mask register (set = mask = disable) |
| `INTC_MIR_CLEARn`   | `...` | Clear mask (enable interrupt) |
| `INTC_ISRSETn`      | `...` | Software trigger |
| `INTC_PENDING_IRQn` | `...` | Pending interrupt bits |

> **Nguồn**: spruh73q.pdf, Table 6-4 (INTC Register Map)

---

## 4. Mô hình Interrupt trong Linux Kernel

Trong Linux, không truy cập INTC trực tiếp từ driver. Kernel quản lý toàn bộ qua:

```
Hardware IRQ (GPIO line) 
    ↓
interrupt descriptor table (irq_desc)
    ↓
request_irq() đăng ký handler 
    ↓
ISR (Interrupt Service Routine)
```

### 4.1 Linux IRQ Number vs Hardware IRQ

Linux dùng **virtual IRQ number** (virq) khác với hardware IRQ number.

```c
/* Lấy IRQ number từ GPIO number */
int gpio_irq_num = gpio_to_irq(gpio_number);

/* Hoặc từ device tree node */
int irq_num = platform_get_irq(pdev, 0);
```

### 4.2 Top Half và Bottom Half

Khi interrupt xảy ra, Linux chia thành 2 giai đoạn:

```
Interrupt arrives
    │
    ▼
Top Half (ISR) — chạy ngay, KHÔNG được sleep, nhanh và ngắn
    │ - Đọc trạng thái hardware
    │ - Clear interrupt flag
    │ - Schedule bottom half
    ▼
Bottom Half — chạy sau (có thể sleep)
    ├── tasklet: không sleep, priority cao
    ├── workqueue: có thể sleep, chạy trong kernel thread
    └── softirq: network, block I/O (không dùng trực tiếp)
```

---

## 5. Code - Đăng ký Interrupt trong Kernel Module

```c
/*
 * gpio_irq_module.c
 * Kernel module đăng ký interrupt cho GPIO1_21 (USR0 button hoặc GPIO ngoài)
 *
 * Build: xem Makefile đi kèm
 * Load: sudo insmod gpio_irq_module.ko
 * Kiểm tra: dmesg | tail -20
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#define GPIO_NUM    49      /* GPIO1_17 = 32+17 = 49 hoặc dùng nút S2 */
#define GPIO_DESC   "gpio_irq_demo"

static int irq_number;
static int press_count = 0;

/* === Top Half ISR === */
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
    press_count++;
    printk(KERN_INFO "GPIO IRQ: nhấn #%d (GPIO%d)\n", press_count, GPIO_NUM);
    return IRQ_HANDLED;
}

static int __init gpio_irq_init(void)
{
    int ret;

    /* Request GPIO */
    ret = gpio_request(GPIO_NUM, GPIO_DESC);
    if (ret) {
        printk(KERN_ERR "Không thể request GPIO%d: %d\n", GPIO_NUM, ret);
        return ret;
    }

    /* Set direction: input */
    ret = gpio_direction_input(GPIO_NUM);
    if (ret) {
        printk(KERN_ERR "Không thể set GPIO%d input: %d\n", GPIO_NUM, ret);
        gpio_free(GPIO_NUM);
        return ret;
    }

    /* Lấy IRQ number tương ứng với GPIO */
    irq_number = gpio_to_irq(GPIO_NUM);
    if (irq_number < 0) {
        printk(KERN_ERR "Không lấy được IRQ cho GPIO%d\n", GPIO_NUM);
        gpio_free(GPIO_NUM);
        return irq_number;
    }

    printk(KERN_INFO "GPIO%d → IRQ%d\n", GPIO_NUM, irq_number);

    /* Đăng ký IRQ handler: falling edge (nút nhấn) */
    ret = request_irq(irq_number, gpio_irq_handler,
                      IRQF_TRIGGER_FALLING,
                      GPIO_DESC, NULL);
    if (ret) {
        printk(KERN_ERR "request_irq thất bại: %d\n", ret);
        gpio_free(GPIO_NUM);
        return ret;
    }

    printk(KERN_INFO "GPIO IRQ module loaded. GPIO%d sẵn sàng.\n", GPIO_NUM);
    return 0;
}

static void __exit gpio_irq_exit(void)
{
    free_irq(irq_number, NULL);
    gpio_free(GPIO_NUM);
    printk(KERN_INFO "GPIO IRQ module unloaded. Tổng số lần nhấn: %d\n", press_count);
}

module_init(gpio_irq_init);
module_exit(gpio_irq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Learner");
MODULE_DESCRIPTION("GPIO Interrupt Demo Module");
```

### 5.1 Makefile cho kernel module

```makefile
# Makefile
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m := gpio_irq_module.o

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
```

### Giải thích chi tiết từng dòng code `gpio_irq_module.c`

#### a) Header và API kernel

```c
#include <linux/module.h>     // module_init(), module_exit(), MODULE_LICENSE
#include <linux/kernel.h>     // printk(), KERN_INFO, KERN_ERR
#include <linux/init.h>       // __init, __exit — đánh dấu hàm chỉ dùng khi load/unload
#include <linux/gpio.h>       // gpio_request(), gpio_direction_input(), gpio_to_irq()
#include <linux/interrupt.h>   // request_irq(), free_irq(), IRQF_TRIGGER_FALLING
```

> Khác với userspace: kernel module KHÔNG dùng `stdio.h`, `stdlib.h`... mà dùng API kernel riêng.

#### b) `gpio_irq_init()` — trình tự đăng ký interrupt

```c
ret = gpio_request(GPIO_NUM, GPIO_DESC);
// Yêu cầu kernel dành riêng GPIO 49 cho module này
// Nếu GPIO đã bị module/driver khác chiếm → trả lỗi
```

```c
ret = gpio_direction_input(GPIO_NUM);
// Cấu hình GPIO là input (vì ta muốn nhận sự kiện nút nhấn)
// Kernel tự ghi thanh ghi GPIO_OE tương ứng
```

```c
irq_number = gpio_to_irq(GPIO_NUM);
// Chuyển GPIO number (49) → Linux virtual IRQ number
// Ví dụ: GPIO 49 → virq 200 (số cụ thể khác nhau tùy kernel)
// Linux KHÔNG dùng hardware IRQ trực tiếp — dùng virtual IRQ để
// trừ tượng hóa và hỗ trợ nhiều interrupt controller khác nhau
```

```c
ret = request_irq(irq_number, gpio_irq_handler,
                  IRQF_TRIGGER_FALLING,
                  GPIO_DESC, NULL);
// irq_number: virtual IRQ từ gpio_to_irq()
// gpio_irq_handler: con trỏ hàm ISR (top half)
// IRQF_TRIGGER_FALLING: trigger khi tín hiệu HIGH→LOW (nhấn nút)
//   Các flag khác: IRQF_TRIGGER_RISING, IRQF_TRIGGER_BOTH
// GPIO_DESC: tên (hiển thị trong /proc/interrupts)
// NULL: dev_id — dùng khi share interrupt, ở đây không cần
```

#### c) ISR (Top Half)

```c
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
    press_count++;      // Đếm số lần nhấn (lưu ý: không atomic, OK cho demo)
    printk(KERN_INFO ...);
    return IRQ_HANDLED;  // Báo kernel: interrupt đã được xử lý
                         // Nếu trả IRQ_NONE: kernel biết ISR này không phải chủ interrupt
}
```

> **Quy tắc ISR**: KHÔNG được sleep, KHÔNG malloc, KHÔNG mutex_lock. Chỉ làm việc tối thiểu rồi thoát nhanh. Nếu cần xử lý nặng → `schedule_work()` (bottom half).

#### d) Cleanup khi unload module

```c
free_irq(irq_number, NULL);  // Hủy đăng ký interrupt handler
gpio_free(GPIO_NUM);          // Trả GPIO về cho hệ thống
// Thứ tự quan trọng: free_irq TRƯỚC rồi mới gpio_free
// Nếu ngược lại, interrupt có thể fire khi GPIO đã bị giải phóng
```

---

## 6. Dùng Workqueue cho Bottom Half

```c
#include <linux/workqueue.h>

static struct work_struct my_work;

/* Bottom half: chạy trong process context, có thể sleep */
static void work_handler(struct work_struct *work)
{
    /* Có thể sleep, làm việc nặng ở đây */
    printk(KERN_INFO "Bottom half: xử lý sự kiện GPIO\n");
    msleep(10);  /* OK vì đây là workqueue */
}

/* Trong ISR (top half) — không được sleep */
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
    /* Chỉ làm việc tối thiểu */
    schedule_work(&my_work);  /* schedule bottom half */
    return IRQ_HANDLED;
}

/* Trong init: */
INIT_WORK(&my_work, work_handler);
```

---

## 7. Câu hỏi kiểm tra

1. Tại sao ISR (top half) không được gọi `sleep()` hay `msleep()`?
2. `gpio_to_irq()` trả về giá trị gì? Tại sao Linux không dùng trực tiếp hardware IRQ number?
3. Khi dùng `request_irq()`, flag `IRQF_TRIGGER_FALLING` có nghĩa là gì?
4. Workqueue và tasklet khác nhau ở điểm nào quan trọng nhất?
5. Nếu ISR trả về `IRQ_NONE`, điều gì xảy ra?

---

## 8. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| INTC overview | spruh73q.pdf | Chapter 6, Section 6.1 |
| INTC register map | spruh73q.pdf | Table 6-4 |
| Interrupt mapping table | spruh73q.pdf | Table 6-3 |
| INTC base address | spruh73q.pdf | Table 2-1 |
| Linux request_irq | Linux Kernel API | `include/linux/interrupt.h` |
