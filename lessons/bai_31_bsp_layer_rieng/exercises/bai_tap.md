# Bài tập - Bài 31: Tạo BSP Layer Riêng

## Bài tập 1: Tạo layer từ đầu

Tạo hoàn chỉnh `meta-bbb-custom` layer với yêu cầu:

1. Tạo `conf/layer.conf` với tất cả fields cần thiết
2. Layer phải tương thích với `kirkstone` và `scarthgap`
3. Layer phải depend vào `meta-ti-bsp`
4. Priority là 10

Kiểm tra bằng:
```bash
bitbake-layers show-layers | grep bbb-custom
bitbake-layers check-userlayers
```

---

## Bài tập 2: Custom machine configuration

Tạo machine `bbb-sensor-node.conf` kế thừa từ `beaglebone` với:
- Thêm MACHINE_FEATURES: `can`
- Đặt SERIAL_CONSOLES port 0: ttyO0 115200 (UART0 của AM335x)
- IMAGE_FSTYPES thêm `wic`
- Comment giải thích từng setting

Kiểm tra:
```bash
MACHINE=bbb-sensor-node bitbake -e | grep "^MACHINE_FEATURES"
```

---

## Bài tập 3: Conflict resolution với priority

Xét tình huống: `meta-ti-bsp` (priority 8) và `meta-bbb-custom` (priority 10) đều có `linux-ti_5.10.bb`.

**Câu hỏi:**
1. BitBake sẽ dùng recipe nào?
2. Cách kiểm tra recipe nào đang được dùng
3. Cách đúng đắn để override recipe là gì? (fork vs bbappend — cái nào nên dùng?)
4. Khi nào bắt buộc phải fork recipe (không thể dùng bbappend)?

---

## Bài tập 4: Layer dependency chain

Cho bblayers.conf sau, phân tích xem có đủ dependency không:

```bitbake
BBLAYERS ?= " \
  .../poky/meta \
  .../poky/meta-poky \
  .../meta-bbb-custom \
  "
```

Layer `meta-bbb-custom` có:
```bitbake
LAYERDEPENDS_bbb-custom = "core openembedded-layer meta-ti-bsp"
```

1. Dependency nào bị thiếu?
2. Lỗi BitBake sẽ như thế nào khi thiếu dependency?
3. Viết lại `bblayers.conf` đúng cách

---

## Bài tập 5 (Thực hành): Thêm kernel patch

Bạn phát hiện một bug trong driver AM335x I2C, có file patch `0001-i2c-fix-timeout.patch`.

Mô tả đầy đủ cách bạn:
1. Đặt patch file vào đâu trong `meta-bbb-custom`
2. Viết `linux-ti_%.bbappend` để apply patch
3. Verify patch đã được apply sau build
4. Nếu patch không apply được (reject), debug như thế nào?
