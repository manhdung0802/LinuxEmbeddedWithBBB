# Bài 17 - Concurrency trong Kernel

## 1. M?c tiêu bài h?c
- Hi?u ngu?n g?c concurrency trong Linux kernel
- S? d?ng mutex, spinlock, atomic operations
- Phân bi?t khi nào dùng mutex vs spinlock
- Dùng completion và wait queue cho synchronization
- Tránh deadlock và race condition

---

## 2. T?i sao c?n Concurrency Control?

### 2.1 Ngu?n g?c concurrency trong kernel:

```
+-----------------------------------------+
¦  Ngu?n concurrency trong kernel:         ¦
¦                                          ¦
¦  1. Multiple processes ? g?i driver d?ng th?i  ¦
¦  2. Interrupt handler ? preempt process context  ¦
¦  3. SMP (multi-core) ? ch?y song song th?t s?    ¦
¦  4. Kernel preemption ? task b? preempt gi?a ch?ng ¦
¦  5. Softirq/tasklet ? bottom half processing     ¦
+-----------------------------------------+
```

### 2.2 Ví d? race condition:

```c
/* HAI process cùng g?i write() vào driver */
static int counter = 0;

/* Process A */          /* Process B */
val = counter;           val = counter;     /* C? hai d?c 5 */
val++;                   val++;             /* C? hai tính 6 */
counter = val;           counter = val;     /* counter = 6, m?t 1 increment! */
```

**C?n**: Co ch? b?o v? shared data.

---

## 3. Mutex (Mutual Exclusion)

### 3.1 Khi nào dùng:
- B?o v? shared data trong **process context** (có th? sleep)
- **Không dùng** trong interrupt handler (vì mutex có th? sleep)

### 3.2 API:

```c
#include <linux/mutex.h>

/* Khai báo */
static DEFINE_MUTEX(my_mutex);

/* Ho?c dynamic init */
struct mutex my_mutex;
mutex_init(&my_mutex);

/* Lock/unlock */
mutex_lock(&my_mutex);        /* Block n?u mutex dang locked */
/* ... critical section ... */
mutex_unlock(&my_mutex);

/* Try lock (không block) */
if (mutex_trylock(&my_mutex)) {
    /* L?y du?c lock */
    mutex_unlock(&my_mutex);
} else {
    /* Không l?y du?c */
}

/* Lock interruptible (có th? b? signal interrupt) */
if (mutex_lock_interruptible(&my_mutex)) {
    return -ERESTARTSYS;  /* B? signal */
}
```

### 3.3 Ví d? trong driver:

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
- B?o v? shared data khi **không th? sleep** (interrupt context, atomic context)
- Critical section **r?t ng?n** (spinlock busy-waits, t?n CPU)
- Dùng khi c? process context VÀ interrupt handler truy c?p cùng data

### 4.2 API:

```c
#include <linux/spinlock.h>

/* Khai báo */
static DEFINE_SPINLOCK(my_lock);

/* Process context ONLY */
spin_lock(&my_lock);
/* ... critical section ... */
spin_unlock(&my_lock);

/* Process context + IRQ (disable interrupt trên CPU hi?n t?i) */
unsigned long flags;
spin_lock_irqsave(&my_lock, flags);
/* ... critical section ... */
spin_unlock_irqrestore(&my_lock, flags);

/* Process context + bottom half (disable softirq) */
spin_lock_bh(&my_lock);
/* ... critical section ... */
spin_unlock_bh(&my_lock);
```

### 4.3 T?i sao c?n `irqsave`?

```
Scenario KHÔNG dùng irqsave:
  1. Process A l?y spin_lock()
  2. Interrupt x?y ra trên CÙNG CPU
  3. IRQ handler cung c?n spin_lock()
  4. DEADLOCK! (vì spin_lock busy-wait, interrupt không return)

Scenario dùng irqsave:
  1. Process A l?y spin_lock_irqsave() ? disable IRQ
  2. Interrupt không th? x?y ra trên CPU này
  3. Không deadlock
  4. spin_unlock_irqrestore() ? enable IRQ l?i
```

