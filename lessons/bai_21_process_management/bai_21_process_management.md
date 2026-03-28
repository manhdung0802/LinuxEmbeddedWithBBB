# Bài 21 - Process Management: fork, exec, wait, Zombie & Orphan

## 1. Mục tiêu bài học

- Hiểu mô hình process trong Linux: PCB, PID, PPID
- Thực hành `fork()` tạo child process
- Dùng `exec*()` để thay thế image của process
- Xử lý exit status qua `wait()`/`waitpid()`
- Hiểu và phòng tránh **zombie** và **orphan** process
- Ứng dụng trong embedded: watchdog process, worker subprocess

---

## 2. Process Model trong Linux

Mỗi process trong Linux có:
- **PID** (Process ID): số định danh duy nhất
- **PPID** (Parent PID): PID của process cha
- **PCB** (Process Control Block): kernel struct lưu trạng thái
- **Address space**: code, data, stack, heap riêng biệt

```bash
# Xem process tree trên BBB
ps aux                      # toàn bộ process
ps -ef --forest             # dạng cây
pstree                      # tree đẹp hơn

# Ví dụ output pstree:
# systemd─┬─sshd───sshd───bash───pstree
#         ├─kthreadd─┬─...
#         └─...
```

---

## 3. fork() — Tạo Child Process

`fork()` tạo bản sao của process hiện tại. Đây là system call duy nhất tạo process mới trong Linux (trừ `clone()`).

```c
/*
 * fork_demo.c — Demo fork() cơ bản
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   /* fork, getpid, getppid */
#include <sys/wait.h> /* wait */

int main(void)
{
    pid_t pid;
    int shared_var = 100;  /* biến trước khi fork */

    printf("Before fork: PID=%d, shared_var=%d\n", getpid(), shared_var);

    pid = fork();

    if (pid < 0) {
        /* fork thất bại */
        perror("fork");
        return 1;
    }
    else if (pid == 0) {
        /* === CHILD PROCESS ===
         * fork() trả về 0 cho child
         * Child có bản sao của shared_var (copy-on-write)
         */
        shared_var = 200;  /* chỉ thay đổi trong child */
        printf("Child:  PID=%d, PPID=%d, shared_var=%d\n",
               getpid(), getppid(), shared_var);
        exit(0);  /* child thoát với code 0 */
    }
    else {
        /* === PARENT PROCESS ===
         * fork() trả về PID của child cho parent
         */
        int status;
        wait(&status);  /* chờ child kết thúc */

        printf("Parent: PID=%d, child PID=%d, shared_var=%d (unchanged)\n",
               getpid(), pid, shared_var);

        if (WIFEXITED(status))
            printf("Child exited with code: %d\n", WEXITSTATUS(status));
    }

    return 0;
}
```

### Copy-on-Write (COW)

Sau `fork()`, child **không** nhận bản sao ngay lập tức — chúng dùng **chung** trang nhớ vật lý. Chỉ khi một bên ghi (write), kernel mới tạo bản sao riêng. → Fork rất nhanh dù process lớn.

---

## 4. exec() — Thay Thế Image Process

`exec*()` thay **toàn bộ** code và data của process hiện tại bằng chương trình mới. PID **không đổi**.

```c
/*
 * exec_demo.c — fork + exec pattern (spawn subprocess)
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    pid_t pid = fork();

    if (pid == 0) {
        /* Child: thực thi lệnh "ls -la /dev/gpio*" */
        char *args[] = { "ls", "-la", "/dev/gpio*", NULL };
        execvp("ls", args);

        /* Nếu execvp trả về → lỗi (không tìm thấy program) */
        perror("execvp");
        _exit(1);   /* dùng _exit() trong child sau exec failure */
    }

    int status;
    waitpid(pid, &status, 0);
    printf("ls exited: %d\n", WEXITSTATUS(status));
    return 0;
}
```

### Các biến thể exec

```c
execl("/bin/ls", "ls", "-la", NULL);        /* list args trực tiếp */
execv("/bin/ls", argv_array);               /* array args */
execle("/bin/ls", "ls", NULL, envp);        /* custom environment */
execve("/bin/ls", argv_array, envp);        /* argv + envp */
execlp("ls", "ls", "-la", NULL);            /* tìm trong PATH */
execvp("ls", argv_array);                   /* PATH + argv array (thường dùng) */
```

---

## 5. wait() và waitpid()

```c
#include <sys/wait.h>

/* wait() — chờ BẤT KỲ child nào kết thúc */
pid_t wait(int *status);

/* waitpid() — chờ child CỤ THỂ */
pid_t waitpid(pid_t pid, int *status, int options);
/*
 * pid:
 *   > 0 : chờ child có PID đó
 *   -1  : chờ bất kỳ child (giống wait())
 *   0   : chờ bất kỳ child trong cùng process group
 *
 * options:
 *   0        : block cho đến khi child kết thúc
 *   WNOHANG  : không block, trả về 0 nếu chưa có child nào kết thúc
 */

/* Macros phân tích status */
WIFEXITED(status)     /* = 1 nếu child exit bình thường */
WEXITSTATUS(status)   /* exit code (chỉ valid khi WIFEXITED) */
WIFSIGNALED(status)   /* = 1 nếu child bị kill bởi signal */
WTERMSIG(status)      /* signal number (chỉ valid khi WIFSIGNALED) */
```

---

## 6. Zombie Process

**Zombie** = process đã exit nhưng entry PCB vẫn còn trong kernel vì parent **chưa gọi wait()**.

