# Bài 29 - Viết Yocto Recipe: bb, bbappend, devtool

## 1. Mục tiêu bài học

- Nắm vững cấu trúc một recipe `.bb`
- Hiểu tất cả biến và tasks quan trọng
- Viết recipe thực tế cho ứng dụng C (LED blinker cho BBB)
- Tích hợp systemd service vào recipe
- Dùng `.bbappend` để override recipe có sẵn mà không fork
- Sử dụng `devtool` cho rapid development workflow

---

## 2. Anatomy của Recipe (.bb file)

```bitbake
# ════════════════════════════════════════════════════════════════
#  PHẦN 1: METADATA — Mô tả package
# ════════════════════════════════════════════════════════════════
SUMMARY = "Mô tả ngắn (~80 ký tự), dùng trong package manager"
DESCRIPTION = "Mô tả chi tiết hơn về chức năng package"
AUTHOR = "Nguyen Van A <nva@example.com>"
SECTION = "console/utils"    # Dùng để phân loại trong package manager

# ════════════════════════════════════════════════════════════════
#  PHẦN 2: LICENSE — Bắt buộc có
# ════════════════════════════════════════════════════════════════
LICENSE = "MIT"
# Checksum của file license để đảm bảo reproducibility
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
# Hoặc từ file LICENSE trong source:
# LIC_FILES_CHKSUM = "file://LICENSE;md5=<md5sum của file đó>"

# ════════════════════════════════════════════════════════════════
#  PHẦN 3: SOURCE — Nơi lấy code
# ════════════════════════════════════════════════════════════════
# Local files (nằm trong recipes-xxx/xxx/files/)
SRC_URI = "file://myapp.c \
           file://myapp.service"

# Git repository (ưu tiên dùng SRCREV cho reproducibility)
# SRC_URI = "git://github.com/user/repo.git;protocol=https;branch=main"
# SRCREV = "a1b2c3d4e5f6..."     # Commit hash cụ thể

# HTTP tarball
# SRC_URI = "https://example.com/myapp-${PV}.tar.gz"
# SRC_URI[sha256sum] = "abc123..."   # Kiểm tra integrity

# S = source directory sau khi unpack
S = "${WORKDIR}"          # Cho local files
# S = "${WORKDIR}/git"   # Cho git clone
# S = "${WORKDIR}/myapp-${PV}"  # Cho tarball myapp-1.0.tar.gz

# ════════════════════════════════════════════════════════════════
#  PHẦN 4: DEPENDENCIES
# ════════════════════════════════════════════════════════════════
# Build-time: cần khi cross-compile
DEPENDS = "libcjson zlib"

# Runtime: cần khi chạy trên target
RDEPENDS:${PN} = "libcjson"
# ${PN} = tên package = tên recipe (Package Name)

# ════════════════════════════════════════════════════════════════
#  PHẦN 5: TASKS — Các bước build
# ════════════════════════════════════════════════════════════════
do_compile() {
    # ${CC} = arm-linux-gnueabihf-gcc (cross-compiler)
    # ${CFLAGS} = optimization flags từ Yocto
    # ${LDFLAGS} = linker flags (hardening, rpath...)
    ${CC} ${CFLAGS} myapp.c -o myapp ${LDFLAGS}
    
    # Hoặc dùng make:
    # oe_runmake     ← tương đương ${MAKE} ${PARALLEL_MAKE}
}

do_install() {
    # ${D} = staging area (fake rootfs)
    # install vào ${D}${bindir} → nằm ở /usr/bin/ trong rootfs
    install -d ${D}${bindir}
    install -m 0755 myapp ${D}${bindir}/myapp
}
```

---

## 3. Biến Đường Dẫn Quan Trọng

| Biến | Đường dẫn trên target | Dùng cho |
|-----|----------------------|---------|
| `${D}` | Staging area (fake root) | Luôn prefix khi install |
| `${bindir}` | `/usr/bin` | User executables |
| `${sbindir}` | `/usr/sbin` | System executables (root) |
| `${libdir}` | `/usr/lib` | Shared libraries |
| `${includedir}` | `/usr/include` | Header files |
| `${sysconfdir}` | `/etc` | Cấu hình hệ thống |
| `${datadir}` | `/usr/share` | Arch-independent data |
| `${localstatedir}` | `/var` | Variable runtime data |
| `${systemd_unitdir}` | `/lib/systemd` | Systemd unit files |
| `${WORKDIR}` | Build work dir | Chỉ dùng khi build |

---

## 4. Ví Dụ Thực Tế: LED Blinker cho BBB

### 4.1 Cấu trúc layer và recipe

```
meta-bbb-custom/
├── conf/
│   └── layer.conf
└── recipes-bbb/
    └── led-blinker/
        ├── led-blinker_1.0.bb        ← Recipe
        └── files/
            ├── led_blinker.c          ← Source code
            └── led-blinker.service    ← Systemd service
```

### 4.2 led_blinker.c

