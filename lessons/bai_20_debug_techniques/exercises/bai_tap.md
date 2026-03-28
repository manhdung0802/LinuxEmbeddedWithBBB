# Bài Tập - Bài 20: Debug Techniques

## Bài tập 1: Phân tích /proc/interrupts

Trên BBB đang chạy, thực hiện:

```bash
# Lấy baseline
cat /proc/interrupts > baseline.txt

# Blink LED USR0 100 lần qua /sys/class/gpio
for i in $(seq 1 100); do
    echo 1 > /sys/class/gpio/gpio53/value
    echo 0 > /sys/class/gpio/gpio53/value
done

# Lấy snapshot sau
cat /proc/interrupts > after.txt

# So sánh
diff baseline.txt after.txt
```

**Yêu cầu**:
1. IRQ nào tăng sau khi blink? Tại sao?
2. Tìm IRQ của UART và timer trong `/proc/interrupts`
3. Giải thích cột đầu tiên (số), cột CPU0, cột tên controller, cột device name

---

## Bài tập 2: Dynamic Debug với Driver Bài 18

Load `mychar_driver` từ bài 18, sau đó:

```bash
# 1. Bật dynamic debug cho module
echo "module mychar_driver +p" > /sys/kernel/debug/dynamic_debug/control

# 2. Test tất cả operations
echo "Hello world" > /dev/mychar
cat /dev/mychar
# ioctl reset (dùng test program từ bài 18)

# 3. Xem output chi tiết
dmesg | tail -20

# 4. Bật thêm thông tin file/line/function
echo "module mychar_driver +pflmt" > /sys/kernel/debug/dynamic_debug/control
echo "test again" > /dev/mychar
dmesg | tail -10
```

**Yêu cầu**: Mô tả sự khác biệt giữa output khi dùng `+p` và `+pflmt`. Giải thích ý nghĩa từng flag.

---

## Bài tập 3: ftrace Call Graph

Trace execution flow khi ghi vào `/dev/mychar`:

```bash
cd /sys/kernel/debug/tracing

# Setup
echo function_graph > current_tracer
echo "mychar_write" > set_graph_function

# Trace
echo 1 > tracing_on
echo "ftrace test" > /dev/mychar
echo 0 > tracing_on

# Xem kết quả
cat trace | head -30
```

**Yêu cầu**:
1. Những kernel function nào được `mychar_write` gọi? (theo trace)
2. Hàm nào tốn thời gian nhất?
3. Thay đổi sang `current_tracer = function` (không phải graph) — output khác gì?
4. Dùng `set_ftrace_filter` thay vì `set_graph_function`, so sánh kết quả
