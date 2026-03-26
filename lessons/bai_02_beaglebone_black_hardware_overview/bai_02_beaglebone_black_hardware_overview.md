# Bài 2 - BeagleBone Black Hardware Overview

## 1. Mục tiêu bài học

- Hiểu sơ đồ khối của BeagleBone Black
- Biết các thành phần chính ngoài AM335x
- Nắm được P8/P9 header là gì, chân nào nối vào module nào
- Hiểu khái niệm multiplexing ở mức pinmux
- Xác định khi nào cần tra bảng pinout để tìm chân phù hợp

---

## 2. BeagleBone Black là cái gì?

BeagleBone Black (BBB) không phải chỉ là một chip AM335x.

Nó là một **board phát triển** (development board) đầy đủ, tức là:
- Chip AM335x
- Bộ nhớ DDR3 bên ngoài
- eMMC để lưu hệ điều hành
- Nguồn điện, voltage regulator
- Các header (P8, P9) để người dùng nối thêm thứ khác
- Các IC hỗ trợ (PMIC, oscillator, PHY ethernet, v.v.)

Mục đích: giúp lập trình viên học tập, phát triển ứng dụng mà không cần thiết kế PCB riêng.

Nguồn: `BBB_docs/datasheets/BBB_SRM_C.pdf` (BeagleBone Black System Reference Manual)

---

## 3. Sơ đồ khối tổng quan board

```
┌─────────────────────────────────────────────────────────────┐
│  BeagleBone Black Board                                     │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  AM335x SoC (tất cả ngoại vi bên trong)            │  │
│  │  - CPU Cortex-A8                                    │  │
│  │  - GPIO, UART, I2C, SPI, Timer, ADC                │  │
│  │  - DDR controller                                  │  │
│  │  - PRU-ICSS                                        │  │
│  └──────────────────────────────────────────────────────┘  │
│      ↓       ↓        ↓        ↓                           │
│   DDR3   eMMC   PMIC  RTC  Ethernet  USB  JTAG   ...     │
│   512M   4GB   TPS...  ...    PHY    PHY   ...           │
│                                                             │
│  Header P8/P9 ← kết nối các GPIO, UART, I2C, SPI ra ngoài│
└─────────────────────────────────────────────────────────────┘
```

Các thành phần quan trọng:

### 3.1 AM335x SoC (CPU chính)
- Cortex-A8 @ 1 GHz
- Chạy Linux, baremetcal, hoặc firmware khác
- Điều khiển mọi thứ trên board

### 3.2 DDR3 512 MB
- Bộ nhớ bên ngoài (external RAM)
- Nơi Linux kernel, rootfs, userspace program chạy
- AM335x có **DDR controller** bên trong để quản lý chip RAM này
- Địa chỉ vật lý (physical address) của DDR: `0x80000000` tới `0x9FFFFFFF` (512 MB)

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, trang 179 - EMIF0 SDRAM về Memory Map

### 3.3 eMMC 4 GB (Embedded MultiMediaCard)
- Lưu trữ dài hạn, tương tự SSD
- Chứa U-Boot bootloader, Linux kernel, root filesystem
- Khi bật board lên, bộ Rom boot của AM335x sẽ nạp eMMC vào RAM rồi chạy
- Ngoài ra còn có thể boot từ microSD

### 3.4 PMIC (Power Management IC)
- TPS65217C hoặc tương tự
- Lấy nguồn từ cổng Micro-USB (5V)
- Chia thành nhiều rail điện:
  - 1.8V cho core
  - 3.3V cho I/O
  - 1.5V cho DDR
  - v.v.
- Rất quan trọng: không có PMIC thì không có mạch hoạt động

Nguồn: `BBB_docs/datasheets/BBB_SRM_C.pdf`, power section

### 3.5 RTC Backup (Real Time Clock)
- Chạy độc lập, giữ thời gian khi board tắt
- Có pin backup, thường là pin nút
- Dùng cho ứng dụng yêu cầu thời gian chính xác

### 3.6 Ethernet PHY (LAN8710A hay tương tự)
- IC kết nối với EMAC của AM335x
- Cung cấp giao tiếp ethernet 10/100 Mbps
- Có LED status, link indicator

### 3.7 USB PHY
- Chip PHY cho USB trên AM335x
- Cho phép kết nối USB device hoặc host

### 3.8 JTAG Header
- Để debug với external JTAG adapter
- Hữu dụng khi cần trace, single stepping mã bare-metal

---

## 4. Oscillator (Clock source)

AM335x cần một xung nhịp (clock) từ bên ngoài để hoạt động.

BeagleBone Black có một **oscillator 24 MHz** ngoài chip.

