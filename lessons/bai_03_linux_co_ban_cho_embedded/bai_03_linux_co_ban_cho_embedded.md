# Bài 3 - Linux cơ bản cho Embedded

## 1. Mục tiêu bài học

- Hiểu cấu trúc filesystem trên Linux embedded (BBB)
- Biết các thư mục đặc biệt `/dev`, `/sys`, `/proc` dùng để làm gì
- Nắm các lệnh shell tối thiểu để làm việc với phần cứng
- Hiểu Device Tree là gì ở mức khái niệm: ai nói cho kernel biết board có những gì
- Phân biệt rõ: userspace, kernel space, và phần cứng

---

## 2. Kết nối với BeagleBone Black

Trước khi làm bất cứ thứ gì trên Linux, bạn cần kết nối vào board.

### 2.1 Qua USB Serial (phổ biến nhất khi học)

Cắm cáp micro-USB vào cổng USB debug trên BBB. Board sẽ xuất hiện như một cổng serial trên PC.

Trên Linux host:
```bash
# Tìm cổng serial
ls /dev/ttyACM*

# Kết nối (115200 baud, 8N1)
screen /dev/ttyACM0 115200
```

Trên Windows: dùng PuTTY hoặc Tera Term, chọn COM port tương ứng, baud rate 115200.

### 2.2 Qua SSH (nếu có mạng)

```bash
ssh root@192.168.7.2    # USB network
ssh root@<IP_address>   # Ethernet
```

### 2.3 Qua UART header (J1)

Dùng USB-to-UART adapter nối vào header J1 trên board. Đây là cách truyền thống nhất.

---

## 3. Shell cơ bản trên BBB

Khi đã login vào board, bạn đang ở **userspace** - nơi chương trình người dùng chạy.

### 3.1 Các lệnh cơ bản cần biết

| Lệnh | Ý nghĩa | Ví dụ |
|-------|---------|------|
| `ls` | Liệt kê file/thư mục | `ls /dev` |
| `cd` | Chuyển thư mục | `cd /sys/class/gpio` |
| `cat` | Đọc nội dung file | `cat /proc/cpuinfo` |
| `echo` | Ghi giá trị vào file | `echo 1 > /sys/class/leds/beaglebone:green:usr0/brightness` |
| `hexdump` | Đọc nội dung nhị phân | `hexdump -C /dev/mem` |
| `dmesg` | Xem log kernel | `dmesg | tail -20` |
| `lsmod` | Liệt kê kernel module đang load | `lsmod` |
| `devmem2` | Đọc/ghi trực tiếp địa chỉ vật lý | `devmem2 0x44E07000` |

### 3.2 Hai lệnh quan trọng nhất cho embedded

**`cat`** - đọc giá trị từ phần cứng qua file ảo:
```bash
cat /sys/class/gpio/gpio60/value    # đọc trạng thái chân GPIO
cat /proc/cpuinfo                    # xem thông tin CPU
```

**`echo`** - ghi giá trị xuống phần cứng qua file ảo:
```bash
echo 60 > /sys/class/gpio/export             # yêu cầu kernel export GPIO 60
echo out > /sys/class/gpio/gpio60/direction   # set GPIO 60 là output
echo 1 > /sys/class/gpio/gpio60/value         # bật GPIO 60 lên HIGH
```

Ý nghĩa: trên Linux, **ghi vào file ảo = điều khiển phần cứng**. Kernel sẽ nhận lệnh từ file ảo rồi ghi vào thanh ghi cho bạn.

---

## 4. Filesystem trên Linux embedded

### 4.1 Cấu trúc thư mục chính

```
/
├── bin/       ← chương trình cơ bản (ls, cat, echo...)
├── dev/       ← file đại diện cho thiết bị phần cứng
├── etc/       ← file cấu hình hệ thống
├── home/      ← thư mục người dùng
├── lib/       ← thư viện chia sẻ
├── proc/      ← thông tin kernel và process (ảo)
├── sys/       ← interface tới driver và phần cứng (ảo)
├── tmp/       ← file tạm
├── usr/       ← chương trình và thư viện mở rộng
└── var/       ← log, dữ liệu thay đổi
```

