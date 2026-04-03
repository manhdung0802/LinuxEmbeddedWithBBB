# Bài 4 - Thiết lập môi trường phát triển

## 1. Mục tiêu bài học
- Cài đặt cross-compile toolchain cho ARM (arm-linux-gnueabihf-gcc)
- Clone và build Linux kernel source cho BeagleBone Black
- Hiểu cấu trúc source tree của Linux kernel
- Build out-of-tree kernel module đầu tiên
- Thiết lập kết nối SSH/serial tới BBB để deploy và test

---

## 2. Tại sao cần cross-compile?

BeagleBone Black dùng CPU ARM Cortex-A8, trong khi máy phát triển (host) thường dùng x86_64.

**Cross-compile** = biên dịch trên host (x86) nhưng tạo binary chạy trên target (ARM).

```
Host (x86_64 Ubuntu/Debian)          Target (ARM BBB)
┌─────────────────────┐              ┌──────────────────┐
│ arm-linux-gnueabihf │  ──scp──►    │ insmod module.ko │
│ -gcc / make         │              │ Linux kernel     │
└─────────────────────┘              └──────────────────┘
```

Lý do: BBB có tài nguyên hạn chế (512MB RAM, CPU 1GHz) → build kernel module trên host nhanh hơn rất nhiều.

---

## 3. Cài đặt Cross-Compile Toolchain

### 3.1 Trên Ubuntu/Debian host:

```bash
# Cài đặt toolchain và các tool cần thiết
sudo apt update
sudo apt install -y gcc-arm-linux-gnueabihf \
                    build-essential \
                    bc \
                    bison \
                    flex \
                    libssl-dev \
                    libncurses5-dev

# Kiểm tra version
arm-linux-gnueabihf-gcc --version
```

### 3.2 Kiểm tra toolchain hoạt động:

```bash
# Tạo file test đơn giản
echo 'int main() { return 0; }' > test.c
arm-linux-gnueabihf-gcc -o test test.c
file test
# Output: test: ELF 32-bit LSB executable, ARM, EABI5 ...
```

Nếu output có chữ "ARM" → toolchain hoạt động.

---

## 4. Clone Linux Kernel Source

### 4.1 Lấy kernel source phù hợp với BBB:

```bash
# Clone kernel source (dùng branch ổn định cho BBB)
git clone https://github.com/beagleboard/linux.git \
    --depth 1 --branch 5.10 ~/bbb-kernel

# Hoặc dùng kernel mainline
git clone https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git \
    --depth 1 --branch linux-5.10.y ~/linux-stable
```

### 4.2 Cấu trúc kernel source tree quan trọng:

```
linux/
├── arch/arm/           ← Mã nguồn cho kiến trúc ARM
│   ├── boot/dts/       ← Device Tree Source files
│   ├── configs/        ← Default kernel configs
│   └── mach-omap2/     ← Machine code cho AM335x (OMAP family)
├── drivers/            ← TẤT CẢ driver code
│   ├── gpio/           ← GPIO drivers
│   ├── i2c/            ← I2C drivers
│   ├── spi/            ← SPI drivers
│   ├── pwm/            ← PWM drivers
│   ├── iio/            ← IIO (ADC) drivers
│   └── ...
├── include/linux/      ← Kernel headers
│   ├── module.h        ← Kernel module API
│   ├── platform_device.h
│   ├── gpio/driver.h   ← GPIO driver API
│   └── ...
├── kernel/             ← Core kernel code
├── fs/                 ← Filesystem code
├── Documentation/      ← Kernel docs
├── Makefile            ← Top-level Makefile
└── Kconfig             ← Configuration system
```

Thư mục quan trọng nhất khi viết driver: `drivers/`, `include/linux/`, `arch/arm/boot/dts/`.

---

## 5. Build Kernel cho BBB

### 5.1 Cấu hình kernel:

```bash
cd ~/bbb-kernel

# Dùng default config cho BBB
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bb.org_defconfig

# Hoặc nếu dùng mainline kernel
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- omap2plus_defconfig

# (Tùy chọn) Mở menuconfig để tùy chỉnh
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig
```

### 5.2 Build kernel + modules:

```bash
# Build kernel image
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage

# Build Device Tree Blobs
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) dtbs

# Build tất cả in-tree modules
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) modules

# Install modules vào thư mục tạm
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
     INSTALL_MOD_PATH=~/bbb-modules modules_install
```

### 5.3 Các file output quan trọng:

| File | Đường dẫn | Mô tả |
|------|-----------|-------|
| zImage | `arch/arm/boot/zImage` | Kernel image nén |
| DTB | `arch/arm/boot/dts/am335x-boneblack.dtb` | Device Tree Binary |
| modules | `~/bbb-modules/lib/modules/` | Kernel modules (.ko) |

---

## 6. Build Out-of-Tree Kernel Module

"Out-of-tree" = module nằm ngoài source tree của kernel nhưng vẫn build dựa trên kernel headers.