```c
/* led_blinker.c
 * Nhấp nháy LED usr0 trên BeagleBone Black
 * Dùng /sys/class/leds interface (không cần root đặc biệt)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define LED_PATH "/sys/class/leds/beaglebone:green:usr0"

static volatile int running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    running = 0;
}

static int write_sysfs(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    write(fd, val, strlen(val));
    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    int interval_ms = 500;
    if (argc == 2)
        interval_ms = atoi(argv[1]);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* Tắt trigger tự động (heartbeat) để kiểm soát thủ công */
    write_sysfs(LED_PATH "/trigger", "none");

    printf("LED blinker: interval=%dms (Ctrl+C để dừng)\n", interval_ms);

    while (running) {
        write_sysfs(LED_PATH "/brightness", "1");
        usleep(interval_ms * 1000);
        write_sysfs(LED_PATH "/brightness", "0");
        usleep(interval_ms * 1000);
    }

    /* Tắt LED khi kết thúc */
    write_sysfs(LED_PATH "/brightness", "0");
    printf("LED blinker: đã dừng\n");
    return 0;
}
```

### Giải thích chi tiết từng dòng code (led_blinker.c)

a) **`#define LED_PATH "/sys/class/leds/beaglebone:green:usr0"`**:
- Sử dụng **sysfs LED interface** thay vì truy cập GPIO trực tiếp. Đây là cách chuẩn trên Linux — không cần root, không cần `/dev/mem`.
- Kernel driver `leds-gpio` (kích hoạt qua Device Tree `gpio-leds`) tạo interface này tự động.

b) **`write_sysfs()` helper**:
- Mở file sysfs → ghi giá trị → đóng. Sysfs file không cần `lseek` (khác với `/sys/class/gpio/gpioXX/value` phải `lseek` khi dùng `poll`).
- Mỗi lần ghi phải `open` + `close` vì sysfs driver xử lý dữ liệu ngay khi `write`.

c) **`write_sysfs(LED_PATH "/trigger", "none")`**:
- Tắt trigger tự động (heartbeat, mmc0, timer...) để kiểm soát LED thủ công.
- Nếu không tắt, kernel trigger sẽ ghi đè `brightness` của chúng ta.

d) **`write_sysfs(LED_PATH "/brightness", "1")` / `"0"`**:
- Ghi `"1"` = bật LED, `"0"` = tắt LED.
- Giá trị có thể là `0-255` cho LED hỗ trợ PWM dimming.

e) **Signal handling + cleanup**:
- `signal(SIGTERM, sig_handler)` — quan trọng cho systemd service. Khi `systemctl stop`, systemd gửi `SIGTERM`.
- Cleanup tắt LED → sau `systemctl stop`, LED không bị kẹt ở trạng thái bật.

f) **`atoi(argv[1])` cho interval**:
- Cho phép truyền interval từ command line hoặc systemd ExecStart.
- Trong service file: `ExecStart=/usr/bin/led_blinker 500` = 500ms interval.

> **Bài học**: Code dùng sysfs interface (đơn giản, portable) thay vì mmap/ioremap (đã học ở bài 4, 17). Trong Yocto image, đây là cách tiêu chuẩn cho application-level GPIO access.

### 4.3 led-blinker.service

```ini
[Unit]
Description=LED Blinker Daemon for BeagleBone Black
After=sysinit.target local-fs.target
Wants=local-fs.target

[Service]
Type=simple
ExecStart=/usr/bin/led_blinker 500
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### 4.4 led-blinker_1.0.bb (recipe)

```bitbake
SUMMARY = "LED blinker application for BeagleBone Black"
DESCRIPTION = "Blinks user LED via /sys/class/leds, runs as systemd service"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://led_blinker.c \
           file://led-blinker.service"
S = "${WORKDIR}"

# Kế thừa class systemd để tự động handle enable/disable
inherit systemd

# Tên service file (phải khớp với tên file trong SRC_URI)
SYSTEMD_SERVICE:${PN} = "led-blinker.service"
# Auto-enable khi install vào image
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} led_blinker.c -o led_blinker
}

do_install() {
    # Cài binary
    install -d ${D}${bindir}
    install -m 0755 led_blinker ${D}${bindir}/led_blinker

    # Cài systemd service
    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/led-blinker.service \
        ${D}${systemd_unitdir}/system/led-blinker.service
}
```

---

## 5. Autotools và CMake

Với packages dùng autotools (./configure) hay CMake:

```bitbake
# Autotools — inherit autotools
inherit autotools
# BitBake tự chạy autoreconf, ./configure --host=arm-..., make, make install

# CMake — inherit cmake
inherit cmake
# BitBake tự chạy cmake -DCMAKE_TOOLCHAIN_FILE=..., make, make install

# Nếu cần pass thêm args cho configure:
EXTRA_OECONF = "--disable-gtk --enable-ssl"