Lý do:
- AM335x không có oscillator tích hợp sẵn
- Sử dụng 24 MHz làm đầu vào cho **PLL (Phase Locked Loop)** bên trong
- PLL sẽ **nhân** lên thành 1 GHz cho CPU, 192 MHz cho DDPLL, v.v.

Chi tiết: `BBB_docs/datasheets/spruh73q.pdf`, chapter về Clock Management

---

## 5. Header P8/P9 - Kết nối với ngoài

BeagleBone Black có **2 header chính**:
- **P8**: 46 chân
- **P9**: 46 chân

Tổng cộng 92 chân để người dùng kết nối thiết bị ngoài (LED, nút bấm, cảm biến, v.v.).

### 5.1 Mỗi chân có thể làm gì?

Mỗi chân trên P8/P9 có thể được config làm:
- **GPIO** (digital input/output)
- **UART** (serial)
- **I2C** (clock/data)
- **SPI** (clock/MOSI/MISO/CS)
- **PWM** (pulse width modulation)
- **ADC** (analog input)
- **Timer** input/output
- **Các chức năng khác**

Ý tưởng:
- AM335x bên trong chỉ có những GPIO, UART, I2C modules nhất định (vài chục chân)
- Nhưng sao có thể kết nối 92 chân ra ngoài?
- **Đáp án: Pinmux (Pin Multiplexing)**

---

## 6. Pinmux - Khái niệm quan trọng

Pinmux là cơ chế để **một chân vật lý** có thể đảm nhận **nhiều chức năng khác nhau**.

### 6.1 Ý tưởng:
```
┌──────────────────────────────────────┐
│  Chân P8.3 trên board (vật lý)      │
└──────────────────────────────────────┘
                ↓
Bên trong AM335x, chân này có thể nối tới:
  - GPIO1_6 (GPIO module 1, bit 6)
  - UART3_DTR
  - TIMER7_OUT
  - ...hay các chức năng khác
  
Thông qua thanh ghi **CONTROL MODULE**, ta config:
  "Chân P8.3 hôm nay tôi muốn dùng làm GPIO1_6"
  "Chân P8.4 hôm nay tôi muốn dùng làm UART3_RxD"
```

### 6.2 Thanh ghi Pinmux nằm ở đâu?

Tất cả pinmux config nằm trong **Control Module**, `base address 0x44E10000`.

Mỗi chân vật lý có một thanh ghi `CONF_<module>_<pin>` để config chức năng.

Ví dụ:
- `CONF_GPMC_AD0` (offset 0x840) - pin GPMC_AD0 trên SoC
- `CONF_GPMC_AD1` (offset 0x844) - pin GPMC_AD1 trên SoC
- ... và còn hàng trăm thanh ghi khác

Mỗi thanh ghi này có field:
- **Bit [2:0]**: Select function (0=GPIO, 1=UART, 2=I2C, ...)
- **Bit [3]**: Pull enable
- **Bit [4]**: Pull direction (up/down)
- **Bit [5]**: Receiver enable
- **Bit [5]**: Receiver enable (chân có nhận tín hiệu input không)
- Và các bit khác

Ví dụ cấu hình chân P8.3 thành GPIO output:
```
Thanh ghi: CONF_GPMC_AD6 tại 0x44E10818
Ghi giá trị: 0b000111
               ││││││
               │││││└─ bit0 ┐
               ││││└── bit1 ├─ mode = 111 = GPIO
               │││└─── bit2 ┘
               ││└──── bit3 = 1: pull enable
               │└───── bit4 = 0: pull-down
               └────── bit5 = 0: receiver disable (output)
```

### 6.3 Control Module là "bảng điều phối" của chip

Control Module **không điều khiển GPIO, UART hay bất kỳ ngoại vi nào** - nó là **trung tâm cấu hình toàn chip**:

```
          Control Module (0x44E10000)
               │
    ┌────────────────┼────────────────┐
    ↓                ↓                ↓
 Pinmux config    Pull up/down    Receiver enable
(chân làm gì)   (kéo lên/xuống) (có đọc input không)
```

> **Nguyên tắc**: Trước khi dùng bất kỳ chân nào trên P8/P9, bước đầu tiên luôn là **ghi vào Control Module** để chip biết chân đó sẽ làm gì. Sau đó mới cấu hình module GPIO/UART/I2C tương ứng.

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, **Chapter 9 - Control Module**, Pad Configuration Register

---

## 7. Cách tìm chân P8/P9 nối vào module nào

Khi muốn dùng GPIO hoặc UART hoặc I2C từ P8/P9, bạn cần:

1. **Biết muốn dùng chức năng nào** (ví dụ: "Tôi muốn GPIO input ở chân P8.3")
2. **Tra bảng pinout** để tìm xem chân P8.3 tương ứng với GPIO nào

Ví dụ từ `BBB_docs/datasheets/BeagleboneBlackP8HeaderTable.pdf`:

| Pin | Function 0 | Function 1 | Function 2 | Function 3 | Function 4 | Function 5 | Function 6 | GPIO |
|-----|-----------|-----------|-----------|-----------|-----------|-----------|-----------|------|
| P8.3 | GPIO1_6 | ... | ... | ... | ... | ... | ... | 1.6 |
| P8.4 | GPIO1_7 | ... | ... | ... | ... | ... | ... | 1.7 |
| P8.26 | GPIO1_29 | ... | ... | ... | ... | ... | ... | 1.29 |

Cột "GPIO" cho ta biết: **chân P8.3 là GPIO1_6**, tức là **GPIO module 1, bit 6**.

### 7.1 GPIO1_6 có nghĩa là gì?

`GPIO1_6` = **tên chân**, không phải tên thanh ghi. Cú pháp đọc như sau:
- `GPIO1` = GPIO **Module** số 1
- `_6` = **bit thứ 6** trong các thanh ghi data của module đó

### 7.2 Cấu trúc bên trong GPIO Module

**GPIO Module 1** là một **khối phần cứng** chứa khoảng **26 thanh ghi** bên trong. Mỗi thanh ghi rộng **32-bit**.

```
GPIO Module 1 (base: 0x4804C000)
│
├── GPIO_OE          (offset 0x134)  ← direction: 0=output, 1=input
├── GPIO_DATAIN      (offset 0x138)  ← đọc trạng thái chân input
├── GPIO_DATAOUT     (offset 0x13C)  ← ghi output
├── GPIO_SETDATAOUT  (offset 0x194)  ← ghi 1 để set bit output
├── GPIO_CLEARDATAOUT(offset 0x190)  ← ghi 1 để clear bit output
├── GPIO_IRQSTATUS   (offset 0x02C)  ← trạng thái interrupt
├── GPIO_SYSCONFIG   (offset 0x010)  ← cấu hình nội bộ module
└── ...~19 thanh ghi khác
```

> **Lưu ý**: Không phải tất cả thanh ghi đều map bit→chân. Các thanh ghi data (OE, DATAIN, DATAOUT...) thì mỗi bit = 1 chân. Các thanh ghi cấu hình nội bộ (SYSCONFIG, REVISION...) thì không.

### 7.3 Tại sao 26 thanh ghi nhưng quản lý 32 chân?

**Hai con số hoàn toàn độc lập nhau:**

| | Ý nghĩa |
|---|---|
| **26 thanh ghi** | Số lượng thanh ghi cần thiết để cấu hình mọi chức năng của module |
| **32 chân** | Độ rộng bit của mỗi thanh ghi data (32-bit = quản lý 32 chân cùng lúc) |

```
GPIO_DATAOUT (32-bit):
 Bit: 31 30 29 ... 8  7  6  5  4  3  2  1  0
                    │
                    └─ GPIO1_6 = chân P8.3
```

### 7.4 Mỗi module có vùng địa chỉ riêng

**26 thanh ghi không chia cho 4 module.** Mỗi module có **26 thanh ghi riêng của nó**, cùng layout, nhưng nằm ở địa chỉ vật lý khác nhau:

```
GPIO Module 0: 0x44E07000 → 0x44E07FFF  (4KB)
GPIO Module 1: 0x4804C000 → 0x4804CFFF  (4KB)
GPIO Module 2: 0x481AC000 → 0x481ACFFF  (4KB)
GPIO Module 3: 0x481AE000 → 0x481AEFFF  (4KB)
```

Ví dụ - thanh ghi `GPIO_DATAOUT` (offset `0x13C`) của từng module:

| Module | Địa chỉ thực |
|--------|-------------|
| GPIO0 | `0x44E0713C` |
| GPIO1 | `0x4804C13C` |
| GPIO2 | `0x481AC13C` |
| GPIO3 | `0x481AE13C` |

### 7.5 Tóm tắt: chân GPIO1_6 → ghi/đọc thanh ghi nào?

```
Chân GPIO1_6 (P8.3)
   │
   ├── cấu hình direction → GPIO_OE       (0x4804C134), bit 6
   ├── ghi output         → GPIO_DATAOUT  (0x4804C13C),  bit 6
   └── đọc input          → GPIO_DATAIN   (0x4804C138),  bit 6
```

Nguồn chi tiết cho toàn bộ mục 7: `BBB_docs/datasheets/spruh73q.pdf`, **Chapter 25 - GPIO Registers**

