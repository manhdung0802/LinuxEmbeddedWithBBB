# Bài Tập - Bài 19: Platform Driver & Device Tree Binding

## Bài tập 1: Đọc Nhiều Property từ DT

Mở rộng `myled_platform_driver.c` để đọc thêm property từ DT:
- `led-label` (string): tên LED (ví dụ: "usr0")
- `blink-interval-ms` (u32): thời gian blink (ms), default 500
- `active-high` (boolean): LED sáng khi pin HIGH hay LOW

**DTS mẫu**:
```dts
myled@4804C000 {
    compatible = "bbb-tutorial,myled";
    reg = <0x4804C000 0x1000>;
    my-gpio-pin = <21>;
    led-label = "usr0";
    blink-interval-ms = <250>;
    active-high;
};
```

**Yêu cầu**: Log đầy đủ các giá trị đọc được khi probe. Nếu property không có thì dùng giá trị mặc định.

---

## Bài tập 2: Platform Driver + Char Device

Kết hợp bài 18 và 19: Viết platform driver tạo thêm character device `/dev/myled` để userspace control LED.

**Thiết kế**:
1. `probe()`: ioremap GPIO, tạo cdev + /dev/myled
2. ioctl commands:
   - `LED_ON`: bật LED
   - `LED_OFF`: tắt LED
   - `LED_TOGGLE`: đổi trạng thái
3. `remove()`: xóa cdev, iounmap

**Hint**: Kết hợp code từ bài 18 (`mychar_driver.c`) và bài 19 (`myled_platform_driver.c`). Dùng `devm_*` cho tất cả allocations trong probe.

---

## Bài tập 3: Phân tích Driver Thực

Đọc source code driver GPIO của AM335x trong kernel:
```bash
# Trên BBB hoặc trong kernel source tree
# File: drivers/gpio/gpio-omap.c
```

Hoặc xem online tại: `https://elixir.bootlin.com/linux/latest/source/drivers/gpio/gpio-omap.c`

**Yêu cầu**:
1. Tìm `of_match_table` của driver — compatible string là gì?
2. Hàm `probe()` làm những gì? (tóm tắt các bước chính)
3. Nó dùng `devm_*` nào?
4. So sánh cấu trúc với driver bạn đã viết trong bài này