### 4.2 Ba thư mục quan trọng nhất cho embedded

#### `/dev` - Device files

Mỗi thiết bị phần cứng được kernel biểu diễn như một **file** trong `/dev`:

```
/dev/
├── mem          ← toàn bộ bộ nhớ vật lý (dùng cho mmap)
├── ttyS0        ← UART0 serial port
├── ttyS1        ← UART1 serial port
├── i2c-0        ← I2C bus 0
├── i2c-1        ← I2C bus 1
├── spidev1.0    ← SPI bus 1, chip select 0
├── mmcblk0      ← eMMC
├── mmcblk1      ← microSD
└── ...
```

**Ý nghĩa**: Thay vì phải biết địa chỉ thanh ghi, bạn mở file `/dev/ttyS0` rồi đọc/ghi như file bình thường → kernel driver sẽ lo phần thanh ghi.

#### `/sys` - Sysfs (System Filesystem)

Đây là nơi kernel **xuất thông tin phần cứng** ra cho userspace theo dạng cây thư mục:

**Ban đầu** (chưa export GPIO nào):
```bash
$ ls /sys/class/gpio/
export  gpiochip512  gpiochip544  gpiochip576  gpiochip608  unexport
```

- `export` - file để kích hoạt GPIO (ghi số GPIO vào đây)
- `unexport` - file để hủy kích hoạt GPIO
- `gpiochip*` - cây tổng hợp thông tin các GPIO module (đọc thấy chi tiết từng GPIO có sẵn)

**Sau khi export GPIO 60**:
```bash
$ echo 60 > /sys/class/gpio/export
$ ls /sys/class/gpio/
export  gpio60  gpiochip512  gpiochip544  gpiochip576  gpiochip608  unexport

$ ls /sys/class/gpio/gpio60/
direction  edge  value  ...
```

Mới xuất hiện thư mục `gpio60/` với các file:
- `direction` - "in" hoặc "out"
- `value` - "0" hoặc "1"
- `edge` - "none", "rising", "falling", "both"

**Ý nghĩa**: Mỗi file trong `/sys` tương ứng một thuộc tính phần cứng. Đọc/ghi file = đọc/ghi thuộc tính.

#### `/proc` - Process và Kernel info

```
/proc/
├── cpuinfo        ← thông tin CPU
├── meminfo        ← thông tin RAM
├── interrupts     ← bảng interrupt đang dùng
├── iomem          ← memory map các vùng I/O
├── device-tree/   ← device tree đang chạy
└── [PID]/         ← thông tin từng process
```

Ví dụ hữu dụng:
```bash
# Xem CPU là gì
cat /proc/cpuinfo

# Xem memory map
cat /proc/iomem

# Xem interrupt nào đang dùng
cat /proc/interrupts
```

---

## 5. Số GPIO trên Linux khác với tên GPIO trong TRM

Đây là điểm **rất hay gây nhầm lẫn**!

Trong TRM, ta dùng ký hiệu `GPIO1_29` (module 1, bit 29).

Nhưng Linux sysfs sử dụng một **số GPIO duy nhất (global number)**:

```
Công thức:
  linux_gpio_number = (module * 32) + bit

Ví dụ:
  GPIO1_29 → (1 × 32) + 29 = 61
  GPIO1_21 → (1 × 32) + 21 = 53
  GPIO0_7  → (0 × 32) + 7  = 7
  GPIO2_1  → (2 × 32) + 1  = 65
```

Vì vậy khi dùng sysfs:
```bash
# Muốn dùng GPIO1_29 (chân P8.26):
echo 61 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio61/direction
echo 1 > /sys/class/gpio/gpio61/value
```

> **Luôn nhớ chuyển đổi**: tên TRM → số Linux trước khi thao tác sysfs.

---

## 6. Ba tầng của hệ thống Linux embedded

```
┌───────────────────────────────────────┐
│         USERSPACE                     │
│  (chương trình C, shell, sysfs,      │
│   /dev/mem, mmap...)                  │
├───────────────────────────────────────┤
│         KERNEL SPACE                  │
│  (driver, interrupt handler,         │
│   scheduler, memory management,      │
│   device tree parser)                 │
├───────────────────────────────────────┤
│         PHẦN CỨNG                     │
│  (AM335x: GPIO, UART, I2C registers, │
│   DDR controller, PMIC...)            │
└───────────────────────────────────────┘
```

