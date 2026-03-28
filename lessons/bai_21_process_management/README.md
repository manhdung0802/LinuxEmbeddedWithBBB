# Bài 21 - Process Management: Tham Khảo Nhanh

## fork() return value
```
pid_t pid = fork();
if (pid < 0)  → lỗi
if (pid == 0) → đang chạy trong CHILD
if (pid > 0)  → đang chạy trong PARENT, pid = PID của child
```

## exec patterns
```c
execvp("ls", args);           // tìm trong PATH, argv array
execl("/bin/ls", "ls", NULL); // path tuyệt đối, list args
```

## wait macros
```c
waitpid(pid, &st, 0);          // block đến khi child xong
waitpid(-1, &st, WNOHANG);     // non-blocking, any child
WIFEXITED(st)   → exit bình thường?
WEXITSTATUS(st) → exit code
WIFSIGNALED(st) → bị kill bởi signal?
WTERMSIG(st)    → signal number
```

## Phòng zombie
```c
signal(SIGCHLD, SIG_IGN);                           // đơn giản nhất
// hoặc handler:
void h(int s) { while(waitpid(-1,NULL,WNOHANG)>0); }
signal(SIGCHLD, h);
```

## /proc info
```bash
ps aux | grep Z          # tìm zombie
cat /proc/PID/status     # thông tin process
pstree -p                # tree với PID
```

## References
- [bai_21_process_management.md](bai_21_process_management.md)