### 6.1 Makefile chuẩn cho out-of-tree module:

```makefile
# Tên module (không có .ko)
obj-m += hello.o

# Đường dẫn tới kernel source đã build
KERNEL_DIR ?= ~/bbb-kernel

# Cross compiler prefix
CROSS_COMPILE ?= arm-linux-gnueabihf-
ARCH ?= arm

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
	     -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
	     -C $(KERNEL_DIR) M=$(PWD) clean
```

### 6.2 Giải thích Makefile:

- `obj-m += hello.o`: Khai báo file object cần build thành module
- `-C $(KERNEL_DIR)`: Chuyển vào thư mục kernel source, dùng Makefile của kernel
- `M=$(PWD)`: Chỉ build module ở thư mục hiện tại
- `modules`: Target build module

### 6.3 Build module:

```bash
make
# Output: hello.ko (kernel object file)

file hello.ko
# Output: hello.ko: ELF 32-bit LSB relocatable, ARM, EABI5 ...
```

---

## 7. Kết nối tới BBB

### 7.1 Qua USB-Serial (minicom/screen):

```bash
# Cài minicom
sudo apt install minicom

# Kết nối serial console (thường là /dev/ttyUSB0)
sudo minicom -D /dev/ttyUSB0 -b 115200
```

### 7.2 Qua SSH (USB network):

Khi BBB kết nối qua cổng USB client:
- BBB tự tạo network interface
- IP mặc định: `192.168.7.2` (BBB), `192.168.7.1` (host)

```bash
# SSH vào BBB
ssh debian@192.168.7.2
# Mật khẩu mặc định: temppwd

# Hoặc qua ethernet
ssh debian@10.42.0.206
```

### 7.3 Deploy module lên BBB:

```bash
# Copy module từ host sang BBB
scp hello.ko debian@192.168.7.2:/home/debian/

# Trên BBB: Load module
sudo insmod hello.ko

# Kiểm tra log
dmesg | tail

# Gỡ module
sudo rmmod hello

# Liệt kê module đang load
lsmod
```

---

## 8. Script build & deploy tự động

```bash
#!/bin/bash
# build_and_deploy.sh

MODULE_NAME="hello"
BBB_IP="192.168.7.2"
BBB_USER="debian"
BBB_PATH="/home/debian"

# Build
echo "=== Building module ==="
make clean && make
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Deploy
echo "=== Deploying to BBB ==="
scp ${MODULE_NAME}.ko ${BBB_USER}@${BBB_IP}:${BBB_PATH}/

# Load trên BBB
echo "=== Loading module on BBB ==="
ssh ${BBB_USER}@${BBB_IP} "sudo rmmod ${MODULE_NAME} 2>/dev/null; \
    sudo insmod ${BBB_PATH}/${MODULE_NAME}.ko && \
    dmesg | tail -5"

echo "=== Done ==="
```

---

## 9. Các lỗi thường gặp

### 9.1 "version magic" mismatch
```
insmod: ERROR: could not insert module hello.ko: Invalid module format
```
**Nguyên nhân**: Module build với kernel version khác kernel đang chạy trên BBB.
**Giải pháp**: Dùng đúng kernel source tương ứng với kernel trên BBB. Kiểm tra `uname -r` trên BBB.

### 9.2 "No such file or directory" khi make
**Nguyên nhân**: KERNEL_DIR trỏ sai hoặc kernel chưa được build.
**Giải pháp**: Build kernel trước (ít nhất `make prepare && make modules_prepare`).

### 9.3 Permission denied khi insmod
**Giải pháp**: Dùng `sudo insmod`.

---

## 10. Kiến thức cốt lõi sau bài này

1. **Cross-compile**: Dùng `arm-linux-gnueabihf-gcc` để build cho ARM trên host x86
2. **Kernel source tree**: Nắm vị trí `drivers/`, `include/linux/`, `arch/arm/boot/dts/`
3. **Out-of-tree module build**: Makefile với `obj-m`, `-C $(KERNEL_DIR) M=$(PWD) modules`
4. **Deploy workflow**: `make` → `scp` → `insmod` → `dmesg`
5. **Version matching**: Module phải build cùng kernel version đang chạy trên target

---

## 11. Câu hỏi kiểm tra

1. Tại sao cần cross-compile thay vì build trực tiếp trên BBB?
2. Biến `ARCH` và `CROSS_COMPILE` trong Makefile có ý nghĩa gì?
3. Lệnh `make -C $(KERNEL_DIR) M=$(PWD) modules` hoạt động ra sao?
4. Khi gặp lỗi "version magic", nguyên nhân là gì và sửa thế nào?
5. Làm sao kiểm tra module đã được load thành công trên BBB?

---

## 12. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 4 - Device Tree cơ bản**

Ta sẽ học cách Linux kernel biết board có những gì thông qua Device Tree — cơ chế mô tả phần cứng bằng file text cấu trúc dạng cây.
