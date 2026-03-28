# Bài tập - Bài 30: Tạo Custom Image cho BBB

## Bài tập 1: Thiết kế image recipe

Thiết kế image recipe `factory-bbb-image.bb` cho một thiết bị BBB dùng trong nhà máy với các yêu cầu:

**Yêu cầu phần cứng:** AM335x BBB, SPI flash ngoài, 3 cảm biến I2C, CAN bus

**Yêu cầu phần mềm:**
- Ứng dụng `factory-monitor` (binary tự viết)
- Hỗ trợ can-utils để debug CAN
- i2c-tools và spi-tools để maintenance
- Python 3 với thư viện smbus và RPi.GPIO
- Không cần X11, không cần WiFi
- SSH server để remote access từ LAN
- **Production mode:** KHÔNG có debug-tweaks, root password đặt là hash cụ thể

Viết hoàn chỉnh:
1. `factory-bbb-image.bb` với đúng IMAGE_FEATURES và IMAGE_INSTALL
2. `bbb-factory.cfg` kernel fragment với các CONFIG cần thiết
3. Giải thích tại sao từng package được thêm vào

---

## Bài tập 2: Kernel config verification

Sau khi build image với fragment dưới đây:

```
CONFIG_OMAP_WATCHDOG=y
CONFIG_I2C_CHARDEV=y
CONFIG_SPI_SPIDEV=y
CONFIG_CAN=y
CONFIG_CAN_RAW=y
```

**Câu hỏi:**
1. Dùng lệnh nào để kiểm tra fragment đã được merge vào `.config` không?
2. Khi boot BBB với image này, file device nào sẽ xuất hiện tại `/dev/`?
3. Nếu `CONFIG_OMAP_WATCHDOG=y` nhưng `/dev/watchdog` không xuất hiện, nguyên nhân có thể là gì?

---

## Bài tập 3: SDK và workflow

Scenario: Developer B không có quyền truy cập máy build Yocto (chỉ chạy trên server). Developer B cần cross-compile ứng dụng C++ cho BBB.

1. Developer A (có server) cần chạy lệnh gì để tạo artifact cho Developer B?
2. Developer B nhận được file gì? Cần làm gì với file đó?
3. Sau khi source SDK environment, Developer B gặp lỗi: `c++: command not found`. Debug và fix lỗi này.
4. Compile xong binary, làm thế nào để copy nhanh lên BBB đang chạy qua mạng LAN?

---

## Bài tập 4 (Nâng cao): Partition layout tùy chỉnh

Thiết kế partition layout cho BBB với yêu cầu:
- Partition `/boot`: 64MB FAT, chứa MLO + u-boot + kernel
- Partition `/`: 1GB ext4, rootfs - read-only khi production
- Partition `/data`: phần còn lại ext4, persistent data, read-write
- Viết file `.wks` tương ứng
- Trong image recipe, khai báo `WKS_FILE` và `IMAGE_FSTYPES`
