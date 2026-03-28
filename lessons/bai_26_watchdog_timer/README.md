# Bài 26 - Watchdog Timer: Tham Khảo Nhanh

## /dev/watchdog Usage
```c
int wfd = open("/dev/watchdog", O_RDWR);    // timer STARTS immediately!
write(wfd, "\0", 1);                         // kick (keep alive)
write(wfd, "V", 1); close(wfd);             // safe stop (magic close)
```

## ioctl commands
```c
ioctl(wfd, WDIOC_GETSUPPORT,    &info);          // watchdog_info struct
ioctl(wfd, WDIOC_SETTIMEOUT,    &timeout_sec);   // set timeout
ioctl(wfd, WDIOC_GETTIMEOUT,    &timeout_sec);   // get timeout
ioctl(wfd, WDIOC_GETTIMELEFT,   &timeleft_sec);  // remaining time
ioctl(wfd, WDIOC_GETBOOTSTATUS, &bootstatus);    // was last reset by WDT?
ioctl(wfd, WDIOC_KEEPALIVE,     NULL);           // alternative kick
```

## Check last reset cause
```c
int bs;
ioctl(wfd, WDIOC_GETBOOTSTATUS, &bs);
if (bs & WDIOF_CARDRESET) printf("Last reset: WATCHDOG!\n");
```

## sysfs watchdog
```bash
cat /sys/class/watchdog/watchdog0/timeout    # current timeout
cat /sys/class/watchdog/watchdog0/timeleft   # seconds remaining
cat /sys/class/watchdog/watchdog0/state      # active | inactive
```

## Safe daemon pattern
```
timeout = 30s, kick_interval = 10s   → ratio 1:3 = safe
if (system_healthy) kick; else do_nothing → WDT resets board
```

## AM335x Hardware
- WDT1 base: 0x44E35000 (spruh73q.pdf Table 2-3)
- Linux driver: `drivers/watchdog/omap_wdt.c`

## References
- [bai_26_watchdog_timer.md](bai_26_watchdog_timer.md)
- spruh73q.pdf, Section 20 (Watchdog Timer)