### 4.4 Ví d?: register access v?i spinlock:

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
| Sleep du?c? | ? Có | ? Không |
| Dùng trong IRQ? | ? Không | ? Có |
| Chi phí khi contention | Th?p (sleep) | Cao (busy-wait) |
| Critical section | Dài OK | Ph?i r?t ng?n |
| Reentrant? | ? Không | ? Không |

**Quy t?c don gi?n**:
- N?u có th? sleep ? dùng **mutex**
- N?u trong interrupt ho?c c?n atomic ? dùng **spinlock**
- N?u ch? là counter don gi?n ? dùng **atomic**

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

Dùng khi m?t task c?n **ch?** task khác hoàn thành:

```c
#include <linux/completion.h>

static DECLARE_COMPLETION(my_completion);

/* Thread/handler báo hi?u hoàn thành */
static irqreturn_t my_handler(int irq, void *dev_id)
{
    /* X? lý xong */
    complete(&my_completion);
    return IRQ_HANDLED;
}

/* Thread ch? */
static ssize_t my_read(struct file *f, char __user *buf,
                        size_t count, loff_t *off)
{
    /* Ch? interrupt handler báo complete */
    wait_for_completion(&my_completion);    /* Block ? dây */
    /* Ho?c có timeout: */
    /* wait_for_completion_timeout(&my_completion, msecs_to_jiffies(1000)); */

    /* Reinit cho l?n ti?p theo */
    reinit_completion(&my_completion);

    /* Ð?c data t? hardware... */
    return count;
}
```

---

## 8. Wait Queue

Cho phép process **sleep** cho d?n khi di?u ki?n th?a mãn:

```c
#include <linux/wait.h>

static DECLARE_WAIT_QUEUE_HEAD(my_queue);
static int data_ready = 0;

/* IRQ handler ho?c writer: */
data_ready = 1;
wake_up_interruptible(&my_queue);

/* Reader: ch? data_ready */
static ssize_t my_read(struct file *f, char __user *buf,
                        size_t count, loff_t *off)
{
    /* Sleep cho d?n khi data_ready != 0 */
    if (wait_event_interruptible(my_queue, data_ready != 0))
        return -ERESTARTSYS;

    data_ready = 0;
    /* Ð?c data... */
    return count;
}
```

### 8.1 Variants:

```c
/* Block vô h?n */
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

### 9.1 Quy t?c:

1. **Lock ordering**: Luôn l?y lock theo th? t? c? d?nh
2. **Không hold lock khi sleep** (n?u là spinlock)
3. **Tránh nested lock** n?u có th?
4. **Dùng lockdep** (CONFIG_LOCKDEP) d? kernel phát hi?n deadlock

### 9.2 Ví d? deadlock:

```c
/* Thread A */                /* Thread B */
mutex_lock(&lock_A);         mutex_lock(&lock_B);
mutex_lock(&lock_B);  ??     mutex_lock(&lock_A);  /* DEADLOCK! */
```

**Fix**: Luôn l?y lock_A tru?c lock_B.

---

## 10. Ki?n th?c c?t lõi sau bài này

1. **Mutex**: process context, có th? sleep, critical section dài
2. **Spinlock**: interrupt context, busy-wait, critical section ng?n
3. **spin_lock_irqsave**: dùng khi data chia s? gi?a process + IRQ
4. **Atomic**: cho counter/flag don gi?n, không c?n lock
5. **Completion**: ch? event m?t l?n (ví d?: ch? IRQ)
6. **Wait queue**: ch? condition, process sleep/wake

---

## 11. Câu h?i ki?m tra

1. T?i sao không dùng mutex trong interrupt handler?
2. Khi nào dùng `spin_lock_irqsave` thay vì `spin_lock`?
3. Vi?t code dùng atomic d? d?m s? l?n open() driver.
4. Completion và wait queue khác nhau ? di?m nào?
5. Ðua ví d? deadlock và cách fix.

---

## 12. Chu?n b? cho bài sau

Bài ti?p theo: **Bài 12 - Interrupt Handling**

Ta s? h?c cách dang ký interrupt handler, x? lý interrupt t? ph?n c?ng AM335x, chia top-half/bottom-half.
