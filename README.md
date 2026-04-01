# 📘 Ghi Chú Kiến Thức - AM335x & Embedded Linux

> File này được cập nhật sau mỗi buổi học để ôn tập lại kiến thức.
> Mỗi phần sẽ tóm tắt: khái niệm, thanh ghi quan trọng, câu lệnh Linux, code mẫu.

---

## 📋 Mục lục
- [Giai đoạn 1: Nền tảng](#giai-đoạn-1-nền-tảng)
- [Giai đoạn 2: Ngoại vi cơ bản](#giai-đoạn-2-ngoại-vi-cơ-bản)
- [Giai đoạn 3: Ngoại vi nâng cao](#giai-đoạn-3-ngoại-vi-nâng-cao)
- [Giai đoạn 4: Hệ thống](#giai-đoạn-4-hệ-thống)
- [Câu lệnh Linux thường dùng](#câu-lệnh-linux-thường-dùng)
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

### Bài 3 - Linux cơ bản cho Embedded

- Cấu trúc thư mục quan trọng khi làm embedded Linux:
	- `/dev`: thiết bị dạng file (UART, I2C, SPI, eMMC, `/dev/mem`)
	- `/sys`: thuộc tính thiết bị do kernel export (sysfs)
	- `/proc`: thông tin runtime của kernel và process

- Sysfs GPIO thực tế:
	- Ban đầu thường chỉ có `export`, `unexport`, `gpiochip*`
	- Thư mục `gpioN` chỉ xuất hiện sau khi export thành công
	- Cần quyền root khi ghi vào sysfs node nhạy cảm

- Công thức đổi tên GPIO từ TRM sang Linux:

```text
linux_gpio_number = (module * 32) + bit
```

Ví dụ:
	- `GPIO2_3 = 67`
	- `GPIO1_29 = 61`

- Device Tree:
	- `.dts` là file text mô tả phần cứng
	- Biên dịch thành `.dtb`
	- U-Boot nạp `.dtb`, kernel đọc để tạo thiết bị/driver tương ứng

- Bài học rút ra từ thực hành:
	- Lỗi `Permission denied` khi ghi sysfs là lỗi quyền
	- Lỗi `invalid GPIO` thường liên quan cấu hình kernel/driver/pinmux hoặc GPIO không thuộc dải hợp lệ
	- Nên kiểm tra nhanh bằng `/sys/class/leds/` trước khi debug GPIO generic

### Bài 4 - Memory-mapped I/O

- **Mô tả**: Giới thiệu cách truy cập thanh ghi phần cứng từ userspace bằng cách ánh xạ địa chỉ vật lý sang địa chỉ ảo với `mmap()` trên `/dev/mem`.

- **Các điểm chính**:
  - Virtual memory và vai trò của MMU: userspace không truy cập trực tiếp physical address.
  - Dùng `/dev/mem` + `mmap()` để ánh xạ vùng thanh ghi (ví dụ `GPIO1_BASE`).
  - Thanh ghi GPIO quan trọng: `GPIO_OE`, `GPIO_DATAOUT`, `GPIO_SETDATAOUT`, `GPIO_CLEARDATAOUT`.
  - Dùng `volatile` khi truy cập thanh ghi; luôn kiểm tra pinmux, clock và Device Tree trước khi thao tác.

- **Ví dụ**: `lessons/bai_04_memory_mapped_io/examples/led_mmap.c` — minh họa đọc/sửa `GPIO_OE` và dùng `SETDATAOUT`/`CLEARDATAOUT` để toggle LED.

- **Lệnh thử nhanh**:
```bash
gcc -o led_mmap lessons/bai_04_memory_mapped_io/examples/led_mmap.c
sudo ./led_mmap
```

- **Trạng thái**: Completed (2026-04-01) — nội dung đã mở rộng; ví dụ cần kiểm tra thực thi trên hardware.

---

## Giai đoạn 2: Ngoại vi cơ bản

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 3: Ngoại vi nâng cao

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 4: Hệ thống

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Câu lệnh Linux thường dùng

| Lệnh | Mô tả |
|-------|--------|
| *(sẽ cập nhật)* | |

---

## Địa chỉ thanh ghi quan trọng

| Module | Thanh ghi | Địa chỉ | Mô tả |
|--------|-----------|----------|--------|
| CM_PER | Base | 0x44E00000 | Clock module cho nhiều peripheral |
| GPIO0 | Base | 0x44E07000 | GPIO bank 0 |
| UART0 | Base | 0x44E09000 | UART console chính trên BBB |
| CONTROL MODULE | Base | 0x44E10000 | Pinmux và pad control |
| GPIO1 | Base | 0x4804C000 | GPIO bank 1 |

---

> 💡 **Tip**: Luôn mở file `BBB_docs/datasheets/spruh73q.pdf` (AM335x TRM) khi học để tra cứu thanh ghi.
