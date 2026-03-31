# Bài 19 - Platform Driver & Device Tree Binding

## 1. Mục tiêu bài học

- Hiểu mô hình Platform Driver trong Linux kernel
- Viết driver với `probe()` và `remove()` lifecycle
- Bind driver với Device Tree node qua `compatible` và `of_match_table`
- Dùng `devm_*` managed resources để tránh memory leak
- Kết hợp DT overlay (bài 16) + char device (bài 18) + platform driver (bài 19)

---

## 2. Platform Driver là gì?

**Platform driver** là mô hình driver cho thiết bị **không tự discovery** được (không phải USB hay PCI — các bus này có auto-enumerate).

AM335x peripherals (GPIO, UART, I2C, SPI...) đều là platform devices vì:
- Không có bus tự enumerate
- Địa chỉ và interrupt cố định trong SoC
- Được mô tả trong Device Tree

```
Device Tree (DTS)           Kernel Driver
────────────────            ──────────────────────
uart1: serial@48022000 {    platform_driver uart_driver {
    compatible =     ─────►     .of_match_table = uart_ids
        "ti,am3352-uart"        .probe  = uart_probe
    reg = <...>                 .remove = uart_remove
    interrupts = <...>      }
    status = "okay"
}
```

---

## 3. struct platform_driver

```c
static struct platform_driver my_driver = {
    .probe  = my_probe,    /* gọi khi tìm thấy matching DT node */
    .remove = my_remove,   /* gọi khi driver unload hoặc device removed */
    .driver = {
        .name  = "my_platform_dev",     /* tên driver */
        .owner = THIS_MODULE,
        .of_match_table = my_of_ids,    /* DT compatible strings */
    },
};

/* Đăng ký và hủy tự động khi module init/exit */
module_platform_driver(my_driver);
```

---

## 4. Device Tree Binding

### 4.1 Compatible String

Chuỗi `compatible` trong DT node phải khớp với `of_match_table` trong driver.

**DTS node**:
```dts
myled@0 {
    compatible = "bbb-tutorial,myled";  /* vendor,device format */
    reg = <0x4804C000 0x1000>;          /* GPIO1 base, size */
    my-gpio-pin = <21>;                 /* custom property */
    status = "okay";
};
```

**Driver of_match_table**:
```c
static const struct of_device_id my_of_ids[] = {
    { .compatible = "bbb-tutorial,myled" },  /* phải khớp DTS */
    { }  /* sentinel - kết thúc array */
};
MODULE_DEVICE_TABLE(of, my_of_ids);
```

---

## 5. Ví dụ: LED Platform Driver với Device Tree

### 5.1 DTS Overlay

```dts
/* bbb-myled-overlay.dts */
/dts-v1/;
/plugin/;

/ {
    compatible = "ti,beaglebone-black";
    part-number = "BBB-MYLED";
    version = "00A0";

    fragment@0 {
        /* Thêm node vào root của device tree */
        target-path = "/";
        __overlay__ {
            myled@4804C000 {
                compatible = "bbb-tutorial,myled";
                reg = <0x4804C000 0x1000>;  /* GPIO1 */
                my-gpio-pin = <21>;          /* GPIO1_21 = USR0 LED */
                status = "okay";
            };
        };
    };
};
```

### 5.2 Platform Driver Code