```
Child exit()  →  trạng thái = Z (zombie)  →  parent wait()  →  PCB removed
```

```c
/*
 * zombie_demo.c — tạo zombie có chủ đích (để quan sát)
 * ĐỪNG làm trong production code!
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void)
{
    pid_t pid = fork();

    if (pid == 0) {
        printf("Child exiting (PID=%d)\n", getpid());
        exit(0);  /* child exit ngay */
    }

    /* Parent KHÔNG gọi wait() → child trở thành zombie */
    printf("Parent sleeping. Check: ps aux | grep Z\n");
    printf("Zombie PID = %d\n", pid);
    sleep(30);    /* trong 30 giây, child là zombie */

    /* Sau đây parent gọi wait → zombie được dọn */
    int st;
    wait(&st);
    printf("Zombie cleaned up\n");
    return 0;
}
```

```bash
# Quan sát zombie
./zombie_demo &
ps aux | grep -w Z
# USER  PID  ... STAT ...
# root  1234  ... Z    ...  [zombie_demo] <defunct>
```

### Phòng tránh Zombie

```c
/* Cách 1: Luôn gọi wait()/waitpid() */

/* Cách 2: Ignore SIGCHLD → kernel tự dọn zombie */
signal(SIGCHLD, SIG_IGN);

/* Cách 3: Handler SIGCHLD dùng waitpid(WNOHANG) */
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);  /* dọn hết zombie */
}
signal(SIGCHLD, sigchld_handler);

/* Cách 4: Double fork (child thoát ngay, grandchild được init adopt) */
pid_t pid = fork();
if (pid == 0) {
    pid_t grandchild = fork();
    if (grandchild == 0) {
        /* đây là grandchild — sẽ làm việc thực sự */
        /* khi parent (child) exit, grandchild được adopt bởi init (PID 1) */
    }
    exit(0);   /* child thoát ngay → grandchild thành orphan → được init adopt */
}
waitpid(pid, NULL, 0);  /* chỉ cần wait cho child ngắn gọn */
```

---

## 7. Orphan Process

**Orphan** = child process mà parent đã thoát. Kernel tự động "reparent" orphan cho **init/systemd** (PID 1).

```c
/*
 * orphan_demo.c
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void)
{
    pid_t pid = fork();

    if (pid == 0) {
        /* Child: sleep lâu → parent sẽ thoát trước */
        printf("Child PID=%d, PPID=%d (before parent exit)\n",
               getpid(), getppid());
        sleep(5);
        /* Sau khi parent thoát, PPID sẽ đổi thành 1 (init) */
        printf("Child PID=%d, PPID=%d (after parent exit)\n",
               getpid(), getppid());
        exit(0);
    }

    /* Parent thoát ngay (không wait) → child thành orphan */
    printf("Parent PID=%d exiting\n", getpid());
    exit(0);
}
```

---

## 8. Ứng dụng Embedded: Watchdog Process Pattern

Pattern phổ biến trong embedded: parent monitor child process, restart nếu crash.

```c
/*
 * watchdog_process.c — Monitor + restart worker process
 * Ứng dụng trên BBB: giám sát daemon đọc cảm biến
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_RESTARTS 5

static volatile int running = 1;

void sigterm_handler(int sig) { running = 0; }

void worker_process(void)
{
    /* Giả lập sensor reading loop */
    printf("[Worker PID=%d] Reading sensors...\n", getpid());
    for (int i = 0; i < 10; i++) {
        printf("[Worker] Sensor value: %d\n", i * 42);
        sleep(1);
    }
    /* Giả lập crash */
    printf("[Worker] Simulating crash!\n");
    abort();
}

int main(void)
{
    int restarts = 0;
    signal(SIGTERM, sigterm_handler);

    printf("[Monitor PID=%d] Starting worker watchdog\n", getpid());

    while (running && restarts < MAX_RESTARTS) {
        pid_t pid = fork();
        if (pid == 0) {
            worker_process();
            exit(0);
        }

        int status;
        waitpid(pid, &status, 0);

        if (!running) break;    /* bị SIGTERM → thoát gracefully */

        if (WIFSIGNALED(status)) {
            printf("[Monitor] Worker crashed (signal %d), restarting... (%d/%d)\n",
                   WTERMSIG(status), ++restarts, MAX_RESTARTS);
            sleep(1);   /* backoff trước khi restart */
        } else {
            printf("[Monitor] Worker exited normally (code %d)\n",
                   WEXITSTATUS(status));
            break;
        }
    }

    printf("[Monitor] Done. Total restarts: %d\n", restarts);
    return 0;
}
```

---

## 9. Câu hỏi kiểm tra

1. `fork()` trả về gì cho parent và gì cho child?
2. Tại sao sau `fork()`, cả hai process có thể đọc cùng dữ liệu nhưng khi ghi thì không ảnh hưởng nhau?
3. Zombie process khác orphan process như thế nào?
4. Tại sao `_exit()` thay vì `exit()` trong child sau exec failure?
5. Double-fork pattern giải quyết vấn đề gì?

---

## 10. Tài liệu tham khảo

| Nội dung | Man page | Ghi chú |
|----------|----------|---------|
| fork() | man 2 fork | trả về PID hoặc 0 hoặc -1 |
| execvp() | man 3 execvp | tìm trong PATH |
| waitpid() | man 2 waitpid | WNOHANG, status macros |
| SIGCHLD | man 7 signal | zombie prevention |
