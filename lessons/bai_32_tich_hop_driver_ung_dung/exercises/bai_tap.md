# Bài tập - Bài 32: Tích Hợp Driver và Ứng Dụng

## Bài tập 1: Recipe hoàn chỉnh cho kernel module

Viết recipe `bbb-spi-adc_1.0.bb` để đóng gói kernel module SPI ADC (bài 10) vào Yocto:

Yêu cầu:
- `inherit module`
- Module tự load khi boot
- Cài udev rule để tạo `/dev/spi_adc0` với permissions `0664`
- Group `spidev` có quyền đọc/ghi

Viết đủ:
1. Recipe file `bbb-spi-adc_1.0.bb`
2. udev rule `99-spi-adc.rules`
3. `Makefile` cho module

---

## Bài tập 2: Phụ thuộc giữa driver và app

Bạn có 2 packages:
- `bbb-adc-driver` — kernel module tạo `/dev/adc_dev0`
- `adc-logger` — userspace app đọc từ `/dev/adc_dev0`

**Câu hỏi:**
1. Trong recipe `adc-logger`, dùng `DEPENDS` hay `RDEPENDS` để khai báo cần `bbb-adc-driver`?
2. Trong systemd service của `adc-logger`, cần `After=` gì để đảm bảo module đã load trước khi app chạy?
3. Nếu người dùng install `adc-logger` bằng `opkg install adc-logger`, hệ thống có tự install `bbb-adc-driver` không? Tại sao?

---

## Bài tập 3: Debugging module version mismatch

Sau khi flash image mới lên BBB, gặp lỗi:

```
# modprobe my_adc_driver
modprobe: ERROR: could not insert 'my_adc_driver':
    Exec format error
dmesg | tail -5:
[   5.234] my_adc_driver: version magic
    '5.10.168-ti armv7-A SMP mod_unload modversions ARMv7 p2v8'
    should be
    '5.10.120-ti armv7-A SMP mod_unload modversions ARMv7 p2v8'
```

1. Version mismatch ở đây là gì?
2. Nguyên nhân gốc trong Yocto build là gì?
3. Làm thế nào để fix? (không cần rebuild toàn bộ image)
4. Cách phòng tránh vấn đề này trong tương lai?

---

## Bài tập 4: Full integration test script

Viết shell script `test_integration.sh` chạy trên BBB để kiểm tra toàn bộ stack:

```bash
#!/bin/sh
# test_integration.sh
# Kiểm tra driver + app đã được cài đúng

PASS=0
FAIL=0

check() {
    local desc="$1"
    local cmd="$2"
    if eval "$cmd" > /dev/null 2>&1; then
        echo "[PASS] $desc"
        PASS=$((PASS+1))
    else
        echo "[FAIL] $desc"
        FAIL=$((FAIL+1))
    fi
}

# Viết các test cases:
check "Module loaded" "lsmod | grep -q my_gpio_drv"
check "Device node exists" "test -c /dev/my_gpio0"
# ... thêm nhiều checks ...

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ] && exit 0 || exit 1
```

Hoàn thiện script với ít nhất 8 test cases bao gồm:
- Module đã load
- Device node tồn tại
- Permissions đúng
- Service đang chạy
- App binary tồn tại và executable
- App hoạt động đúng (write/read)
- udev rule được áp dụng
- Log file được tạo (nếu app có log)

---

## Bài tập 5 (Nâng cao): OTA update scenario

Yêu cầu update firmware của thiết bị BBB đang chạy ở field (không có người trực tiếp tới board):

1. Bạn đã fix bug trong `my_gpio_drv.c` và build package mới
2. Board BBB đang kết nối internet
3. Cần update chỉ package `gpio-control` mà không reboot

**Mô tả quy trình:**
1. Trên máy build: làm thế nào để tạo `.ipk` package riêng lẻ?
2. Deploy package repository (opkg feed) ở đâu?
3. Trên board: lệnh opkg nào để update chỉ `gpio-control`?
4. Systemd service cần làm gì để áp dụng binary mới mà không reboot?
5. Rủi ro gì có thể xảy ra và cách rollback?