### 6.1 Userspace
- Nơi bạn viết code C hoặc chạy lệnh shell
- Không thể truy cập thanh ghi trực tiếp (trừ khi dùng `/dev/mem` + `mmap`)
- Giao tiếp với phần cứng qua `/dev`, `/sys`, hoặc system call

### 6.2 Kernel space
- Nơi driver chạy
- Có toàn quyền truy cập phần cứng
- Nhận request từ userspace, ghi vào thanh ghi phần cứng
- Quản lý interrupt, DMA, memory

### 6.3 Phần cứng
- AM335x SoC với các thanh ghi memory-mapped
- Chỉ kernel (hoặc bare-metal code) mới trực tiếp điều khiển

**Quy tắc quan trọng**: Trên Linux, userspace **không bao giờ** ghi thẳng vào thanh ghi phần cứng theo cách thông thường. Phải đi qua kernel, trừ khi mmap `/dev/mem`.

---

## 7. Device Tree - Ai nói cho kernel biết board có gì?

### 7.1 Vấn đề

Khi Linux kernel boot lên, nó cần biết:
- Board này có bao nhiêu GPIO module?
- UART nào đang hoạt động?
- I2C bus nào nối với sensor gì?
- Chân nào cần pinmux thế nào?

Kernel **không tự biết** những thứ này. Ai đó phải nói cho nó biết.

### 7.2 Giải pháp: Device Tree

**Device Tree** là một file mô tả phần cứng, được truyền cho kernel lúc boot.

```
Bootloader (U-Boot)
    │
    ├── Nạp kernel vào RAM
    └── Nạp Device Tree (.dtb) vào RAM
         │
         └── Kernel đọc Device Tree
              → biết board có GPIO0, GPIO1, GPIO2, GPIO3
              → biết UART0 ở 0x44E09000
              → biết I2C2 nối với sensor XYZ
              → biết pinmux cho từng chân
```

### 7.3 Device Tree Source (.dts) trông như thế nào?

```dts
/* Ví dụ rút gọn - không phải file thật */
/ {
    model = "TI AM335x BeagleBone Black";
    compatible = "ti,am335x-bone-black";

    /* Mô tả GPIO1 */
    gpio1: gpio@4804c000 {
        compatible = "ti,omap4-gpio";
        reg = <0x4804c000 0x1000>;    /* base address, size */
        interrupts = <98>;
        gpio-controller;
        #gpio-cells = <2>;
    };

    /* Mô tả UART0 */
    uart0: serial@44e09000 {
        compatible = "ti,am335x-uart";
        reg = <0x44e09000 0x2000>;
        interrupts = <72>;
    };
};
```

Nhìn vào đây bạn thấy:
- `reg = <0x4804c000 0x1000>` → base address và kích thước = **đúng là thông tin từ memory map**
- Kernel đọc cái này và biết: "GPIO1 nằm ở 0x4804C000, kích thước 4KB"
- Từ đó kernel load driver phù hợp cho GPIO1

### 7.4 .dts vs .dtb

| File | Ý nghĩa |
|------|---------|
| `.dts` | Device Tree Source - text, con người đọc được |
| `.dtb` | Device Tree Blob - nhị phân, kernel đọc |

Quá trình:
```
.dts  →  dtc (compiler)  →  .dtb  →  U-Boot nạp vào RAM  →  Kernel đọc
```

### 7.5 Device Tree Overlay

Ngoài file `.dtb` chính, Linux còn hỗ trợ **overlay** - cho phép thêm/sửa mô tả phần cứng **lúc runtime** mà không cần biên dịch lại toàn bộ.

Ví dụ: bạn cắm thêm một cape (board mở rộng) có LCD → load overlay để kernel biết có thêm thiết bị mới.

Chi tiết sẽ học ở **Bài 16 - Device Tree**.

---

## 8. Ví dụ tổng hợp: Bật LED USR0 bằng sysfs

