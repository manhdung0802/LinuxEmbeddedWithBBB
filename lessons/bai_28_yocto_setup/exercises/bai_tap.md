# Bài tập - Bài 28: Cài Đặt Môi Trường Yocto

## Bài tập 1: Hiểu biến môi trường

Sau khi chạy `source poky/oe-init-build-env build-bbb`, hãy kiểm tra và giải thích các biến sau:

```bash
echo $BBPATH
echo $BUILDDIR
echo $PATH | tr ':' '\n' | grep yocto
```

**Câu hỏi:**
1. `BBPATH` trỏ đến đâu? Dùng để làm gì?
2. Tại sao `PATH` được thêm các thư mục của BitBake?
3. Nếu bạn mở tab terminal mới và gõ `bitbake --version`, có lỗi không? Vì sao?

---

## Bài tập 2: Phân tích bblayers.conf

Cho `bblayers.conf` sau, hãy:
1. Xác định layer nào cung cấp `MACHINE = "beaglebone"`
2. Layer nào cần thêm để có thể dùng `python3-numpy` trong image?
3. Nếu thiếu `meta-ti/meta-ti-bsp`, lỗi gì sẽ xảy ra?

```bitbake
BBLAYERS ?= " \
  /home/user/yocto-bbb/poky/meta \
  /home/user/yocto-bbb/poky/meta-poky \
  /home/user/yocto-bbb/poky/meta-yocto-bsp \
  /home/user/yocto-bbb/meta-openembedded/meta-oe \
  "
```

---

## Bài tập 3: Debug lỗi build

Trong quá trình `bitbake core-image-minimal`, gặp lỗi:

```
ERROR: Nothing PROVIDES 'virtual/kernel' (but /home/.../meta-ti/meta-ti-bsp/recipes-kernel/linux/linux-ti_5.10.bb PROVIDES it)
NOTE: Runtime target 'virtual/kernel' is unavailable.
Contributions to the '...' task depends ...
```

**Câu hỏi:**
1. Lỗi này xảy ra vì layer nào chưa được thêm vào `bblayers.conf`?
2. Sau khi thêm đúng layer, cần chạy lệnh gì để BitBake đọc lại layers?
3. `virtual/kernel` là gì? Tại sao không phải là tên recipe cụ thể?

---

## Bài tập thực hành: Kiểm tra môi trường

Nếu bạn có máy Ubuntu, hãy thực hiện và ghi lại kết quả:

```bash
# Kiểm tra xem host có đủ điều kiện không
bitbake -e | grep "^MACHINE ="
bitbake-layers show-layers
bitbake -s | wc -l    # Xem tổng số recipes
```

Báo cáo:
- MACHINE có phải là "beaglebone" không?
- Có bao nhiêu layers?
- Có bao nhiêu recipes tổng cộng?
