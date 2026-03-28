# Bài 20 - Debug Techniques: Tham Khảo Nhanh

## printk / pr_* levels
| Macro | Level | Dùng khi |
|-------|-------|----------|
| pr_emerg/pr_alert/pr_crit | 0-2 | Crash, hardware fail |
| pr_err | 3 | Lỗi cần fix |
| pr_warn | 4 | Đáng chú ý nhưng tiếp tục được |
| pr_info | 6 | Thông tin thường |
| pr_debug | 7 | Debug, chỉ in khi dynamic_debug bật |

## dmesg
```bash
dmesg -w               # live watch
dmesg -l err,warn      # chỉ lỗi
dmesg -T               # timestamp đẹp
echo 8 > /proc/sys/kernel/printk  # hiện tất cả
```

## Dynamic Debug
```bash
echo "module my_mod +p" > /sys/kernel/debug/dynamic_debug/control
echo "func my_func +pflmt" > /sys/kernel/debug/dynamic_debug/control
```

## /proc quan trọng
```bash
/proc/interrupts    # IRQ counters
/proc/iomem         # physical memory map
/proc/devices       # major numbers
/proc/modules       # loaded modules
```

## GPIO sysfs
```bash
echo 53 > /sys/class/gpio/export       # GPIO1_21 = 32+21=53
echo out > /sys/class/gpio/gpio53/direction
echo 1 > /sys/class/gpio/gpio53/value
echo 53 > /sys/class/gpio/unexport
```

## ftrace
```bash
cd /sys/kernel/debug/tracing
echo function_graph > current_tracer
echo "my_func" > set_graph_function
echo 1 > tracing_on ; ... ; echo 0 > tracing_on
cat trace
echo nop > current_tracer   # cleanup
```

## strace
```bash
strace -e trace=open,ioctl ./app   # filter syscalls
strace -p PID                       # attach to running process
```

## References
- [bai_20_debug_techniques.md](bai_20_debug_techniques.md)
- BBB_SRM_C.pdf (JTAG J1 pinout)
