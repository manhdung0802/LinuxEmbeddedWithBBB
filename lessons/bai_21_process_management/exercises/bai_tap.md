# Bài Tập - Bài 21: Process Management

## Bài tập 1: Process Family Tree

Viết chương trình tạo cây process như sau:
```
main (P)
├── child1 (C1) → tính tổng 1..100, in kết quả rồi exit(sum % 256)
└── child2 (C2) → fork thêm:
    └── grandchild (G) → in "I am grandchild PID=X, PPID=Y" rồi thoát
```
Parent phải dùng `waitpid()` chờ từng child và in exit code của chúng.

**Yêu cầu**: Đảm bảo không có zombie nào sau khi main kết thúc.

---

## Bài tập 2: Shell Mini

Viết một mini shell đơn giản:
```
mysh> ls -la
mysh> echo hello
mysh> exit
```

**Template**:
```c
while (1) {
    printf("mysh> ");
    fflush(stdout);
    fgets(line, sizeof(line), stdin);
    // parse line thành argv[]
    // fork() + execvp(argv[0], argv)
    // parent waitpid()
}
```

**Yêu cầu**:
- Xử lý lệnh `exit` để thoát shell
- Xử lý trường hợp lệnh không tồn tại (execvp trả về -1)
- In exit code của lệnh sau khi kết thúc

---

## Bài tập 3: Parallel Sensor Reader

Mô phỏng đọc 3 cảm biến song song trên BBB:

```c
/* Mỗi child đọc một "cảm biến" (sleep random rồi trả về value qua exit code) */
for (int i = 0; i < 3; i++) {
    pid_t pid = fork();
    if (pid == 0) {
        srand(getpid());
        int sensor_val = rand() % 100;
        sleep(rand() % 3 + 1);  /* 1-3 giây */
        printf("Sensor %d: value=%d\n", i, sensor_val);
        exit(sensor_val);
    }
}
/* Parent thu thập kết quả theo thứ tự kết thúc */
for (int i = 0; i < 3; i++) {
    int status;
    pid_t done = wait(&status);
    printf("PID %d finished, value=%d\n", done, WEXITSTATUS(status));
}
```

**Yêu cầu**: Chạy và giải thích tại sao thứ tự in của parent không đảm bảo bằng thứ tự fork.
