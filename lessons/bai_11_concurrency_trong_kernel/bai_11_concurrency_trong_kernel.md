# Bài 11 - Concurrency trong Kernel

## 1. Mục tiêu bài học
- Hiểu nguồn gốc concurrency trong Linux kernel
- Sử dụng mutex, spinlock, atomic operations
- Phân biệt khi nào dùng mutex vs spinlock
- Dùng completion và wait queue cho synchronization
- Tránh deadlock và race condition

---

## 2. Tại sao cần Concurrency Control?

### 2.1 Nguồn gốc concurrency trong kernel:

```
┌─────────────────────────────────────────┐
│  Nguồn concurrency trong kernel:         │
│                                          │
│  1. Multiple processes → gọi driver đồng thời  │
│  2. Interrupt handler → preempt process context  │
│  3. SMP (multi-core) → chạy song song thật sự    │
│  4. Kernel preemption → task bị preempt giữa chừng │
│  5. Softirq/tasklet → bottom half processing     │
└─────────────────────────────────────────┘
```

### 2.2 Ví dụ race condition:

```c
/* HAI process cùng gọi write() vào driver */
static int counter = 0;

/* Process A */          /* Process B */
val = counter;           val = counter;     /* Cả hai đọc 5 */
val++;                   val++;             /* Cả hai tính 6 */
counter = val;           counter = val;     /* counter = 6, mất 1 increment! */
```

**Cần**: Cơ chế bảo vệ shared data.

---

## 3. Mutex (Mutual Exclusion)

### 3.1 Khi nào dùng:
- Bảo vệ shared data trong **process context** (có thể sleep)
- **Không dùng** trong interrupt handler (vì mutex có thể sleep)

### 3.2 API:

```c
#include <linux/mutex.h>

/* Khai báo */
static DEFINE_MUTEX(my_mutex);

/* Hoặc dynamic init */
struct mutex my_mutex;
mutex_init(&my_mutex);

/* Lock/unlock */
mutex_lock(&my_mutex);        /* Block nếu mutex đang locked */
/* ... critical section ... */
mutex_unlock(&my_mutex);

/* Try lock (không block) */
if (mutex_trylock(&my_mutex)) {
    /* Lấy được lock */
    mutex_unlock(&my_mutex);
} else {
    /* Không lấy được */
}

/* Lock interruptible (có thể bị signal interrupt) */
if (mutex_lock_interruptible(&my_mutex)) {
    return -ERESTARTSYS;  /* Bị signal */
}
```

### 3.3 Ví dụ trong driver:

```c
struct my_device {
    struct mutex lock;
    char buf[256];
    int buf_len;
};

static ssize_t my_write(struct file *f, const char __user *ubuf,
                         size_t count, loff_t *off)
{
    struct my_device *dev = f->private_data;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (copy_from_user(dev->buf, ubuf, min(count, sizeof(dev->buf) - 1))) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    dev->buf_len = count;

    mutex_unlock(&dev->lock);
    return count;
}
```

---

## 4. Spinlock

### 4.1 Khi nào dùng:
- Bảo vệ shared data khi **không thể sleep** (interrupt context, atomic context)
- Critical section **rất ngắn** (spinlock busy-waits, tốn CPU)
- Dùng khi cả process context VÀ interrupt handler truy cập cùng data

### 4.2 API:

```c
#include <linux/spinlock.h>

/* Khai báo */
static DEFINE_SPINLOCK(my_lock);

/* Process context ONLY */
spin_lock(&my_lock);
/* ... critical section ... */
spin_unlock(&my_lock);

/* Process context + IRQ (disable interrupt trên CPU hiện tại) */
unsigned long flags;
spin_lock_irqsave(&my_lock, flags);
/* ... critical section ... */
spin_unlock_irqrestore(&my_lock, flags);

/* Process context + bottom half (disable softirq) */
spin_lock_bh(&my_lock);
/* ... critical section ... */
spin_unlock_bh(&my_lock);
```

### 4.3 Tại sao cần `irqsave`?

```
Scenario KHÔNG dùng irqsave:
  1. Process A lấy spin_lock()
  2. Interrupt xảy ra trên CÙNG CPU
  3. IRQ handler cũng cần spin_lock()
  4. DEADLOCK! (vì spin_lock busy-wait, interrupt không return)

Scenario dùng irqsave:
  1. Process A lấy spin_lock_irqsave() → disable IRQ
  2. Interrupt không thể xảy ra trên CPU này
  3. Không deadlock
  4. spin_unlock_irqrestore() → enable IRQ lại
```

### 4.4 Ví dụ: register access với spinlock:

```c
struct gpio_driver {
    void __iomem *base;
    spinlock_t reg_lock;
};

static void set_gpio_bit(struct gpio_driver *gd, int bit)
{
    unsigned long flags;
    u32 val;

    spin_lock_irqsave(&gd->reg_lock, flags);

    val = readl(gd->base + GPIO_DATAOUT);
    val |= (1 << bit);
    writel(val, gd->base + GPIO_DATAOUT);

    spin_unlock_irqrestore(&gd->reg_lock, flags);
}
```

---

## 5. Mutex vs Spinlock