Chi tiết thực hành sẽ học ở **Bài 5 - GPIO**.

---

## 8. Các bảng pinout quan trọng

### P8 Header:
- Tra ở: `BBB_docs/datasheets/BeagleboneBlackP8HeaderTable.pdf`
- Bảng liệt kê tất cả 46 chân P8
- Cột "Signal Name" là tên chân trên SoC
- Cột "GPIO" là GPIO tương ứng nếu dùng function 0

### P9 Header:
- Tra ở: `BBB_docs/datasheets/BeagleboneBlackP9HeaderTable.pdf`
- Tương tự P8

### Schematic:
- `BBB_docs/schematics/BBB_SCH.pdf`
- Vẽ sơ đồ điện toàn bộ board
- Hữu dụng để hiểu dòng điện, pull-up/down, capacitor

---

## 9. Ví dụ thực tế: Sử dụng LED trên board

BeagleBone Black có **3 LED** tích hợp trên board:
- **USR0, USR1, USR2, USR3** - LED xanh lam

Các LED này nối vào GPIO cụ thể. Để bật/tắt, ta:

1. Tra bảng pinout biết LED0 nối vào GPIO nào
2. Bật clock cho GPIO module đó
3. Config pinmux
4. Ghi giá trị vào thanh ghi GPIO

Ví dụ (sẽ chi tiết hơn ở bài 5):
- `USR0` nối vào `GPIO1_21`
- Để bật LED: ghi `1` vào thanh ghi output của GPIO1_21
- Để tắt LED: ghi `0`

---

## 10. Boot sequence - Sao board bật lên được?

Khi bấm nút power:

1. **ROM Boot Code** (nằm trong Boot ROM bên trong chip)
   - Khởi tạo PMIC, DDR, oscillator
   - Tìm bootloader ở eMMC hoặc microSD
   
2. **U-Boot** (bootloader)
   - Khởi tạo thêm các thiết bị
   - Nạp Linux kernel vào RAM
   
3. **Linux Kernel**
   - Khởi động hệ điều hành
   - Nạp device driver
   - Khởi động userspace

4. **Userspace**
   - Shell, daemons, ứng dụng chạy

Chi tiết: `BBB_docs/datasheets/BBB_SRM_C.pdf`, boot section

---

## 11. Sơ đồ giai đoạn học sắp tới

Bạn đã học xong Bài 1 (AM335x architecture) và Bài 2 (BBB physical layout).

Bây giờ:
- **Bài 3**: Device Tree cơ bản - cách Linux biết board có những gì
- **Bài 4**: Memory-mapped I/O và /dev/mem
- **Bài 5**: GPIO - code bật/tắt LED

---

## 12. Câu hỏi kiểm tra

Trả lời ngắn gọn:

1. BeagleBone Black ngoài AM335x SoC còn có những thành phần chính nào?
2. Tại sao pinmux lại quan trọng? Nó cho phép làm gì?
3. Bảng pinout (P8/P9) được dùng để làm gì? Thông tin chủ yếu là gì?
4. Nếu bạn muốn dùng chân P8.26 làm GPIO, bạn sẽ tra ở đâu để biết chân này nối vào GPIO module/bit nào?
5. PMIC là gì? Nếu PMIC không hoạt động, sẽ xảy ra gì?

---

## 13. Tài liệu tham khảo chính cho Bài 2

1. **BBB System Reference Manual**: `BBB_docs/datasheets/BBB_SRM_C.pdf`
   - Diagram sơ đồ khối
   - Power supply, PMIC spec
   - Memory map

2. **AM335x Technical Reference Manual**: `BBB_docs/datasheets/spruh73q.pdf`
   - Control Module, Pinmux registers
   - Memory map chi tiết

3. **P8 Header Pinout**: `BBB_docs/datasheets/BeagleboneBlackP8HeaderTable.pdf`
4. **P9 Header Pinout**: `BBB_docs/datasheets/BeagleboneBlackP9HeaderTable.pdf`

5. **BeagleBone Black Schematic**: `BBB_docs/schematics/BBB_SCH.pdf`
   - Toàn bộ mạch điện

6. **BBB Overview**: `BBB_docs/datasheets/beaglebone-black.pdf`
   - Tổng quan, specification

---

## 14. Ghi nhớ một câu

Trên BeagleBone Black, **pinmux là chìa khóa** để biến một chân vật lý thành bất kỳ chức năng nào bạn muốn. Tìm hiểu Control Module = tìm hiểu cách "lập trình" các chân.

---

**Bạn đã sẵn sàng trả lời các câu hỏi kiểm tra để xác nhận bài 2 chưa?**

Hay bạn muốn tôi giải thích lại bất kỳ phần nào?