# Nếu cần pass thêm args cho CMake:
EXTRA_OECMAKE = "-DENABLE_TESTS=OFF"
```

---

## 6. bbappend — Override Không Fork

`.bbappend` cho phép extend/override recipe có sẵn từ layer khác **mà không cần copy recipe gốc**:

```
Quy tắc đặt tên:
  recipe gốc: openssh_9.3.0.bb  (trong meta-openembedded)
  bbappend:   openssh_9.3.0.bbappend   (version cụ thể)
              openssh_%.bbappend        (% = bất kỳ version nào)
```

### Ví dụ 1: Thêm custom sshd_config

```
meta-bbb-custom/
└── recipes-connectivity/
    └── openssh/
        ├── openssh_%.bbappend
        └── files/
            └── sshd_config.bbb
```

```bitbake
# openssh_%.bbappend
# Thêm thư mục files/ của bbappend vào search path
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Thêm file vào SRC_URI
SRC_URI:append = " file://sshd_config.bbb"

# Chạy thêm sau do_install gốc
do_install:append() {
    install -m 0600 ${WORKDIR}/sshd_config.bbb \
        ${D}${sysconfdir}/ssh/sshd_config
}
```

### Ví dụ 2: Thêm kernel config fragment

```
meta-bbb-custom/
└── recipes-kernel/
    └── linux/
        ├── linux-ti_%.bbappend
        └── files/
            └── bbb-extras.cfg      ← kernel config fragment
```

```bitbake
# linux-ti_%.bbappend
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append = " file://bbb-extras.cfg"
```

```
# bbb-extras.cfg — Kernel config fragment cho BBB
CONFIG_OMAP_WATCHDOG=y          # Hardware watchdog
CONFIG_I2C_CHARDEV=y            # /dev/i2c-* devices
CONFIG_SPI_SPIDEV=y             # /dev/spidev* devices
CONFIG_CAN=y                    # CAN bus base
CONFIG_CAN_RAW=y                # RAW CAN socket
# CONFIG_FRAMEBUFFER_CONSOLE is not set   # Tiết kiệm memory
```

---

## 7. devtool — Rapid Development Workflow

`devtool` giúp tạo và iterate recipe nhanh hơn:

```bash
# ──── Tạo recipe mới từ source code ────
devtool add myapp /path/to/myapp/source
# Tạo: workspace/recipes/myapp/myapp_git.bb (auto-generated)
# Tạo: workspace/sources/myapp/ (link tới source của bạn)

# ──── Build và deploy lên BBB qua SSH ────
devtool build myapp
devtool deploy-target myapp root@192.168.7.2
# Binary được copy trực tiếp lên board → test ngay!

# ──── Sau khi test xong, undeploy ────
devtool undeploy-target myapp root@192.168.7.2

# ──── Finalize: chuyển recipe vào layer chính thức ────
devtool finish myapp meta-bbb-custom
# Recipe được copy vào meta-bbb-custom/recipes-.../myapp/

# ──── Modify recipe có sẵn ────
devtool modify linux-ti    # Checkout kernel source để edit
# Edits trực tiếp trong workspace/sources/linux-ti/
devtool build linux-ti
devtool finish linux-ti meta-bbb-custom  # Lưu thành patches

# ──── Upgrade recipe lên version mới ────
devtool upgrade myapp --version 2.0
```

---

## 8. Giải Quyết Dependencies

```bash
# Tìm recipe nào cung cấp một thư viện
bitbake -s | grep -i cjson
# libcjson           :1.7.15-r0

# Tìm package nào chứa file cụ thể
oe-pkgdata-util find-path /usr/lib/libcjson.so
# libcjson: /usr/lib/libcjson.so

# Xem dependency tree của một recipe
bitbake -g led-blinker && cat task-depends.dot | grep led-blinker

# Xem tất cả packages trong một image
bitbake -g core-image-minimal && cat pn-buildlist
```

---

## 9. Variables Thường Dùng Trong Recipe

| Biến | Ý nghĩa |
|-----|---------|
| `PN` | Package Name (tên recipe, VD: `led-blinker`) |
| `PV` | Package Version (VD: `1.0`) |
| `PR` | Package Revision (VD: `r0`) — tăng khi recipe thay đổi |
| `PF` | Package Full = `${PN}-${PV}-${PR}` |
| `WORKDIR` | Thư mục làm việc build |
| `S` | Source directory |
| `D` | Destination (staging area) |
| `B` | Build directory (khác S nếu out-of-tree build) |
| `T` | Temp directory (chứa logs) |

---

## 10. Câu hỏi kiểm tra

1. Sự khác biệt giữa `DEPENDS` và `RDEPENDS` là gì? Cho ví dụ cụ thể với `libi2c`.
2. Tại sao nên dùng `${bindir}` thay vì hard-code `/usr/bin`?
3. `inherit systemd` làm gì tự động? Nếu không inherit, bạn phải làm gì thêm?
4. Khi dùng `SRC_URI = "git://..."`, tại sao bắt buộc phải có `SRCREV`?
5. `openssh_%.bbappend` vs `openssh_9.0.bbappend` — khi nào dùng cái nào?
6. `FILESEXTRAPATHS:prepend := "${THISDIR}/files:"` — `:=` và `:prepend` có nghĩa gì?
