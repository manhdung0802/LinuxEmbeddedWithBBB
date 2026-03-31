# Bài 27 - Tổng Quan Yocto Project: Kiến Trúc, Layer, BitBake, Recipe

## 1. Mục tiêu bài học

- Hiểu Yocto Project là gì và tại sao cần thiết cho embedded
- Nắm vững kiến trúc: Poky, OpenEmbedded-Core, BitBake
- Hiểu layer model, recipe, image, machine configuration
- Biết luồng build đầy đủ từ recipe → package → image
- So sánh Yocto với Buildroot và compile thủ công
- Hiểu các release codename và vòng đời hỗ trợ

---

## 2. Yocto Project Là Gì?

**Yocto Project** là một **framework** mã nguồn mở để xây dựng custom Linux distribution cho embedded systems. Đây là chuẩn công nghiệp được dùng bởi hầu hết các công ty làm embedded Linux chuyên nghiệp.

```
┌─────────────────────────────────────────────────────────────────┐
│  Yocto KHÔNG PHẢI:                                              │
│    ✗ Một distro Linux (không như Ubuntu, Debian)                │
│    ✗ Một công cụ đơn lẻ (không như cross-gcc)                   │
│    ✗ Chỉ cho một board duy nhất                                 │
│                                                                 │
│  Yocto LÀ:                                                      │
│    ✓ Framework build toàn bộ Linux stack từ source              │
│    ✓ Cross-compilation toolchain tự động theo board             │
│    ✓ Hệ sinh thái recipe cho 10,000+ packages                   │
│    ✓ Reproducible builds — cùng input → cùng output             │
│    ✓ Multi-machine: 1 codebase cho nhiều board khác nhau        │
└─────────────────────────────────────────────────────────────────┘
```

### 2.1 Khi nào dùng Yocto cho BBB?

| Tình huống | Công cụ phù hợp |
|-----------|----------------|
| Học nhanh, chạy demo | Debian/Ubuntu trực tiếp |
| Prototype, rootfs nhỏ, 1 board | Buildroot |
| Sản phẩm thương mại, nhiều models | **Yocto** |
| Cần long-term security updates | **Yocto** |
| Team lớn, nhiều developer | **Yocto** |
| Cần binary packages (update từng phần) | **Yocto** |

---

## 3. Kiến Trúc Tổng Quan

```
┌─────────────────────────────────────────────────────────────────┐
│                        YOCTO PROJECT                            │
│                                                                 │
│  ┌───────────────────────────── POKY ───────────────────────┐  │
│  │                                                           │  │
│  │   ┌─────────────┐    ┌──────────────────────────────┐   │  │
│  │   │   BitBake   │    │     OpenEmbedded-Core         │   │  │
│  │   │  (Build     │◄───│  (meta layer: 2000+ recipes   │   │  │
│  │   │   Engine)   │    │   busybox, gcc, glibc, ...)   │   │  │
│  │   └─────────────┘    └──────────────────────────────┘   │  │
│  │                                                           │  │
│  │   ┌──────────┐  ┌────────────────────────────────────┐  │  │
│  │   │ meta-    │  │  meta-yocto-bsp                    │  │  │
│  │   │ poky     │  │  (Yocto reference BSPs: mpc8315e)  │  │  │
│  │   └──────────┘  └────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Additional layers:                                             │
│  ┌──────────────┐  ┌─────────────────┐  ┌──────────────────┐  │
│  │  meta-ti     │  │ meta-openembed  │  │ meta-bbb-custom  │  │
│  │ (AM335x BSP) │  │ (3000+ recipes) │  │ (YOUR layer)     │  │
│  └──────────────┘  └─────────────────┘  └──────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.1 Vai trò của từng thành phần

| Thành phần | Vai trò | Cung cấp gì |
|-----------|---------|------------|
| **BitBake** | Build engine | Parse recipe, tính dependency, chạy tasks |
| **OE-Core** | Layer cơ sở | Recipes: libc, busybox, gcc, python... |
| **Poky** | Reference distro | = BitBake + OE-Core + meta-poky |
| **meta-ti** | TI BSP layer | Kernel config cho AM335x, U-Boot |
| **meta-openembedded** | Community recipes | Python, Perl, networking tools... |
| **meta-bbb-custom** | Layer của bạn | App tự viết, override config |

---

## 4. Các Khái Niệm Cốt Lõi

### 4.1 Recipe — Công thức build một package

```bitbake
# ─── Ví dụ recipe: blink-led_1.0.bb ───
SUMMARY = "LED blinker for BeagleBone Black"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://blink_led.c"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} blink_led.c -o blink_led ${LDFLAGS}
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 blink_led ${D}${bindir}/blink_led
}
```

### Giải thích chi tiết recipe

a) **`SUMMARY`, `LICENSE`, `LIC_FILES_CHKSUM`**:
- Metadata bắt buộc. `LIC_FILES_CHKSUM` là MD5 hash của file license — BitBake sẽ **fail** nếu hash không khớp (phát hiện license thay đổi).

b) **`SRC_URI = "file://blink_led.c"`**:
- Lấy source từ thư mục local `files/blink_led.c` trong recipe directory.
- Các scheme khác: `git://`, `https://`, `ftp://`. Ví dụ: `SRC_URI = "git://github.com/..."`.

