# Bài 22 - Multithreading: Tham Khảo Nhanh

## Thread lifecycle
```c
pthread_t tid;
pthread_create(&tid, NULL, thread_fn, arg);  // tạo thread
pthread_join(tid, &retval);                  // chờ + lấy return value
pthread_detach(tid);                         // tự cleanup khi xong
pthread_cancel(tid);                         // request hủy thread
```

## Thread function signature
```c
void *my_thread(void *arg) {
    int param = *(int *)arg;   // cast arg
    // ...
    return (void *)(long)result;  // cast return
}
```

## Mutex
```c
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock(&m);     // block nếu locked
pthread_mutex_trylock(&m);  // không block, trả về EBUSY
pthread_mutex_unlock(&m);
pthread_mutex_destroy(&m);
```

## Semaphore
```c
sem_t s;
sem_init(&s, 0, N);  // N = initial count
sem_wait(&s);        // P(): block nếu s==0, rồi s--
sem_post(&s);        // V(): s++, unblock một waiter
sem_destroy(&s);
```

## Condition Variable
```c
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
// Waiter:
pthread_mutex_lock(&m);
while (!condition) pthread_cond_wait(&cv, &m);  // unlock m khi wait, relock khi wake
// ... use data ...
pthread_mutex_unlock(&m);
// Signaler:
pthread_mutex_lock(&m);
condition = true;
pthread_cond_signal(&cv);    // wake 1
pthread_cond_broadcast(&cv); // wake all
pthread_mutex_unlock(&m);
```

## Build
```bash
gcc myfile.c -o myfile -lpthread
```

## References
- [bai_22_multithreading.md](bai_22_multithreading.md)
