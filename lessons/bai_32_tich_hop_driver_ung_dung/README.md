# Bài 32 - Tích Hợp Driver & Ứng Dụng vào Yocto

## Quy trình tổng quát

```
Code (driver + app) → Recipe → Layer → Image
```

## Recipe cho kernel module

```bitbake
# my-gpio-driver_1.0.bb
inherit module          # ← Class đặc biệt cho kernel module

SRC_URI = "file://my_gpio_drv.c file://Makefile"
S = "${WORKDIR}"

# Tự động load khi boot
KERNEL_MODULE_AUTOLOAD += "my_gpio_drv"
```

## Recipe cho userspace app

```bitbake
# gpio-control_1.0.bb
inherit systemd

# Phụ thuộc kernel module tại runtime
RDEPENDS:${PN} = "kernel-module-my-gpio-drv"

SYSTEMD_SERVICE:${PN} = "gpio-control.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
```

## Thêm vào image

```bitbake
IMAGE_INSTALL:append = " \
    my-gpio-driver \
    gpio-control \
    kernel-modules \
    "
```

## Kiểm tra trên board

```bash
lsmod | grep my_gpio_drv       # Module đã load?
ls -la /dev/my_gpio0            # Device node tồn tại?
gpio_control on                 # Test app
systemctl status gpio-control   # Service running?
```

## devtool rapid iteration

```bash
devtool modify my-gpio-driver
# Sửa code trong workspace/sources/
devtool build my-gpio-driver
devtool deploy-target my-gpio-driver root@192.168.7.2
# Test trên board
devtool finish my-gpio-driver meta-bbb-custom
```

## Tên package kernel module

```
Module file:    my_gpio_drv.ko
Recipe name:    my-gpio-driver
Package name:   kernel-module-my-gpio-drv
                (underscore → dash, prefix kernel-module-)
```
