# Bài 18 - Character Device Driver: Tham Khảo Nhanh

## Sequence tạo char device
```c
alloc_chrdev_region(&dev_num, 0, 1, "name"); // cấp major/minor
cdev_init(&my_cdev, &fops);                  // gán file_operations
cdev_add(&my_cdev, dev_num, 1);              // đăng ký vào kernel
my_class = class_create(THIS_MODULE, "cls"); // tạo /sys/class/cls/
device_create(my_class, NULL, dev_num, NULL, "name"); // tạo /dev/name
```

## Gỡ NGƯỢC khi exit (destroy in reverse order)
```c
device_destroy(my_class, dev_num);
class_destroy(my_class);
cdev_del(&my_cdev);
unregister_chrdev_region(dev_num, 1);
```

## file_operations cần nhớ
```c
static const struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = my_open,
    .release        = my_release,
    .read           = my_read,         /* ssize_t (file*, char __user*, size_t, loff_t*) */
    .write          = my_write,        /* ssize_t (file*, const char __user*, size_t, loff_t*) */
    .unlocked_ioctl = my_ioctl,        /* long (file*, unsigned int, unsigned long) */
};
```

## User/Kernel copy (an toàn)
```c
copy_to_user(user_ptr, kernel_ptr, n);    // kernel → user
copy_from_user(kernel_ptr, user_ptr, n);  // user → kernel
// Trả về 0 = OK, >0 = số byte chưa copy
```

## ioctl macros
```c
#define MY_IOC_MAGIC  'X'
#define CMD_RESET     _IO(MY_IOC_MAGIC,   0)        // no data
#define CMD_GET_VAL   _IOR(MY_IOC_MAGIC,  1, int)   // read int từ kernel
#define CMD_SET_VAL   _IOW(MY_IOC_MAGIC,  2, int)   // write int vào kernel
```

## References
- [bai_18_char_device_driver.md](bai_18_char_device_driver.md)