```c
/*
 * myled_platform_driver.c
 * Platform driver điều khiển LED qua Device Tree + ioremap
 */
#include <linux/module.h>
#include <linux/platform_device.h>  /* platform_driver, platform_device */
#include <linux/of.h>               /* of_property_read_u32, of_device_id */
#include <linux/of_device.h>
#include <linux/io.h>               /* devm_ioremap_resource */
#include <linux/resource.h>         /* struct resource */

#define GPIO_OE           0x134
#define GPIO_SETDATAOUT   0x194
#define GPIO_CLEARDATAOUT 0x190

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Student");
MODULE_DESCRIPTION("LED Platform Driver with DT Binding");

/* Per-device private data (mỗi instance của device có struct này) */
struct myled_data {
    void __iomem *base;     /* virtual address của GPIO base */
    u32 gpio_pin;           /* pin number đọc từ DT */
    struct device *dev;
};

/*
 * probe() - gọi khi kernel tìm thấy DT node có compatible khớp
 * pdev: platform device chứa thông tin từ DT (reg, interrupts...)
 */
static int myled_probe(struct platform_device *pdev)
{
    struct myled_data *data;
    struct resource *res;
    u32 gpio_pin;
    int ret;

    dev_info(&pdev->dev, "myled: probe called\n");

    /* 1. Đọc property "my-gpio-pin" từ DT */
    ret = of_property_read_u32(pdev->dev.of_node, "my-gpio-pin", &gpio_pin);
    if (ret) {
        dev_err(&pdev->dev, "Missing 'my-gpio-pin' in DT\n");
        return -EINVAL;
    }
    dev_info(&pdev->dev, "GPIO pin from DT: %u\n", gpio_pin);

    /* 2. Allocate private data
     * devm_kzalloc: tự động free khi device bị remove */
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->gpio_pin = gpio_pin;
    data->dev = &pdev->dev;

    /* 3. Lấy memory resource từ DT (reg property) */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "No memory resource in DT\n");
        return -ENODEV;
    }

    /* 4. ioremap resource
     * devm_ioremap_resource: tự động iounmap khi remove */
    data->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(data->base))
        return PTR_ERR(data->base);

    /* 5. Config GPIO pin làm output */
    u32 oe = ioread32(data->base + GPIO_OE);
    oe &= ~(1 << gpio_pin);   /* 0 = output */
    iowrite32(oe, data->base + GPIO_OE);

    /* 6. Bật LED */
    iowrite32(1 << gpio_pin, data->base + GPIO_SETDATAOUT);

    /* 7. Lưu private data vào platform device (dùng lại trong remove) */
    platform_set_drvdata(pdev, data);

    dev_info(&pdev->dev, "myled: LED GPIO1_%u turned ON\n", gpio_pin);
    return 0;
}

/*
 * remove() - gọi khi driver unload hoặc device bị remove từ DT
 */
static int myled_remove(struct platform_device *pdev)
{
    struct myled_data *data = platform_get_drvdata(pdev);

    /* Tắt LED */
    iowrite32(1 << data->gpio_pin, data->base + GPIO_CLEARDATAOUT);

    /* devm_* resources tự động freed - không cần gọi kzfree, iounmap */
    dev_info(&pdev->dev, "myled: removed, LED OFF\n");
    return 0;
}

/* Compatible strings phải khớp với DT node */
static const struct of_device_id myled_of_ids[] = {
    { .compatible = "bbb-tutorial,myled" },
    { }
};
MODULE_DEVICE_TABLE(of, myled_of_ids);

static struct platform_driver myled_driver = {
    .probe  = myled_probe,
    .remove = myled_remove,
    .driver = {
        .name           = "myled",
        .owner          = THIS_MODULE,
        .of_match_table = myled_of_ids,
    },
};

/* Macro này thay thế module_init/module_exit cho platform driver */
module_platform_driver(myled_driver);
```

### Giải thích chi tiết từng dòng code

a) **Header includes**:
- `<linux/platform_device.h>` — cung cấp `struct platform_driver`, `platform_get_resource()`, `platform_set_drvdata()`. Core API cho platform driver.
- `<linux/of.h>` — Device Tree API: `of_property_read_u32()`, `struct of_device_id`. "of" = Open Firmware (tiền thân của Device Tree).
- `<linux/io.h>` — `devm_ioremap_resource()`, `ioread32()`, `iowrite32()`.
- `<linux/resource.h>` — `struct resource` chứa thông tin memory range (start, end, flags).

b) **`struct myled_data` — per-device private data**:
```c
struct myled_data {
    void __iomem *base;     /* virtual address GPIO base */
    u32 gpio_pin;           /* pin number từ DT */
    struct device *dev;
};
```
- Mỗi **instance** của device (mỗi DT node khớp compatible) có 1 `myled_data` riêng.
- Pattern này cho phép 1 driver phục vụ **nhiều device** cùng lúc (ví dụ: 2 LED trên 2 GPIO khác nhau).

c) **`myled_probe()` — hàm cốt lõi**:
- **Khi nào được gọi?** Khi kernel scan Device Tree, tìm thấy node có `compatible = "bbb-tutorial,myled"` khớp với `of_match_table` → gọi `probe()`.
- `struct platform_device *pdev` — chứa mọi thông tin từ DT node: reg, interrupts, properties.

d) **Bước 1: Đọc DT property**:
```c
of_property_read_u32(pdev->dev.of_node, "my-gpio-pin", &gpio_pin);
```
- `pdev->dev.of_node` — con trỏ đến DT node tương ứng.
- Đọc property `my-gpio-pin = <21>` từ DTS, lưu vào `gpio_pin`.
- Trả về `0` = OK, khác 0 = property không tồn tại → driver báo lỗi và return `-EINVAL`.

e) **Bước 2: `devm_kzalloc()`**:
```c
data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
```
- `devm_` prefix = **device-managed** — kernel tự `kfree()` khi device bị remove. **Không cần** gọi `kfree()` thủ công.
- `kzalloc` = `kmalloc` + `memset(0)` — cấp phát bộ nhớ kernel đã zero-initialized.
- `GFP_KERNEL` = flag cho phép sleep trong khi chờ bộ nhớ (chỉ dùng trong process context, **không dùng trong interrupt**).