LED USR0 trên BBB nối vào `GPIO1_21` → số Linux = `(1 × 32) + 21 = 53`

**Cách 1: Dùng GPIO interface** (cần export trước):
```bash
# Bước 1: Export GPIO 53
echo 53 > /sys/class/gpio/export

# Bước 2: Set direction = output
echo out > /sys/class/gpio/gpio53/direction

# Bước 3: Bật LED
echo 1 > /sys/class/gpio/gpio53/value

# Bước 4: Tắt LED
echo 0 > /sys/class/gpio/gpio53/value

# Bước 5: Hủy export khi xong
echo 53 > /sys/class/gpio/unexport
```

**Cách 2: Dùng LED interface** (thường sẵn có, không cần export):
```bash
# Kiểm tra LED sẵn có
ls /sys/class/leds/

# Bật USR0 (nếu có beaglebone:green:usr0)
echo 1 > /sys/class/leds/beaglebone:green:usr0/brightness

# Tắt USR0
echo 0 > /sys/class/leds/beaglebone:green:usr0/brightness
```

Chuyện gì xảy ra bên trong (cách 1)?

```
echo 1 > /sys/class/gpio/gpio53/value
         │
         ↓
    Kernel nhận lệnh qua sysfs
         │
         ↓
    GPIO driver ghi vào thanh ghi GPIO_SETDATAOUT
    tại 0x4804C194, bit 21
         │
         ↓
    Phần cứng: chân GPIO1_21 lên HIGH
         │
         ↓
    LED USR0 sáng
```

---

## 9. So sánh các cách truy cập phần cứng trên Linux

| Cách | Ví dụ | Ai ghi thanh ghi? | Mức độ khó | An toàn |
|------|-------|-------------------|-----------|---------|
| **sysfs** | echo/cat `/sys/class/gpio/...` | Kernel driver | Dễ nhất | Cao |
| **device file** | open/read/write `/dev/ttyS0` | Kernel driver | Trung bình | Cao |
| **mmap /dev/mem** | mmap() rồi ghi trực tiếp | Bạn tự ghi | Khó | Thấp |
| **kernel module** | Viết .ko, insmod | Code trong kernel | Khó nhất | Trung bình |

Trong khóa học này:
- Bài 3-4: sysfs + mmap
- Bài 5: GPIO qua mmap (hiểu thanh ghi sâu)
- Bài 16+: kernel module, driver

---

## 10. Kiến thức cốt lõi cần nhớ

1. Trên Linux, **mọi thứ là file** - kể cả phần cứng
2. `/dev` chứa device file, `/sys` chứa thuộc tính phần cứng, `/proc` chứa thông tin hệ thống
3. Số GPIO trên Linux = `(module × 32) + bit`
4. **Device Tree** nói cho kernel biết board có phần cứng gì, ở địa chỉ nào
5. Userspace không trực tiếp ghi thanh ghi - phải qua kernel hoặc mmap

---

## 11. Câu hỏi kiểm tra

1. Bạn muốn bật chân `GPIO2_3` trên Linux bằng sysfs. Số GPIO Linux là bao nhiêu? Viết các lệnh cần thiết.
2. `/dev/mem` dùng để làm gì? Tại sao nó nguy hiểm?
3. Device Tree giải quyết vấn đề gì? Ai tạo ra nó và ai đọc nó?
4. Phân biệt `/dev`, `/sys`, `/proc` - mỗi thư mục dùng trong trường hợp nào?
5. Khi bạn `echo 1 > /sys/class/gpio/gpio53/value`, ai thực sự ghi vào thanh ghi GPIO?

---

## 12. Chuẩn bị cho bài sau

Bài 4 sẽ đi vào:

**Memory-mapped I/O - /dev/mem và mmap()**

Ta sẽ:
- Viết code C đầu tiên để truy cập thanh ghi trực tiếp
- Hiểu mmap() ánh xạ physical address → virtual address
- Đọc/ghi thanh ghi GPIO qua code C
- So sánh với cách dùng sysfs

---

## 13. Ghi nhớ một câu

Trên Linux embedded, **bạn điều khiển phần cứng bằng cách đọc/ghi file** - kernel đứng giữa để dịch file operation thành register access.
