# Bài Tập - Bài 17: Linux Kernel Module

## Bài tập 1: Module Hello với Timer

Viết một kernel module thực hiện:
1. Khi load: in ra log `"Module loaded, timestamp: <jiffies>"`
2. Sau 5 giây: in ra `"5 seconds passed! jiffies now: <jiffies>"`
3. Khi unload: hủy timer và in `"Module unloaded"`

**Hint**: Dùng `struct timer_list`, `timer_setup()`, `mod_timer()`, `del_timer_sync()`.

```c
#include <linux/timer.h>

static struct timer_list my_timer;

static void timer_callback(struct timer_list *t)
{
    pr_info("5 seconds passed! jiffies: %lu\n", jiffies);
}

// Trong init:
timer_setup(&my_timer, timer_callback, 0);
mod_timer(&my_timer, jiffies + msecs_to_jiffies(5000));
```

---

## Bài tập 2: Module Parameter GPIO

Viết module với:
- Parameter `gpio_num` (default: 21) — số chân GPIO1
- Khi load với `gpio_num=X`: in ra `"Controlling GPIO1_X"`
- Đọc giá trị DATAIN của chân đó qua ioremap và in ra `"GPIO1_X = 0|1"`

**Yêu cầu**: 
- Validate tham số (0 ≤ gpio_num ≤ 31)
- Nếu invalid: trả về `-EINVAL` trong init

---

## Bài tập 3: Phân tích Module Dependencies

Thực hiện trên BBB:

```bash
# 1. Xem danh sách tất cả module đang load
lsmod

# 2. Chọn một module trong list (ví dụ: musb_hdrc)
# Tìm dependencies của nó
modinfo musb_hdrc | grep depends

# 3. Vẽ sơ đồ dependency tree (text)
# Ví dụ:
# musb_hdrc
#   └── udc-core
#   └── cppi41

# 4. Thử rmmod module đang có dependent
# sudo rmmod udc-core
# → Error là gì? Tại sao?
```

**Yêu cầu**: Giải thích tại sao kernel không cho unload module đang được module khác sử dụng (cột "Used by" trong `lsmod`).
