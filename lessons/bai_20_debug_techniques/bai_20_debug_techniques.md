# Bài 20 - Debug Techniques: JTAG, printk, /proc, /sys, ftrace

## 1. Mục tiêu bài học

- Sử dụng `printk` hiệu quả với đúng log level
- Khai thác `/proc` và `/sys` để debug hệ thống
- Dynamic debug — bật/tắt debug message runtime
- Dùng ftrace để trace kernel function calls
- `strace`/`ltrace` để debug userspace
- JTAG với OpenOCD cho hardware-level debug

---

## 2. printk và Log Levels

### 2.1 Log Levels

```c
/* Kernel log levels (linux/kern_levels.h) */
#define KERN_EMERG    KERN_SOH "0"  /* hệ thống sắp crash */
#define KERN_ALERT    KERN_SOH "1"  /* cần xử lý ngay */
#define KERN_CRIT     KERN_SOH "2"  /* lỗi nghiêm trọng */
#define KERN_ERR      KERN_SOH "3"  /* lỗi */
#define KERN_WARNING  KERN_SOH "4"  /* cảnh báo */
#define KERN_NOTICE   KERN_SOH "5"  /* thông tin quan trọng */
#define KERN_INFO     KERN_SOH "6"  /* thông tin */
#define KERN_DEBUG    KERN_SOH "7"  /* debug */

/* In code, dùng pr_* shortcuts: */
pr_emerg("System crash imminent!\n");
pr_err("Failed to allocate memory: %d\n", ret);
pr_warn("Value %d is out of expected range\n", val);
pr_info("Driver initialized successfully\n");
pr_debug("Register value: 0x%08x\n", readl(base + REG));

/* dev_* variants (dùng trong driver, kèm device name vào log) */
dev_err(&pdev->dev, "probe failed: %d\n", ret);
dev_info(&pdev->dev, "Found device at 0x%x\n", addr);
```

### 2.2 Console Log Level

```bash
# Xem 4 giá trị: current | default | minimum | boot-default
cat /proc/sys/kernel/printk
# 7  4  1  7

# current level: 7 = hiển thị tất cả (0-6)
# Chỉ message có level < current_level mới in ra console

# Tạm thời in tất cả (kể cả KERN_DEBUG)
echo 8 > /proc/sys/kernel/printk

# Tắt message trên console (chỉ vào dmesg)
echo 1 > /proc/sys/kernel/printk
```

### 2.3 dmesg Commands

```bash
dmesg                           # toàn bộ kernel ring buffer
dmesg | tail -30                # 30 dòng cuối
dmesg -w                        # watch live (như tail -f)
dmesg -l err,warn               # lọc chỉ err và warn
dmesg -l debug                  # chỉ debug messages
dmesg -T                        # timestamp dạng human-readable
dmesg -c                        # đọc rồi xóa buffer
dmesg --follow                  # stream live (kernel 3.5+)
```

---

## 3. Dynamic Debug

Dynamic debug cho phép **bật/tắt pr_debug()** của module cụ thể **runtime**, không cần recompile.

### 3.1 Bật debugfs

```bash
# Kiểm tra debugfs đã mount chưa
mount | grep debugfs
# nếu chưa:
mount -t debugfs none /sys/kernel/debug
```

### 3.2 Điều khiển Dynamic Debug

```bash
# Bật debug cho toàn bộ module "mychar_driver"
echo "module mychar_driver +p" > /sys/kernel/debug/dynamic_debug/control

# Tắt debug
echo "module mychar_driver -p" > /sys/kernel/debug/dynamic_debug/control

# Bật debug cho file cụ thể
echo "file mychar_driver.c +p" > /sys/kernel/debug/dynamic_debug/control

# Bật debug cho function cụ thể
echo "func mychar_read +p" > /sys/kernel/debug/dynamic_debug/control

# Xem tất cả dynamic debug points hiện tại
cat /sys/kernel/debug/dynamic_debug/control | grep mychar

# Bật kèm thêm thông tin (file, line, function)
echo "module mychar_driver +pflmt" > /sys/kernel/debug/dynamic_debug/control
# p = print, f = function name, l = line number, m = module, t = thread id
```