c) **`${CC}`, `${CFLAGS}`, `${LDFLAGS}`**:
- Biến do Yocto set tự động: `${CC}` = `arm-linux-gnueabihf-gcc` (cross-compiler).
- **BắT BUỘC** dùng `${LDFLAGS}` để link đúng sysroot. Thiếu `${LDFLAGS}` = lỗi link hoặc binary không chạy.

d) **`do_install()`**:
- `install -d ${D}${bindir}` — tạo thư mục `${D}/usr/bin/` (`${D}` = destination root, `${bindir}` = `/usr/bin`).
- `install -m 0755` — copy file với permission 755 (executable).
- BitBake sau đó tự động đóng gói từ `${D}` thành package (ipk/rpm/deb).

**Giải thích cột `S`:** `${WORKDIR}` là thư mục làm việc của recipe, chứa source sau khi unpack.

### 4.2 Layer — Tập hợp recipes theo chủ đề

```
meta-bbb-custom/               ← Tên layer
├── conf/
│   └── layer.conf             ← Khai báo bắt buộc
├── recipes-bsp/               ← Recipes liên quan bootloader
├── recipes-kernel/            ← Kernel customization
├── recipes-myapp/             ← App tự viết
│   └── myapp/
│       ├── myapp_1.0.bb
│       └── files/
└── README                     ← Mô tả layer
```

### 4.3 Image — Recipe đặc biệt tạo rootfs

```bitbake
# my-bbb-image.bb
require recipes-core/images/core-image-minimal.bb

SUMMARY = "Production image for BBB sensor node"

IMAGE_INSTALL:append = " \
    myapp \
    i2c-tools \
    python3-core \
    openssh \
    "
```

### 4.4 Machine Configuration — Mô tả phần cứng

```bitbake
# conf/machine/beaglebone.conf  (trong meta-ti)
MACHINE = "beaglebone"

# Kiến trúc ARM Cortex-A8 với FPU
TARGET_ARCH = "arm"
DEFAULTTUNE = "cortexa8hf-neon"

# Kernel provider
PREFERRED_PROVIDER_virtual/kernel = "linux-ti"
PREFERRED_VERSION_linux-ti = "5.10%"

# Bootloader
PREFERRED_PROVIDER_virtual/bootloader = "u-boot-ti-staging"
UBOOT_MACHINE = "am335x_evm_defconfig"

# Serial console
SERIAL_CONSOLES = "115200;ttyS0"
```

### 4.5 DISTRO — Cấu hình distribution

```bitbake
# poky/meta-poky/conf/distro/poky.conf
DISTRO = "poky"            ← Tên của distribution
DISTRO_NAME = "Poky"
DISTRO_VERSION = "4.0.x"  ← Kirkstone

# Có thể tự tạo DISTRO riêng:
# DISTRO = "my-bbb-distro"
# DISTRO_FEATURES = "bluetooth wifi systemd"
# TCLIBC = "glibc"
```

---

## 5. Luồng Build BitBake

Khi bạn gõ `bitbake core-image-minimal`, BitBake thực hiện:

```
Phase 1: PARSE
  BitBake đọc TẤT CẢ layers/recipes → tính dependency graph
  (mất ~30s ngay cả khi đã build xong)

Phase 2: FETCH
  Tải source code về ${DL_DIR} (mặc định: build/downloads/)
  Nguồn: git, http/https, local file, svn...

Phase 3: UNPACK → PATCH
  Giải nén vào ${WORKDIR}
  Apply các .patch files

Phase 4: CONFIGURE
  ./configure --host=arm-linux-gnueabihf ...
  hoặc cmake -DCMAKE_TOOLCHAIN_FILE=...

Phase 5: COMPILE
  make -j8 CC=arm-linux-gnueabihf-gcc ...

Phase 6: INSTALL
  install vào ${D} (staging area, fake rootfs)

Phase 7: PACKAGE
  Tạo .ipk / .rpm / .deb packages

Phase 8: ROOTFS → IMAGE
  Combine packages → rootfs → .wic / .ext4 / .tar.bz2
```

