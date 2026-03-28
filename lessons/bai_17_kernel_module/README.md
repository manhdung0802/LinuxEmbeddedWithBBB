# Bài 17 - Linux Kernel Module: Tham Khảo Nhanh

## Makefile chuẩn
```makefile
obj-m += my_module.o
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
```

## Lệnh thiết yếu
```bash
make                          # build
sudo insmod my_module.ko      # load
sudo rmmod my_module          # unload
lsmod | grep my               # kiểm tra
modinfo my_module.ko          # thông tin
dmesg | tail -20              # xem log
dmesg -w                      # watch live
```

## Macro cần nhớ
```c
MODULE_LICENSE("GPL");
module_init(init_fn);
module_exit(exit_fn);
module_param(var, type, perm);
pr_info("msg %d\n", val);
```

## Kernel I/O (thay mmap)
```c
void __iomem *base = ioremap(PHYS_ADDR, SIZE);
u32 val = ioread32(base + OFFSET);
iowrite32(val, base + OFFSET);
iounmap(base);
```

## References
- [bai_17_kernel_module.md](bai_17_kernel_module.md)
- spruh73q.pdf, Table 2-3 (memory map)