---

## 4. /proc — Process và Kernel Info

```bash
# Thông tin interrupt
cat /proc/interrupts
# CPU0
#   3:    12345    INTC  57  Edge     GPIO 1 bank 21
#   71:       0   INTC  71  Level    uart1

# Thông tin memory map (physical)
cat /proc/iomem
# 44e00000-44e00fff : 44e00000.cm
# 44e07000-44e07fff : 44e07000.gpio
# 48022000-48023fff : 48022000.serial

# Devices đã đăng ký
cat /proc/devices
# Character devices:
#   1 mem
#   4 /dev/vc/0
#   240 mychar    ← driver ta tạo

# Modules đang load
cat /proc/modules
# mychar_driver 16384 0 - Live 0xbf000000

# Xem memory usage
cat /proc/meminfo
free -h

# Xem CPU info (kiểm tra ARM Cortex-A8 model)
cat /proc/cpuinfo
```

---

## 5. /sys — Sysfs: Hardware Info

```bash
# Xem tất cả platform devices
ls /sys/bus/platform/devices/

# Xem driver của device
ls -la /sys/bus/platform/devices/44e07000.gpio/driver
# → /sys/bus/platform/drivers/gpio-omap

# GPIO sysfs interface
ls /sys/class/gpio/
echo 53 > /sys/class/gpio/export       # export GPIO1_21 (bank1 × 32 + 21 = 53)
echo out > /sys/class/gpio/gpio53/direction
echo 1 > /sys/class/gpio/gpio53/value  # bật LED
echo 0 > /sys/class/gpio/gpio53/value  # tắt LED
cat /sys/class/gpio/gpio53/value       # đọc trạng thái
echo 53 > /sys/class/gpio/unexport     # release

# Xem power management
cat /sys/kernel/debug/clk/clk_summary  # tất cả clocks và tần số

# Module parameters
cat /sys/module/mychar_driver/parameters/gpio_pin
```

---

## 6. ftrace — Function Tracing

ftrace là tracer tích hợp trong kernel, không cần tool ngoài.

```bash
# Mount debugfs nếu chưa có
mount -t debugfs none /sys/kernel/debug

cd /sys/kernel/debug/tracing

# Xem tracers có sẵn
cat available_tracers
# blk function_graph function nop ...

# === Function Tracer cơ bản ===
echo function > current_tracer   # bật function tracer
echo 1 > tracing_on              # bắt đầu trace

# ... thực hiện hành động muốn trace ...
echo hellowrite > /dev/mychar

echo 0 > tracing_on              # dừng trace
cat trace | head -50             # xem kết quả
echo nop > current_tracer        # tắt tracer

# === Chỉ trace function cụ thể ===
echo "mychar_write" > set_ftrace_filter
echo function > current_tracer
echo 1 > tracing_on
# ... test ...
echo 0 > tracing_on
cat trace

# Reset
echo "" > set_ftrace_filter
echo nop > current_tracer

# === Function Graph (hiện call tree và thời gian) ===
echo function_graph > current_tracer
echo "mychar_write" > set_graph_function
echo 1 > tracing_on
echo "test" > /dev/mychar
echo 0 > tracing_on
cat trace
# Kết quả dạng:
# 0)               |  mychar_write() {
# 0)   1.234 us    |    copy_from_user();
# 0)   0.456 us    |    mutex_lock();
# 0) + 5.678 us    |  }
```

---

## 7. strace và ltrace (Userspace Debug)

```bash
# strace: trace system calls của process
strace ./my_app

# Chỉ trace syscall cụ thể
strace -e trace=open,read,write,ioctl ./my_app

# Trace app đang chạy (by PID)
strace -p 1234

# Lưu ra file
strace -o trace.log ./my_app

# ltrace: trace library calls (libc, etc.)
ltrace ./my_app

# Ví dụ với GPIO test app
strace -e trace=open,ioctl,close ./gpio_test
# open("/dev/mychar", O_RDWR)         = 3
# ioctl(3, 0x4700, 0)                  = 0  ← GPIO_LED_ON
# close(3)                             = 0
```

---

