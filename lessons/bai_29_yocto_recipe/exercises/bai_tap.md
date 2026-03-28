# Bài tập - Bài 29: Viết Recipe và bbappend

## Bài tập 1: Viết recipe từ đầu

Viết một recipe `sensor-logger_1.0.bb` cho ứng dụng C sau đây.

Yêu cầu recipe phải:
- Đặt binary tại `/usr/bin/sensor-logger`
- Đặt file config mẫu tại `/etc/sensor-logger/config.ini`
- Tạo thư mục `/var/log/sensor-logger/` lúc install
- Chạy như systemd service tự khởi động khi boot

```c
/* sensor-logger.c — đọc từ /dev/i2c-1, ghi vào log */
#include <stdio.h>
#include <unistd.h>
int main() {
    while (1) {
        /* Đọc sensor, ghi vào /var/log/sensor-logger/data.log */
        sleep(5);
    }
    return 0;
}
```

Viết đủ:
- `sensor-logger_1.0.bb` (recipe)
- `sensor-logger.service` (systemd unit)
- Phần `do_install` đầy đủ với tất cả files

---

## Bài tập 2: bbappend để patch U-Boot

Bạn muốn thêm environment variable custom (`bbenv`) vào U-Boot cho BBB mà **không** sửa recipe gốc của `u-boot-ti-staging`.

Viết `u-boot-ti-staging_%.bbappend` và file patch tương ứng để:
1. Thêm biến `custom_bootargs=console=ttyO0,115200n8 root=/dev/mmcblk0p2 rw`
2. Đặt `bootdelay=2` thay vì default là 0

Gợi ý: Dùng `SRC_URI:append = " file://0001-custom-env.patch"`

---

## Bài tập 3: devtool debug workflow

Mô tả từng bước bạn sẽ thực hiện trong scenario sau:

**Scenario:** Bạn vừa thêm một tính năng mới vào `sensor-logger`, cần test trên board BBB **nhanh nhất có thể** mà không phải rebuild toàn bộ image.

1. Lệnh `devtool` nào để rebuild chỉ package `sensor-logger`?
2. Lệnh nào để copy binary mới lên board (IP: 192.168.7.2)?  
3. Trên board, lệnh nào để restart service?
4. Sau khi test xong, lệnh nào để commit thay đổi vào layer `meta-bbb-custom`?

---

## Bài tập 4 (Nâng cao): Recipe cho thư viện

Viết recipe `libsensor_1.0.bb` cho một C library (tạo `libsensor.so`).

Yêu cầu:
- `libsensor.so` đặt tại `/usr/lib/libsensor.so.1.0`
- Header `sensor.h` đặt tại `/usr/include/sensor.h`
- Tạo symlink `/usr/lib/libsensor.so` → `/usr/lib/libsensor.so.1.0`
- `sensor-logger` recipe ở bài tập 1 phải dùng `DEPENDS = "libsensor"`

Gợi ý biến: `${libdir}`, `${includedir}`, và dùng `FILES:${PN}-dev` cho header files.