f) **Bước 3-4: Lấy và map memory resource**:
```c
res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
data->base = devm_ioremap_resource(&pdev->dev, res);
```
- `platform_get_resource(pdev, IORESOURCE_MEM, 0)` — lấy entry đầu tiên (index 0) của `reg` property trong DT. Với `reg = <0x4804C000 0x1000>`, trả về `res->start = 0x4804C000`, `res->end = 0x4804CFFF`.
- `devm_ioremap_resource()` — kết hợp `request_mem_region()` + `ioremap()` trong 1 lệnh. Tự động `iounmap()` khi remove.
- `IS_ERR(data->base)` — kiểm tra lỗi theo convention kernel: pointer lỗi được encode thành giá trị âm trong vùng địa chỉ cao.

g) **Bước 5-6: Config GPIO và bật LED**:
```c
u32 oe = ioread32(data->base + GPIO_OE);
oe &= ~(1 << gpio_pin);
iowrite32(oe, data->base + GPIO_OE);
iowrite32(1 << gpio_pin, data->base + GPIO_SETDATAOUT);
```
- Giống bài 17 (gpio_module.c), nhưng dùng `data->base` thay vì global variable → mỗi device instance có base riêng.

h) **`platform_set_drvdata(pdev, data)`**:
- Lưu con trỏ `data` vào `pdev->dev.driver_data`.
- Trong `remove()`, gọi `platform_get_drvdata(pdev)` để lấy lại pointer → pattern truyền data giữa `probe()` và `remove()`.

i) **`myled_remove()` — cleanup**:
- Chỉ cần tắt LED (`iowrite32` vào CLEARDATAOUT).
- **Không cần** `kfree(data)`, `iounmap(base)` — tất cả `devm_*` resource được tự động free.

j) **`of_match_table` và `module_platform_driver()`**:
```c
static const struct of_device_id myled_of_ids[] = {
    { .compatible = "bbb-tutorial,myled" },
    { }  /* sentinel */
};
```
- Array kết thúc bằng `{ }` (sentinel entry) — kernel biết đến đây là hết danh sách.
- `MODULE_DEVICE_TABLE(of, myled_of_ids)` — export thông tin compatible vào module info, cho phép `modprobe` tự load driver đúng.
- `module_platform_driver(myled_driver)` — macro mở rộng thành `module_init` + `module_exit` đăng ký/hủy platform driver. Giảm boilerplate code.

> **Bài học**: Platform driver kết hợp 3 thành phần: **DTS node** (mô tả hardware), **of_match_table** (matching rule), **probe/remove** (lifecycle). Dùng `devm_*` API để kernel tự quản lý resource → code ngắn gọn, ít bug.

---

## 6. devm_* API — Managed Resources

`devm_*` resources được tự động giải phóng khi device bị remove → tránh resource leak:

| Thường | devm_* tương đương |
|--------|-------------------|
| `kzalloc(size, GFP_KERNEL)` | `devm_kzalloc(dev, size, GFP_KERNEL)` |
| `ioremap(addr, size)` | `devm_ioremap(dev, addr, size)` |
| `ioremap_resource(res)` | `devm_ioremap_resource(dev, res)` |
| `request_irq(irq, handler, ...)` | `devm_request_irq(dev, irq, handler, ...)` |
| `clk_get(dev, id)` | `devm_clk_get(dev, id)` |

**Lợi ích**: viết `probe()` ngắn hơn, không cần `goto err_cleanup` dài dòng.

---

## 7. Đọc Các Property DT Thông Dụng

```c
/* Đọc số nguyên 32-bit */
u32 val;
of_property_read_u32(node, "clock-frequency", &val);

/* Đọc mảng u32 */
u32 arr[2];
of_property_read_u32_array(node, "reg", arr, 2);

/* Đọc string */
const char *str;
of_property_read_string(node, "label", &str);

/* Kiểm tra boolean property (key có tồn tại không) */
if (of_property_read_bool(node, "gpio-controller"))
    pr_info("This is a GPIO controller\n");

/* Lấy interrupt number từ DT */
int irq = platform_get_irq(pdev, 0);  /* index 0 = first interrupt */
```

---

## 8. Câu hỏi kiểm tra

1. `probe()` được gọi khi nào? `remove()` được gọi khi nào?
2. Tại sao dùng `devm_kzalloc()` thay vì `kzalloc()` trong platform driver?
3. `platform_get_resource(pdev, IORESOURCE_MEM, 0)` lấy thông tin từ đâu trong DT?
4. `platform_set_drvdata()` và `platform_get_drvdata()` dùng để làm gì?
5. Nếu DT node có `status = "disabled"`, `probe()` có được gọi không?

---

## 9. Tài liệu tham khảo

| Nội dung | Header | Ghi chú |
|----------|--------|---------|
| platform_driver | linux/platform_device.h | probe, remove, driver |
| DT property read | linux/of.h | of_property_read_u32, of_match_table |
| Managed resources | linux/device.h | devm_kzalloc, devm_ioremap |
| GPIO1 registers | spruh73q.pdf, Section 25.4 | OE, SETDATAOUT, CLEARDATAOUT |
| DT node format | spruh73q.pdf + BBB kernel DTS | am335x-bone-common.dtsi |
