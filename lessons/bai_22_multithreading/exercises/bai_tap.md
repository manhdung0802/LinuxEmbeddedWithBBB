# Bài Tập - Bài 22: Multithreading

## Bài tập 1: Thread-safe Ring Buffer

Triển khai ring buffer thread-safe cho BBB sensor data:

```c
typedef struct {
    float samples[256];   /* ADC samples */
    int   write_idx;
    int   read_idx;
    int   count;
    pthread_mutex_t lock;
    pthread_cond_t  data_ready;
} SensorBuffer;
```

**Yêu cầu**:
- `sensor_write(buf, val)`: ghi sample vào buffer (drop oldest nếu full)
- `sensor_read(buf, out, n)`: đọc n samples, block nếu chưa đủ
- Thread 1: gọi `sensor_write()` mỗi 10ms (giả lập ADC @ 100Hz)
- Thread 2: gọi `sensor_read()` lấy 10 samples, tính RMS, in ra

---

## Bài tập 2: Read-Write Lock

Hệ thống có nhiều thread đọc config và một thread ghi config. Dùng `pthread_rwlock_t`:

```c
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

// Đọc (nhiều thread cùng lúc OK):
pthread_rwlock_rdlock(&rwlock);
// read config...
pthread_rwlock_unlock(&rwlock);

// Ghi (độc quyền):
pthread_rwlock_wrlock(&rwlock);
// update config...
pthread_rwlock_unlock(&rwlock);
```

**Yêu cầu**:
- 5 reader threads đọc config mỗi 100ms
- 1 writer thread cập nhật config mỗi 1 giây
- In ra khi writer chờ (contention)
- So sánh với mutex: tại sao rwlock hiệu quả hơn khi read nhiều, write ít?

---

## Bài tập 3: Thread Pool cho xử lý GPIO Events

Triển khai thread pool xử lý GPIO interrupt events:

```c
typedef struct {
    int gpio_pin;
    int event_type;  /* RISING=1, FALLING=0 */
    struct timespec timestamp;
} GpioEvent;
```

**Thiết kế**:
- Job queue: circular buffer 32 events, protected by mutex + cond
- 3 worker threads trong pool
- Main thread "produce" events (simulate bằng loop)
- Worker threads consume và xử lý (in timestamp + pin + type)

**Bonus**: thêm graceful shutdown — main gửi N null events để worker tự thoát.
