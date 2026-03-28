# Bài 30 - Tạo Custom Linux Image cho BeagleBone Black

## 1. Mục tiêu bài học

- Viết image recipe tùy chỉnh cho BBB
- Hiểu IMAGE_FEATURES, IMAGE_INSTALL, EXTRA_IMAGE_FEATURES
- Chỉnh sửa kernel qua `.bbappend` và config fragment
- Thêm systemd service vào image và kiểm soát startup
- Tạo SDK (Software Development Kit) để cross-compile ngoài Yocto
- Flash và boot image đã build

---

## 2. Image Recipe Là Gì?

Image recipe là một recipe đặc biệt **khai báo danh sách packages** để lắp thành rootfs:

```
Recipe bình thường:      build 1 package (binary, libraries)
Image recipe:            combine nhiều packages → rootfs → .wic image
```

Kế thừa theo chuỗi trong Poky:
```
core-image-minimal.bb
    ← core-image.bbclass
       ← image.bbclass
          ← rootfs.bbclass
             ← ...
```

---

## 3. Viết Custom Image Recipe

### 3.1 Cấu trúc thư mục

```
meta-bbb-custom/
└── recipes-bbb/
    └── images/
        └── my-bbb-image.bb
```

### 3.2 my-bbb-image.bb — Image đầy đủ

```bitbake
# my-bbb-image.bb
# Image tùy chỉnh cho BBB sensor node
#
# Cách build:
#   bitbake my-bbb-image

SUMMARY = "Custom production image for BBB sensor node"
DESCRIPTION = "Includes sensor app, i2c tools, SSH access"

# Kế thừa từ core-image-minimal (kernel + busybox + udev)
require recipes-core/images/core-image-minimal.bb

# ──────────────────────────────────────────────────────────────
# IMAGE_FEATURES — Tính năng cấp cao (activates recipe groups)
# ──────────────────────────────────────────────────────────────
IMAGE_FEATURES += " \
    ssh-server-openssh \
    package-management \
    "
# Một số IMAGE_FEATURES phổ biến:
# ssh-server-openssh  → cài openssh-server
# ssh-server-dropbear → cài dropbear (nhỏ hơn)
# package-management  → cài opkg/rpm package manager
# x11                 → X11 server
# hwcodecs            → hardware accelerated codecs
# debug-tweaks        → root no-password, dropbear key, ptrace debug

# ──────────────────────────────────────────────────────────────
# EXTRA_IMAGE_FEATURES — Từ local.conf hoặc override ở đây
# ──────────────────────────────────────────────────────────────
# debug-tweaks: root login không cần password (CHỈ DÙNG KHI DEV!)
# Trong production: XÓA debug-tweaks
EXTRA_IMAGE_FEATURES += "debug-tweaks"

# ──────────────────────────────────────────────────────────────
# IMAGE_INSTALL — Danh sách packages thêm vào image
# ──────────────────────────────────────────────────────────────
IMAGE_INSTALL:append = " \
    led-blinker \
    sensor-logger \
    i2c-tools \
    spi-tools \
    python3-core \
    python3-smbus \
    can-utils \
    iproute2 \
    util-linux \
    "
# Chú ý: dùng :append (không phải +=) để tránh override base packages

# ──────────────────────────────────────────────────────────────
# IMAGE_ROOTFS_SIZE — Giới hạn kích thước rootfs (KB)
# ──────────────────────────────────────────────────────────────
IMAGE_ROOTFS_SIZE ?= "262144"   # 256 MB
IMAGE_OVERHEAD_FACTOR ?= "1.3"  # 30% overhead cho filesystem

# ──────────────────────────────────────────────────────────────
# IMAGE_FSTYPES — Loại image files xuất ra
# ──────────────────────────────────────────────────────────────
IMAGE_FSTYPES = "tar.bz2 ext4 wic"
# wic: SD card image (bootable, all-in-one)
# ext4: rootfs filesystem image
# tar.bz2: rootfs tarball (NFS booting)

# ──────────────────────────────────────────────────────────────
# Post-process hooks — Chạy script sau khi tạo rootfs
# ──────────────────────────────────────────────────────────────
# my_post_process() {
#     # Ví dụ: thêm version file
#     echo "${DISTRO_VERSION}" > ${IMAGE_ROOTFS}/etc/bbb-version
#     # Ví dụ: tạo symlink
#     ln -sf /usr/bin/led_blinker ${IMAGE_ROOTFS}/usr/local/bin/blink
# }
# ROOTFS_POSTPROCESS_COMMAND += "my_post_process;"
```

