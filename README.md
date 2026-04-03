# 📘 Ghi Chú Kiến Thức - AM335x Linux Device Driver

> File này được cập nhật sau mỗi buổi học để ôn tập lại kiến thức.
> Mỗi phần sẽ tóm tắt: khái niệm, kernel API, Device Tree, code mẫu driver.

---

## 📋 Mục lục
- [Giai đoạn 1: Nền tảng](#giai-đoạn-1-nền-tảng)
- [Giai đoạn 2: Character Device Driver](#giai-đoạn-2-character-device-driver)
- [Giai đoạn 3: GPIO & Pin Control](#giai-đoạn-3-gpio--pin-control)
- [Giai đoạn 4: Bus & Peripheral Drivers](#giai-đoạn-4-bus--peripheral-drivers)
- [Giai đoạn 5: Advanced Subsystems](#giai-đoạn-5-advanced-subsystems)
- [Giai đoạn 6: Tích hợp & Debug](#giai-đoạn-6-tích-hợp--debug)
- [Kernel API thường dùng](#kernel-api-thường-dùng)
- [Địa chỉ thanh ghi quan trọng](#địa-chỉ-thanh-ghi-quan-trọng)

---

## Giai đoạn 1: Nền tảng

### Bài 1 - Tổng quan AM335x SoC

- AM335x là một SoC: bên trong có CPU Cortex-A8, interconnect, bộ nhớ, clock/reset, interrupt và nhiều peripheral.
- Peripheral được điều khiển bằng memory-mapped I/O: CPU đọc/ghi vào địa chỉ nhớ đặc biệt để tác động phần cứng.
- Công thức cơ bản khi làm việc với thanh ghi:

```text
register_address = base_address + offset
```

- Khi debug một peripheral không hoạt động, luôn kiểm tra theo thứ tự:
	1. Clock của module
	2. Pinmux trong Control Module
	3. Đúng base address, offset và bit-field

- So sánh nhanh:
	- Bare-metal: truy cập trực tiếp phần cứng, timing tốt hơn, nhưng phải tự xử lý nhiều thứ.
	- Linux: có kernel quản lý driver và virtual memory, thuận tiện hơn cho hệ thống phức tạp.

### Bài 2 - Tổng quan phần cứng BeagleBone Black

- BBB là một board hoàn chỉnh, không chỉ là AM335x: có DDR3, eMMC, PMIC, Ethernet PHY, USB PHY, oscillator và header mở rộng P8/P9.
- Hai header P8/P9 là giao diện để đưa tín hiệu nội của AM335x ra ngoài board.
- Khái niệm quan trọng nhất là **pinmux**: một chân vật lý có thể đảm nhiệm nhiều chức năng (GPIO/UART/I2C/SPI/PWM...).
- Control Module (`0x44E10000`) là nơi cấu hình pad/pinmux trước khi dùng GPIO hoặc peripheral tương ứng.
- Với GPIO:
	- `GPIOx_y` nghĩa là module `x`, bit `y`
	- Mỗi module GPIO có vùng địa chỉ riêng và layout thanh ghi giống nhau
	- Các thanh ghi dữ liệu quan trọng: `GPIO_OE`, `GPIO_DATAIN`, `GPIO_DATAOUT`, `GPIO_SETDATAOUT`, `GPIO_CLEARDATAOUT`
- Trình tự kiểm tra khi thao tác chân ngoài:
	1. Tra đúng mapping chân trong bảng P8/P9
	2. Cấu hình pinmux đúng mode
	3. Bật clock module
	4. Đọc/ghi đúng thanh ghi và bit

### Bài 3–6 - Thiết lập môi trường, Device Tree, Kernel Module, MMIO

*(Sẽ được cập nhật khi học)*

---

## Giai đoạn 2: Character Device Driver

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 3: GPIO & Pin Control

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 4: Bus & Peripheral Drivers

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 5: Advanced Subsystems

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 6: Tích hợp & Debug

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Kernel API thường dùng

| API | Mô tả |
|-----|--------|
| `module_init()` / `module_exit()` | Đăng ký hàm init/cleanup cho kernel module |
| `platform_driver_register()` | Đăng ký platform driver |
| `devm_ioremap()` | Map physical address sang kernel virtual address (managed) |
| `readl()` / `writel()` | Đọc/ghi 32-bit register |
| `devm_request_irq()` | Đăng ký interrupt handler (managed) |
| `copy_to_user()` / `copy_from_user()` | Truyền data giữa kernel ↔ userspace |
| *(sẽ cập nhật thêm)* | |

---

## Địa chỉ thanh ghi quan trọng

| Module | Thanh ghi | Địa chỉ | Mô tả |
|--------|-----------|----------|--------|
| CM_PER | Base | 0x44E00000 | Clock module cho nhiều peripheral |
| GPIO0 | Base | 0x44E07000 | GPIO bank 0 |
| UART0 | Base | 0x44E09000 | UART console chính trên BBB |
| CONTROL MODULE | Base | 0x44E10000 | Pinmux và pad control |
| GPIO1 | Base | 0x4804C000 | GPIO bank 1 |
| GPIO2 | Base | 0x481AC000 | GPIO bank 2 |
| GPIO3 | Base | 0x481AE000 | GPIO bank 3 |

---

> 💡 **Tip**: Luôn mở file `BBB_docs/datasheets/spruh73q.pdf` (AM335x TRM) khi học để tra cứu thanh ghi.
