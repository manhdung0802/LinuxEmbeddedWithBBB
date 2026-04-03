# Bài 6 - Memory-Mapped I/O trong Kernel

## 1. Mục tiêu bài học
- Hiểu sự khác nhau giữa MMIO trong userspace (`/dev/mem`) và kernel space
- Sử dụng `ioremap()` / `iounmap()` để map physical address vào kernel virtual address
- Dùng `readl()` / `writel()` để đọc/ghi thanh ghi an toàn
- Dùng `platform_get_resource()` để lấy address từ Device Tree
- Viết module điều khiển GPIO bằng MMIO trực tiếp trong kernel

---

## 2. MMIO trong Kernel vs Userspace

### 2.1 Userspace (bài trước):
```c
/* Userspace: dùng mmap() trên /dev/mem */
fd = open("/dev/mem", O_RDWR);
ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, phys_addr);
*ptr = value;  /* Ghi trực tiếp */
```

**Nhược điểm**: Không an toàn, bypass kernel, xung đột driver.

### 2.2 Kernel space (bài này):
```c
/* Kernel: dùng ioremap() */
void __iomem *base = ioremap(phys_addr, size);
writel(value, base + offset);  /* Ghi qua API kernel */
iounmap(base);
```

**Ưu điểm**: An toàn, đúng kiến trúc Linux, tương thích mọi platform.

### 2.3 So sánh:

| Tiêu chí | Userspace `/dev/mem` | Kernel `ioremap` |
|-----------|---------------------|-------------------|
| An toàn | Thấp | Cao |
| Hiệu năng | Tốt | Tốt |
| Xung đột driver | Có thể | Không (nếu dùng request_mem_region) |
| Phù hợp | Debug, học tập | Driver chính thức |
| Cần quyền | root + /dev/mem | Kernel module |

---

## 3. `ioremap()` và `iounmap()`

### 3.1 ioremap:

```c
#include <linux/io.h>

/* Map physical address vào kernel virtual address */
void __iomem *ioremap(phys_addr_t phys_addr, size_t size);
```

- **phys_addr**: Địa chỉ vật lý (ví dụ `0x4804C000` cho GPIO1)
- **size**: Kích thước vùng nhớ (ví dụ `0x1000` = 4KB)
- **Return**: Con trỏ virtual, hoặc NULL nếu fail
- **`__iomem`**: Annotation cho sparse checker, đánh dấu "đây là I/O memory"

### 3.2 iounmap:

```c
/* Giải phóng mapping khi không cần nữa */
void iounmap(void __iomem *addr);
```

### 3.3 Ví dụ cơ bản:

```c
#define GPIO1_BASE  0x4804C000
#define GPIO1_SIZE  0x1000

void __iomem *gpio1_base;

/* Trong init */
gpio1_base = ioremap(GPIO1_BASE, GPIO1_SIZE);
if (!gpio1_base) {
    pr_err("ioremap failed for GPIO1\n");
    return -ENOMEM;
}

/* Trong exit */
iounmap(gpio1_base);
```

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 2 - Memory Map: GPIO1 base = 0x4804C000

---

## 4. `readl()` / `writel()` - Đọc/ghi thanh ghi

### 4.1 API:

```c
#include <linux/io.h>

/* Đọc 32-bit từ thanh ghi */
u32 readl(const volatile void __iomem *addr);

/* Ghi 32-bit vào thanh ghi */
void writel(u32 value, volatile void __iomem *addr);

/* Các kích thước khác */
u8  readb(addr);    void writeb(u8 val, addr);     /* 8-bit */
u16 readw(addr);    void writew(u16 val, addr);     /* 16-bit */
```

### 4.2 Tại sao không dùng pointer trực tiếp?

```c
/* SAI - không portable, compiler có thể optimize sai */
volatile u32 *reg = (u32 *)gpio1_base;
*reg = 0x123;

/* ĐÚNG - dùng readl/writel */
writel(0x123, gpio1_base + OFFSET);
```

**Lý do**:
- `readl/writel` đảm bảo memory barrier (ordering đúng)
- Portable giữa các kiến trúc (ARM, x86, MIPS, ...)
- Compiler không optimize sai thứ tự

### 4.3 Ví dụ đọc/ghi GPIO:

```c
#define GPIO_OE           0x134   /* Output Enable */
#define GPIO_DATAOUT      0x13C   /* Data Output */
#define GPIO_SETDATAOUT   0x194   /* Set bit */
#define GPIO_CLEARDATAOUT 0x190   /* Clear bit */

/* Đặt GPIO1_21 thành output */
u32 val = readl(gpio1_base + GPIO_OE);
val &= ~(1 << 21);                         /* Clear bit 21 = output */
writel(val, gpio1_base + GPIO_OE);

/* Bật LED (set bit 21) */
writel((1 << 21), gpio1_base + GPIO_SETDATAOUT);

/* Tắt LED (clear bit 21) */
writel((1 << 21), gpio1_base + GPIO_CLEARDATAOUT);
```

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 25 - GPIO Registers

---

## 5. `platform_get_resource()` - Lấy address từ Device Tree

Thay vì hardcode address, driver nên lấy từ Device Tree:

### 5.1 Device Tree node:

```dts
my_gpio: my-gpio@4804c000 {
    compatible = "my,gpio-driver";
    reg = <0x4804c000 0x1000>;
};
```

### 5.2 Driver code:

```c
#include <linux/platform_device.h>

static int my_probe(struct platform_device *pdev)
{
    struct resource *res;
    void __iomem *base;

    /* Lấy resource từ DT node "reg" */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "No memory resource\n");
        return -ENODEV;
    }
    /* res->start = 0x4804C000, res->end = 0x4804CFFF */

    /* Map vào kernel virtual address */
    base = ioremap(res->start, resource_size(res));
    if (!base) {
        dev_err(&pdev->dev, "ioremap failed\n");
        return -ENOMEM;
    }

    /* Dùng base để đọc/ghi thanh ghi... */

    return 0;
}
```

### 5.3 Hàm tiện ích `devm_ioremap_resource()`:

```c
/* Tự động ioremap + request_mem_region + cleanup khi remove */
base = devm_ioremap_resource(&pdev->dev, res);
if (IS_ERR(base))
    return PTR_ERR(base);
```

→ `devm_*` sẽ được giải thích chi tiết ở Bài 10.

---

## 6. `request_mem_region()` - Đánh dấu vùng nhớ đang dùng

```c
/* Đánh dấu vùng [start, start+size) thuộc về driver này */
if (!request_mem_region(phys_addr, size, "my_driver")) {
    pr_err("Memory region already in use\n");
    return -EBUSY;
}

/* Giải phóng khi exit */
release_mem_region(phys_addr, size);
```

**Tác dụng**: Ngăn 2 driver cùng truy cập 1 vùng nhớ → kiểm tra `/proc/iomem`:

```bash
cat /proc/iomem
# 44e07000-44e07fff : 44e07000.gpio
# 4804c000-4804cfff : 4804c000.gpio
```

---

## 7. Ví dụ hoàn chỉnh: GPIO LED module

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>

#define GPIO1_BASE        0x4804C000
#define GPIO1_SIZE        0x1000

/* Offset thanh ghi GPIO (từ TRM Chapter 25) */
#define GPIO_OE           0x134
#define GPIO_SETDATAOUT   0x194
#define GPIO_CLEARDATAOUT 0x190

#define LED_BIT           21    /* GPIO1_21 = USR0 LED */

static void __iomem *gpio1_base;

static int __init gpio_led_init(void)
{
	u32 val;

	/* Map GPIO1 registers vào kernel space */
	gpio1_base = ioremap(GPIO1_BASE, GPIO1_SIZE);
	if (!gpio1_base) {
		pr_err("gpio_led: ioremap failed\n");
		return -ENOMEM;
	}

	/* Đặt GPIO1_21 thành output (clear bit trong OE) */
	val = readl(gpio1_base + GPIO_OE);
	val &= ~(1 << LED_BIT);
	writel(val, gpio1_base + GPIO_OE);

	/* Bật LED */
	writel((1 << LED_BIT), gpio1_base + GPIO_SETDATAOUT);
	pr_info("gpio_led: LED ON (GPIO1_%d)\n", LED_BIT);

	return 0;
}

static void __exit gpio_led_exit(void)
{
	/* Tắt LED */
	writel((1 << LED_BIT), gpio1_base + GPIO_CLEARDATAOUT);
	pr_info("gpio_led: LED OFF\n");

	/* Giải phóng mapping */
	iounmap(gpio1_base);
}

module_init(gpio_led_init);
module_exit(gpio_led_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Direct MMIO GPIO LED driver for AM335x");
```

**Lưu ý**: Ví dụ này truy cập GPIO trực tiếp bằng MMIO, **không qua GPIO subsystem**. Đây chỉ để học. Driver thực tế nên dùng gpiolib (Bài 13-15).

---

## 8. Memory Barrier

### 8.1 Tại sao cần memory barrier?

ARM processor có thể **reorder** thứ tự đọc/ghi bộ nhớ. Khi ghi vào thanh ghi phần cứng, thứ tự rất quan trọng.

```c
/* Ví dụ: phải ghi config TRƯỚC khi enable */
writel(config, base + CONFIG_REG);    /* Bước 1 */
wmb();                                 /* Đảm bảo bước 1 xong */
writel(enable, base + ENABLE_REG);    /* Bước 2 */
```

### 8.2 Các barrier:

| Barrier | Tác dụng |
|---------|---------|
| `wmb()` | Write memory barrier - đảm bảo write trước nó hoàn tất |
| `rmb()` | Read memory barrier |
| `mb()` | Full memory barrier (cả read + write) |

**Lưu ý**: `writel()` đã có barrier tích hợp trên ARM. Thường không cần thêm barrier khi dùng `readl/writel`.

---

## 9. Kiến thức cốt lõi sau bài này

1. **`ioremap()`** map physical address → kernel virtual address
2. **`readl()`/`writel()`** là cách đúng để đọc/ghi thanh ghi trong kernel
3. **`platform_get_resource()`** lấy address từ Device Tree thay vì hardcode
4. **`request_mem_region()`** ngăn xung đột giữa các driver
5. **`devm_ioremap_resource()`** = combo tự động: request + ioremap + cleanup
6. **Memory barrier** đảm bảo thứ tự truy cập đúng

---

## 10. Câu hỏi kiểm tra

1. Sự khác nhau giữa `mmap()` trong userspace và `ioremap()` trong kernel?
2. Tại sao `writel()` an toàn hơn ghi trực tiếp qua pointer?
3. `platform_get_resource(pdev, IORESOURCE_MEM, 0)` trả về gì?
4. `request_mem_region()` dùng để làm gì? Kiểm tra ở đâu?
5. Viết code dùng `ioremap` + `readl` để đọc giá trị GPIO_DATAIN của GPIO0 (base `0x44E07000`).

---

## 11. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 7 - Character Device Driver cơ bản**

Ta sẽ học cách tạo device file `/dev/xxx` để userspace giao tiếp với driver qua `open`, `read`, `write`, `close`.