## 8. JTAG với OpenOCD trên BBB

BBB có cổng JTAG 20-pin ở J1 (không nằm trên P8/P9).

### 8.1 Kết nối

```
BBB JTAG J1          JTAG Adapter
──────────────────────────────────
Pin 1  VTref     →   VTREF (3.3V reference)
Pin 3  nTRST     →   nTRST
Pin 5  TDI       →   TDI
Pin 7  TMS       →   TMS
Pin 9  TCK       →   TCK
Pin 11 RTCK      →   RTCK
Pin 13 TDO       →   TDO
Pin 15 nRESET    →   nRESET
Pin 4,6,8,... GND → GND
```

### 8.2 OpenOCD Script cho BBB

```tcl
# bbb.cfg
source [find interface/ftdi/olimex-arm-usb-tiny-h.cfg]

transport select jtag

set _CHIPNAME am335x
set _CPUTAPID 0x0B94402F

jtag newtap $_CHIPNAME cpu -irlen 4 -expected-id $_CPUTAPID

set _TARGETNAME $_CHIPNAME.cpu
target create $_TARGETNAME cortex_a -chain-position $_TARGETNAME

adapter speed 1000
```

```bash
# Khởi động OpenOCD
openocd -f bbb.cfg

# Kết nối từ terminal khác (port 4444)
telnet localhost 4444

# Các lệnh OpenOCD:
> halt              # dừng CPU
> reg               # xem tất cả registers
> mdw 0x4804C000    # read word tại địa chỉ GPIO1
> mww 0x48200000 1  # write word
> resume            # tiếp tục chạy
> reset             # reset board

# Debug với GDB
arm-linux-gnueabi-gdb vmlinux
(gdb) target remote localhost:3333
(gdb) info registers
(gdb) x/10i $pc     # disassemble tại PC
```

---

## 9. Tổng hợp: Quy trình Debug Một Vấn đề

### Kịch bản: Driver không hoạt động sau khi insmod

```bash
# Bước 1: Kiểm tra module đã load chưa
lsmod | grep my_driver
dmesg | tail -20       # có error message không?

# Bước 2: Kiểm tra device node
ls -la /dev/my*
cat /proc/devices | grep my

# Bước 3: Kiểm tra DT binding (nếu platform driver)
ls /sys/bus/platform/devices/ | grep my
cat /sys/bus/platform/devices/my_device/uevent

# Bước 4: Bật dynamic debug
echo "module my_driver +p" > /sys/kernel/debug/dynamic_debug/control
# Thử lại action → xem pr_debug() output qua dmesg

# Bước 5: Ftrace nếu cần trace execution flow
echo function_graph > /sys/kernel/debug/tracing/current_tracer
echo "my_probe" > /sys/kernel/debug/tracing/set_graph_function
echo 1 > /sys/kernel/debug/tracing/tracing_on
# ... trigger probe ...
cat /sys/kernel/debug/tracing/trace

# Bước 6: Kiểm tra memory map (register đúng không?)
cat /proc/iomem | grep my_device
```

---

## 10. Câu hỏi kiểm tra

1. `cat /proc/sys/kernel/printk` trả về `4 4 1 7` — message nào được in ra console?
2. Dynamic debug và `#define DEBUG` khác nhau thế nào?
3. Lệnh nào dùng để xem GPIO nào đang ở trạng thái HIGH/LOW qua sysfs?
4. `ftrace function_graph` cho biết thêm thông tin gì so với `function`?
5. Tại sao JTAG hữu ích hơn `printk` trong một số trường hợp?

---

## 11. Tài liệu tham khảo

| Nội dung | Tài liệu | Ghi chú |
|----------|----------|---------|
| printk levels | linux/kern_levels.h | KERN_ERR = 3, KERN_DEBUG = 7 |
| dynamic_debug | Documentation/admin-guide/dynamic-debug-howto.rst | Trong kernel docs |
| ftrace | Documentation/trace/ftrace.rst | Trong kernel docs |
| JTAG pinout BBB | BBB_SRM_C.pdf | J1 connector pinout |
| OpenOCD AM335x | openocd.org docs | cortex_a target |
