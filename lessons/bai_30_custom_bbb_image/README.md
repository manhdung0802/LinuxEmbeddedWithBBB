# Bài 30 - Custom BBB Image

## Image recipe nhanh

```bitbake
# my-bbb-image.bb
require recipes-core/images/core-image-minimal.bb

IMAGE_FEATURES += "ssh-server-openssh"
EXTRA_IMAGE_FEATURES += "debug-tweaks"  # ← XÓA trong production!

IMAGE_INSTALL:append = " \
    myapp \
    i2c-tools \
    python3-core \
    can-utils \
    "

IMAGE_FSTYPES = "tar.bz2 ext4 wic"
```

## Kernel fragment

```
# bbb-custom.cfg
CONFIG_OMAP_WATCHDOG=y
CONFIG_I2C_CHARDEV=y
CONFIG_SPI_SPIDEV=y
CONFIG_CAN=y
```

```bitbake
# linux-ti_%.bbappend
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append = " file://bbb-custom.cfg"
```

## Build image

```bash
bitbake my-bbb-image              # Full image build
bitbake my-bbb-image -c populate_sdk  # Tạo SDK
```

## SDK sử dụng

```bash
source /opt/poky/4.0/environment-setup-cortexa8t2hf-...
$CC -o myapp myapp.c      # Cross-compile cho ARM
```

## Flash

```bash
sudo bmaptool copy my-bbb-image-beaglebone.wic /dev/sdX
# Boot từ SD: Giữ nút S2 khi cắm nguồn
```

## IMAGE_FEATURES thường dùng

| Feature | Cài gì |
|---------|--------|
| `ssh-server-openssh` | OpenSSH server |
| `ssh-server-dropbear` | Dropbear (nhỏ hơn) |
| `package-management` | opkg package manager |
| `debug-tweaks` | Root no-password (**chỉ dev**) |
