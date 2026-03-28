# Bài 22 - Multithreading: pthread, Mutex, Semaphore, Condition Variable

## 1. Mục tiêu bài học

- Hiểu thread vs process: ưu và nhược điểm
- Tạo và quản lý thread với `pthread_create` / `pthread_join`
- Bảo vệ shared data bằng **mutex**
- Đồng bộ hóa với **semaphore** (POSIX)
- Phối hợp giữa các thread bằng **condition variable**
- Ứng dụng thực tế: producer-consumer trên BBB

---

## 2. Thread vs Process

| | Thread | Process |
|---|---|---|
| Tạo | Nhanh (`pthread_create`) | Chậm hơn (`fork`) |
| Shared memory | Có (cùng address space) | Không (mỗi process riêng) |
| Giao tiếp | Qua biến shared | IPC (pipe, socket, mmap) |
| Crash ảnh hưởng | Toàn process | Chỉ process đó |
| Overhead | Thấp | Cao hơn |

→ Trong embedded, thread phù hợp khi cần:
- Đọc sensor liên tục (sensor thread) + xử lý dữ liệu (worker thread)
- Real-time task + non-real-time task trong cùng process

---

## 3. pthread_create và pthread_join

```c
/*
 * thread_basic.c — Tạo và join thread
 */
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

/* Thread function: nhận void* arg, trả về void* */
void *sensor_thread(void *arg)
{
    int sensor_id = *(int *)arg;
    printf("[Thread %d] PID=%d, reading sensor...\n",
           sensor_id, getpid());

    for (int i = 0; i < 5; i++) {
        printf("[Thread %d] value = %d\n", sensor_id, i * 10);
        sleep(1);
    }

    /* Trả về giá trị qua pointer (phải heap-allocated, không phải stack) */
    int *result = malloc(sizeof(int));
    *result = sensor_id * 100;
    return result;
}

int main(void)
{
    pthread_t tid[2];
    int ids[2] = {1, 2};

    /* Tạo 2 thread */
    for (int i = 0; i < 2; i++) {
        int ret = pthread_create(&tid[i], NULL, sensor_thread, &ids[i]);
        if (ret != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(ret));
            return 1;
        }
    }

    /* Chờ từng thread và lấy return value */
    for (int i = 0; i < 2; i++) {
        void *retval;
        pthread_join(tid[i], &retval);
        printf("Thread %d returned: %d\n", i+1, *(int *)retval);
        free(retval);
    }

    printf("All threads done\n");
    return 0;
}
```

```bash
# Build (cần -lpthread)
gcc thread_basic.c -o thread_basic -lpthread
./thread_basic
```

---

## 4. Mutex — Mutual Exclusion

Mutex bảo vệ **critical section** — đoạn code chỉ được một thread chạy tại một thời điểm.

```c
/*
 * mutex_demo.c — Bảo vệ shared counter
 */
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTS  100000

static long shared_counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void *increment_worker(void *arg)
{
    for (int i = 0; i < INCREMENTS; i++) {
        /* Không có mutex: race condition → kết quả sai! */
        pthread_mutex_lock(&counter_mutex);
        shared_counter++;           /* critical section */
        pthread_mutex_unlock(&counter_mutex);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, increment_worker, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("Expected: %ld, Got: %ld\n",
           (long)NUM_THREADS * INCREMENTS, shared_counter);
    /* Với mutex: luôn bằng nhau */

    pthread_mutex_destroy(&counter_mutex);
    return 0;
}
```

### Lưu ý Deadlock

Deadlock xảy ra khi 2 mutex bị lock ngược chiều:

```c
/* Thread A */          /* Thread B */
lock(mutex1)            lock(mutex2)
lock(mutex2) ← block   lock(mutex1) ← block
              DEADLOCK!
```

**Phòng tránh**: Luôn lock/unlock theo **cùng thứ tự** trong mọi thread.

---

## 5. Semaphore POSIX

Semaphore là **counter** dùng để kiểm soát số lượng thread truy cập tài nguyên đồng thời.

```c
#include <semaphore.h>

sem_t sem;
sem_init(&sem, 0, initial_value);  /* 0 = thread semaphore (không phải process) */

sem_wait(&sem);   /* P() — decrement, block nếu = 0 */
/* ... critical section ... */
sem_post(&sem);   /* V() — increment, unblock thread đang chờ */

sem_destroy(&sem);
```