```bash
# Theo dõi progress khi build
bitbake core-image-minimal
# Hiển thị: [  3/ 512] Compiling busybox-1.35.0
#           [ 47/ 512] Linking glibc-2.35
#           [512/ 512] Creating rootfs...
```

---

## 6. Yocto Release Schedule

| Release | Codename | Năm | Trạng thái |
|---------|---------|-----|-----------|
| 3.1 | **Dunfell** | 2020 | LTS (hỗ trợ đến 2024) |
| 3.3 | Hardknott | 2021 | EOL |
| 3.4 | Honister | 2021 | EOL |
| **4.0** | **Kirkstone** | **2022** | **LTS ← Dùng cái này** |
| 4.2 | Mickledore | 2023 | EOL |
| 4.3 | Nanbield | 2023 | EOL |
| 4.4 | Scarthgap | 2024 | LTS |

**Tại sao dùng Kirkstone (4.0 LTS)?**
- Được meta-ti hỗ trợ đầy đủ cho AM335x
- Long-Term Support: nhận security fixes
- Hầu hết tutorial trên mạng dùng Kirkstone

---

## 7. So Sánh: Yocto vs Buildroot vs Manual

| Tiêu chí | Manual Cross-Compile | Buildroot | Yocto |
|---------|---------------------|-----------|-------|
| Học dễ không | Dễ nhất | Trung bình | Khó |
| Build time lần đầu | Nhanh | 30-60 phút | 2-6 tiếng |
| Build time lần sau | — | ~10 phút | 2-10 phút (sstate) |
| Số packages | Tự tìm | ~2,500 | **>14,000** |
| Multi-board | Khó | Khó | Dễ |
| Binary packages | Không | Không | Có (.ipk/.rpm) |
| Cập nhật bảo mật | Thủ công | Khó | **Tốt nhất** |
| Dùng trong sản xuất | Không nên | Được | **Khuyến nghị** |

---

## 8. Sstate-Cache — Cơ chế Cache của Yocto

Yocto có một cơ chế cache rất thông minh gọi là **sstate** (shared state):

```
Khi build task A (ví dụ: compile busybox):
1. BitBake tính "hash" dựa trên: source code + config + env variables
2. Lưu kết quả vào sstate-cache/ với key = hash đó
3. Lần sau nếu hash giống nhau → SKIP compile, load từ cache

Kết quả:
- Lần 1: 3 tiếng
- Lần 2 (sau clean --sstate): 10-15 phút (chỉ unpack cache)
- Lần 2 (không clean): 2-5 phút

Team sharing: Có thể chia sẻ sstate-cache trên NFS/S3
  → developer mới vào team: build lần đầu chỉ mất 15 phút!
```

---

## 9. Câu Hỏi Kiểm Tra

1. Tại sao Yocto gọi là "build framework" chứ không phải "Linux distribution"?
2. Sự khác biệt giữa `MACHINE` và `DISTRO` variable là gì? Ví dụ thực tế.
3. Tại sao layer `meta-ti` là layer riêng, không được tích hợp vào Poky?
4. `sstate-cache` hoạt động dựa trên nguyên tắc nào? Điều gì làm cache bị invalidate?
5. Nếu bạn muốn build image cho 2 board khác nhau (BBB và Raspberry Pi), bạn cấu hình như thế nào trong Yocto?
6. Tại sao build Yocto lần đầu mất nhiều giờ? Kể tên ít nhất 3 thành phần lớn phải compile từ source.

---

## 10. Tài Liệu Tham Khảo

| Tài liệu | Link | Ghi chú |
|---------|------|---------|
| Yocto Project Reference | docs.yoctoproject.org | Tài liệu chính thức |
| BitBake Manual | docs.yoctoproject.org/bitbake | Syntax và variables |
| Poky Repository | github.com/yoctoproject/poky | Reference distro |
| meta-ti | git.yoctoproject.org/meta-ti | TI BSP, AM335x |
| OE Layer Index | layers.openembedded.org | Tìm layer theo tên |