---

## 4. Tùy Chỉnh Kernel

### 4.1 Thêm kernel config fragment

```
meta-bbb-custom/
└── recipes-kernel/
    └── linux/
        ├── linux-ti_%.bbappend
        └── files/
            └── bbb-custom.cfg
```

```bitbake
# linux-ti_%.bbappend
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append = " file://bbb-custom.cfg"
```

```
# bbb-custom.cfg — Custom kernel config cho BBB production
#
# Watchdog
CONFIG_OMAP_WATCHDOG=y

# I2C/SPI user-space access
CONFIG_I2C_CHARDEV=y
CONFIG_SPI_SPIDEV=y

# CAN bus
CONFIG_CAN=y
CONFIG_CAN_RAW=y
CONFIG_CAN_VCAN=y
CONFIG_CAN_C_CAN=y
CONFIG_CAN_C_CAN_PLATFORM=y

# Debug: tắt các module không cần để giảm kích thước kernel
# CONFIG_SOUND is not set
# CONFIG_USB_AUDIO is not set
```

### 4.2 Kiểm tra config đã được merge

```bash
# Sau khi build kernel, kiểm tra:
bitbake linux-ti -c kernel_configcheck
# Hiện cảnh báo nếu fragment không được apply

# Hoặc xem file .config cuối:
cat tmp/work/beaglebone_zynq-poky-linux-gnueabi/linux-ti/5.10*/build/.config | grep CONFIG_OMAP_WATCHDOG
```

---

## 5. Tạo Custom DISTRO (Tùy Chọn)

Cho production, tạo DISTRO riêng thay vì dùng "poky":

```
meta-bbb-custom/
└── conf/
    └── distro/
        └── bbb-production.conf
```

```bitbake
# bbb-production.conf
DISTRO = "bbb-production"
DISTRO_NAME = "BBB Production"
DISTRO_VERSION = "1.0"
DISTRO_CODENAME = "v1"

# Kế thừa từ poky
require conf/distro/poky.conf

# Production: dùng systemd thay vì sysvinit
DISTRO_FEATURES:append = " systemd"
DISTRO_FEATURES:remove = "sysvinit"
VIRTUAL-RUNTIME_init_manager = "systemd"
VIRTUAL-RUNTIME_initscripts = ""

# Tắt các feature không cần
DISTRO_FEATURES:remove = "x11 wayland vulkan"

# Thêm features cần
DISTRO_FEATURES:append = " can wifi bluetooth"

# Toolchain: hard-float (Cortex-A8 có VFP)
DEFAULTTUNE = "cortexa8hf-neon"
```

---

## 6. Tạo SDK

SDK cho phép developer cross-compile **ngoài Yocto** mà không cần rebuild:

```bash
# Tạo SDK installer từ image
bitbake my-bbb-image -c populate_sdk

# Output:
# tmp/deploy/sdk/poky-glibc-x86_64-my-bbb-image-cortexa8t2hf-neon-beaglebone-toolchain-4.0.sh

# Cài SDK (chạy trên host developer)
./poky-glibc-x86_64-my-bbb-image-cortexa8t2hf-neon-beaglebone-toolchain-4.0.sh
# Chọn thư mục cài (mặc định /opt/poky/4.0)
```

### 6.1 Sử dụng SDK

```bash
# Source SDK environment
source /opt/poky/4.0/environment-setup-cortexa8t2hf-neon-poky-linux-gnueabi

# Kiểm tra
echo $CC
# arm-poky-linux-gnueabi-gcc ... (cross-compiler)

$CC --version
# arm-poky-linux-gnueabi-gcc (GCC) 11.3.0

# Cross-compile bình thường
$CC -o myapp myapp.c
file myapp
# myapp: ELF 32-bit LSB executable, ARM, ...

# Compile project với Makefile chuẩn
make    # CC đã được set bởi SDK environment
```

---

## 7. Cấu Trúc WIC Image

File `.wic` là SD card image theo chuẩn **wks (Kickstart)**:

```
SD Card layout (beaglebone.wks):
┌─────────────────────────────────────────────┐
│  MBR (512 bytes)                            │
├─────────────────────────────────────────────┤
│  Partition 1: FAT16 (32MB)   ← /boot        │
│    MLO                        ← boots first │
│    u-boot.img                               │
│    uEnv.txt                                 │
│    zImage                                   │
│    *.dtb                                    │
├─────────────────────────────────────────────┤
│  Partition 2: ext4 (rest)    ← /           │
│    /bin, /etc, /usr, /var ...               │
│    rootfs đầy đủ                            │
└─────────────────────────────────────────────┘
```

### 7.1 Custom WKS file

```
meta-bbb-custom/
└── wic/
    └── bbb-custom.wks
```

```
# bbb-custom.wks — Custom partition layout
# Bootloader partition (FAT)
part /boot --source bootimg-partition --ondisk mmcblk0 \
    --fstype=vfat --label boot --active --align 1024 --size 64

# Root partition (ext4) — 2GB
part / --source rootfs --ondisk mmcblk0 \
    --fstype=ext4 --label root --align 1024 --size 2048

# Data partition (ext4) — còn lại
part /data --ondisk mmcblk0 \
    --fstype=ext4 --label data --align 1024
```

Trong recipe:
```bitbake
# Dùng custom WKS
WKS_FILE = "bbb-custom.wks"
IMAGE_FSTYPES = "wic wic.bmap"
```

---

## 8. Flash và Boot

```bash
# Xem danh sách disk (CẨNS THẬN không chọn nhầm sda)
lsblk
# sda   8:0  0 500G ...    ← hard disk → KHÔNG ĐƯỢC DÙNG
# sdb   8:16 1  16G ...    ← SD card

# Flash bằng bmaptool (nhanh nhất, có checksums)
cd tmp/deploy/images/beaglebone/
sudo bmaptool copy my-bbb-image-beaglebone.wic /dev/sdb

# Hoặc dd
sudo dd \
    if=my-bbb-image-beaglebone.wic \
    of=/dev/sdb \
    bs=4M \
    conv=fsync \
    status=progress
sudo sync

# ── Boot BBB từ SD Card ─────────────────────────────────
# 1. Đảm bảo BBB đã tắt nguồn
# 2. Cắm SD card vào slot microSD
# 3. GIỮ nút S2 (BOOT button, gần slot SD)
# 4. Cắm nguồn (USB hoặc 5V barrel jack)
# 5. Thả nút S2 sau khi LED bắt đầu nhấp nháy (~5s)
# 6. BBB sẽ boot từ SD thay vì eMMC

# Kết nối serial console (để xem boot log)
# BBB UART0: P9.21 (RX) / P9.22 (TX) / P9.1 (GND)
screen /dev/ttyUSB0 115200
# hoặc: minicom -D /dev/ttyUSB0 -b 115200
```

---

## 9. Quy Trình Phát Triển Chuẩn

```
Workflow khi thêm tính năng mới:

1. Viết code (C/Python) trên host
2. devtool add/modify để tạo/sửa recipe trong workspace
3. devtool build → deploy-target → test trên board (nhanh!)
4. Nếu ổn: devtool finish → recipe vào meta-bbb-custom
5. bitbake my-bbb-image → tạo image mới
6. Flash lên SD → full integration test
7. Commit meta-bbb-custom lên git

Automated CI/CD:
  git push → Jenkins/GitLab CI → bitbake → flash test board
```

---

## 10. Câu hỏi kiểm tra

1. Sự khác nhau giữa `IMAGE_INSTALL:append` và `IMAGE_INSTALL +=` là gì? Cái nào an toàn hơn?
2. `IMAGE_FEATURES += "ssh-server-openssh"` vs `IMAGE_INSTALL:append = " openssh"` — kết quả có khác nhau không?
3. Tại sao trong production image phải xóa `debug-tweaks`?
4. SDK generate bằng `populate_sdk` khác gì với cross-compiler đơn giản bạn tự build?
5. Nếu muốn resize partition `/data` lên 4GB, bạn sửa file nào và dòng nào?
6. `WKS_FILE` custom giải quyết vấn đề gì mà layout mặc định không làm được?