```c
/*
 * semaphore_demo.c — Giới hạn số thread đọc I2C đồng thời
 * Giả sử I2C bus chỉ handle 2 request cùng lúc
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_THREADS 6
#define MAX_I2C_CONCURRENT 2

static sem_t i2c_sem;

void *i2c_reader(void *arg)
{
    int id = *(int *)arg;
    printf("[Thread %d] Waiting for I2C access...\n", id);

    sem_wait(&i2c_sem);   /* block nếu đã có 2 thread dùng I2C */
    printf("[Thread %d] Reading I2C sensor\n", id);
    sleep(2);             /* simulate I2C transaction */
    printf("[Thread %d] Done\n", id);
    sem_post(&i2c_sem);   /* release I2C slot */

    return NULL;
}

int main(void)
{
    pthread_t threads[MAX_THREADS];
    int ids[MAX_THREADS];

    sem_init(&i2c_sem, 0, MAX_I2C_CONCURRENT);  /* allow 2 concurrent */

    for (int i = 0; i < MAX_THREADS; i++) {
        ids[i] = i + 1;
        pthread_create(&threads[i], NULL, i2c_reader, &ids[i]);
    }
    for (int i = 0; i < MAX_THREADS; i++)
        pthread_join(threads[i], NULL);

    sem_destroy(&i2c_sem);
    return 0;
}
```

---

## 6. Condition Variable

Condition variable cho phép thread **chờ đợi** một điều kiện xảy ra, thay vì busy-wait.

```c
/*
 * producer_consumer.c — Ứng dụng BBB: sensor → processor
 * Producer: đọc sensor → bỏ vào queue
 * Consumer: lấy từ queue → xử lý (filter, log, giao tiếp)
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define QUEUE_SIZE  10

typedef struct {
    int data[QUEUE_SIZE];
    int head, tail, count;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;  /* signal khi có data */
    pthread_cond_t  not_full;   /* signal khi có chỗ */
} CircularQueue;

static CircularQueue queue = {
    .head  = 0, .tail = 0, .count = 0,
    .mutex     = PTHREAD_MUTEX_INITIALIZER,
    .not_empty = PTHREAD_COND_INITIALIZER,
    .not_full  = PTHREAD_COND_INITIALIZER,
};

static volatile int done = 0;

/* Đưa value vào queue (blocking nếu full) */
void queue_push(int value)
{
    pthread_mutex_lock(&queue.mutex);
    while (queue.count == QUEUE_SIZE)
        pthread_cond_wait(&queue.not_full, &queue.mutex);

    queue.data[queue.tail] = value;
    queue.tail = (queue.tail + 1) % QUEUE_SIZE;
    queue.count++;

    pthread_cond_signal(&queue.not_empty);  /* báo consumer */
    pthread_mutex_unlock(&queue.mutex);
}

/* Lấy value từ queue (blocking nếu empty) */
int queue_pop(void)
{
    pthread_mutex_lock(&queue.mutex);
    while (queue.count == 0 && !done)
        pthread_cond_wait(&queue.not_empty, &queue.mutex);

    if (queue.count == 0) {   /* done=1 và queue rỗng */
        pthread_mutex_unlock(&queue.mutex);
        return -1;
    }

    int value = queue.data[queue.head];
    queue.head = (queue.head + 1) % QUEUE_SIZE;
    queue.count--;

    pthread_cond_signal(&queue.not_full);   /* báo producer */
    pthread_mutex_unlock(&queue.mutex);
    return value;
}

void *producer(void *arg)
{
    for (int i = 0; i < 20; i++) {
        int sensor_val = i * 5;   /* giả lập đọc ADC */
        printf("[Producer] sensor=%d\n", sensor_val);
        queue_push(sensor_val);
        usleep(100000);           /* 100ms */
    }
    pthread_mutex_lock(&queue.mutex);
    done = 1;
    pthread_cond_broadcast(&queue.not_empty);  /* wake all consumers */
    pthread_mutex_unlock(&queue.mutex);
    return NULL;
}

void *consumer(void *arg)
{
    int id = *(int *)arg;
    while (1) {
        int val = queue_pop();
        if (val < 0) break;   /* done */
        printf("[Consumer %d] processing=%d\n", id, val * 2);
        usleep(200000);       /* simulate processing */
    }
    return NULL;
}

int main(void)
{
    pthread_t prod, cons[2];
    int ids[2] = {1, 2};

    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons[0], NULL, consumer, &ids[0]);
    pthread_create(&cons[1], NULL, consumer, &ids[1]);

    pthread_join(prod, NULL);
    pthread_join(cons[0], NULL);
    pthread_join(cons[1], NULL);
    return 0;
}
```

---

## 7. Câu hỏi kiểm tra

1. Thread và process khác nhau ở điểm nào quan trọng nhất trong embedded?
2. Tại sao `shared_counter++` bị race condition dù là một dòng code?
3. `pthread_cond_wait()` làm gì với mutex khi nó block?
4. Tại sao phải dùng `while` thay vì `if` khi check condition trong `pthread_cond_wait()`?
5. Deadlock xảy ra khi nào? Làm sao phát hiện?

---

## 8. Tài liệu tham khảo

| API | Man page | Ghi chú |
|-----|----------|---------|
| pthread_create | man 3 pthread_create | -lpthread khi link |
| pthread_mutex_lock | man 3 pthread_mutex_lock | PTHREAD_MUTEX_INITIALIZER |
| sem_wait/post | man 3 sem_wait | POSIX semaphore |
| pthread_cond_wait | man 3 pthread_cond_wait | phải giữ mutex khi gọi |