| Tiêu chí | Mutex | Spinlock |
|----------|-------|----------|
| Sleep được? | ✅ Có | ❌ Không |
| Dùng trong IRQ? | ❌ Không | ✅ Có |
| Chi phí khi contention | Thấp (sleep) | Cao (busy-wait) |
| Critical section | Dài OK | Phải rất ngắn |
| Reentrant? | ❌ Không | ❌ Không |

**Quy tắc đơn giản**:
- Nếu có thể sleep → dùng **mutex**
- Nếu trong interrupt hoặc cần atomic → dùng **spinlock**
- Nếu chỉ là counter đơn giản → dùng **atomic**

---

## 6. Atomic Operations

### 6.1 Cho integer:

```c
#include <linux/atomic.h>

static atomic_t counter = ATOMIC_INIT(0);

atomic_set(&counter, 5);
int val = atomic_read(&counter);

atomic_inc(&counter);       /* counter++ */
atomic_dec(&counter);       /* counter-- */
atomic_add(3, &counter);    /* counter += 3 */
atomic_sub(2, &counter);    /* counter -= 2 */

/* Test and set */
if (atomic_dec_and_test(&counter))
    pr_info("counter reached 0\n");
```

### 6.2 Cho bit operations:

```c
unsigned long flags = 0;

set_bit(3, &flags);         /* flags |= (1 << 3) */
clear_bit(3, &flags);       /* flags &= ~(1 << 3) */
int val = test_bit(3, &flags);  /* (flags >> 3) & 1 */

/* Test and set atomically */
if (test_and_set_bit(3, &flags))
    pr_info("bit 3 was already set\n");
```

---

## 7. Completion

Dùng khi một task cần **chờ** task khác hoàn thành:

```c
#include <linux/completion.h>

static DECLARE_COMPLETION(my_completion);

/* Thread/handler báo hiệu hoàn thành */
static irqreturn_t my_handler(int irq, void *dev_id)
{
    /* Xử lý xong */
    complete(&my_completion);
    return IRQ_HANDLED;
}

/* Thread chờ */
static ssize_t my_read(struct file *f, char __user *buf,
                        size_t count, loff_t *off)
{
    /* Chờ interrupt handler báo complete */
    wait_for_completion(&my_completion);    /* Block ở đây */
    /* Hoặc có timeout: */
    /* wait_for_completion_timeout(&my_completion, msecs_to_jiffies(1000)); */

    /* Reinit cho lần tiếp theo */
    reinit_completion(&my_completion);

    /* Đọc data từ hardware... */
    return count;
}
```

---

## 8. Wait Queue

Cho phép process **sleep** cho đến khi điều kiện thỏa mãn:

```c
#include <linux/wait.h>

static DECLARE_WAIT_QUEUE_HEAD(my_queue);
static int data_ready = 0;

/* IRQ handler hoặc writer: */
data_ready = 1;
wake_up_interruptible(&my_queue);

/* Reader: chờ data_ready */
static ssize_t my_read(struct file *f, char __user *buf,
                        size_t count, loff_t *off)
{
    /* Sleep cho đến khi data_ready != 0 */
    if (wait_event_interruptible(my_queue, data_ready != 0))
        return -ERESTARTSYS;

    data_ready = 0;
    /* Đọc data... */
    return count;
}
```

### 8.1 Variants:

```c
/* Block vô hạn */
wait_event(queue, condition);

/* Block có interrupt (signal) */
wait_event_interruptible(queue, condition);

/* Block có timeout */
wait_event_timeout(queue, condition, timeout_jiffies);

/* Block có interrupt + timeout */
wait_event_interruptible_timeout(queue, condition, timeout);
```

---

## 9. Tránh Deadlock

### 9.1 Quy tắc:

1. **Lock ordering**: Luôn lấy lock theo thứ tự cố định
2. **Không hold lock khi sleep** (nếu là spinlock)
3. **Tránh nested lock** nếu có thể
4. **Dùng lockdep** (CONFIG_LOCKDEP) để kernel phát hiện deadlock

### 9.2 Ví dụ deadlock:

```c
/* Thread A */                /* Thread B */
mutex_lock(&lock_A);         mutex_lock(&lock_B);
mutex_lock(&lock_B);  ←→     mutex_lock(&lock_A);  /* DEADLOCK! */
```

**Fix**: Luôn lấy lock_A trước lock_B.

---

## 10. Kiến thức cốt lõi sau bài này

1. **Mutex**: process context, có thể sleep, critical section dài
2. **Spinlock**: interrupt context, busy-wait, critical section ngắn
3. **spin_lock_irqsave**: dùng khi data chia sẻ giữa process + IRQ
4. **Atomic**: cho counter/flag đơn giản, không cần lock
5. **Completion**: chờ event một lần (ví dụ: chờ IRQ)
6. **Wait queue**: chờ condition, process sleep/wake

---

## 11. Câu hỏi kiểm tra

1. Tại sao không dùng mutex trong interrupt handler?
2. Khi nào dùng `spin_lock_irqsave` thay vì `spin_lock`?
3. Viết code dùng atomic để đếm số lần open() driver.
4. Completion và wait queue khác nhau ở điểm nào?
5. Đưa ví dụ deadlock và cách fix.

---

## 12. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 12 - Interrupt Handling**

Ta sẽ học cách đăng ký interrupt handler, xử lý interrupt từ phần cứng AM335x, chia top-half/bottom-half.
