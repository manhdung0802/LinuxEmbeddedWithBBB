# Bài 4 - Memory-mapped I/O: /dev/mem và mmap()

## 1. Mục tiêu bài học

- Hiểu memory-mapped I/O là gì: ánh xạ địa chỉ vật lý → ảo
- Biết cách dùng `/dev/mem` cho phép truy cập bộ nhớ vật lý
- Viết code C đầu tiên để đọc/ghi thanh ghi GPIO trực tiếp
- Hiểu syscall `mmap()` và cách sử dụng nó
- So sánh sysfs vs mmap: ưu nhược điểm mỗi cách

---

## 2. Vấn đề: Làm cách nào để truy cập thanh ghi từ userspace?

Bạn biết:
- Thanh ghi GPIO nằm ở địa chỉ vật lý (ví dụ `0x4804C13C`)
- Userspace không thể trực tiếp dùng địa chỉ vật lý
- Kernel quản lý virtual memory - tất cả địa chỉ trong userspace đều ảo

**Câu hỏi**: Làm cách nào từ code C trong userspace, mình có thể ghi vào thanh ghi GPIO vật lý?

**Đáp án**: Dùng `mmap()` để **ánh xạ phần nhớ vật lý → ảo**, rồi ghi qua con trỏ ảo.

---

## 3. Linux virtual memory - tại sao cần mmap()?

### 3.1 Mô hình bộ nhớ trên Linux

Khi userspace chạy, CPU không dùng trực tiếp **physical address** (địa chỉ vật lý RAM).
Thay vào đó, CPU dùng **virtual address** (địa chỉ ảo) qua MMU (Memory Management Unit).

```
Userspace code:
  int *ptr = 0x80000000;      ← đây là virtual address
  *ptr = 1;
         │
         ↓
      MMU (Hardware)
    Translate: 0x80000000 (virtual) → 0x0C00ABC0 (physical)
         │
         ↓
      RAM vật lý tại vị trí 0x0C00ABC0 được ghi
```

### 3.2 Bản đồ virtual address của userspace

```
Userspace:
  0x00000000 ┌─────────────────┐
             │  Code (text)    │
             ├─────────────────┤
             │  Global data    │
             ├─────────────────┤
             │  Heap (malloc)  │
             ├─────────────────┤
             │  Stack          │
             │                 │
  0xFFFFFFFF └─────────────────┘
```

**Quan trọng**: Kernel cấp phát một phạm vi virtual address cho mỗi process. Các process khác nhau thấy cùng virtual address nhưng **map tới physical address khác nhau**.

### 3.3 File `/dev/mem` - cửa sau vào bộ nhớ vật lý

```
/dev/mem
  │
  ├── Cho phép userspace mở file này
  ├── Mỗi byte trong file tương ứng một byte RAM vật lý
  └── Offset trong file = physical address
```

Ví dụ:
- Offset `0x4804C13C` trong `/dev/mem` = thanh ghi GPIO_DATAOUT của GPIO1

### 3.4 Hàm `mmap()` - ánh xạ file → virtual memory

**Hàm signature**:
```c
void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
```

**Ý nghĩa**:
```
mmap(NULL,              // Yêu cầu kernel tìm vùng ảo khả dụng
     4096,              // Ánh xạ 4096 byte (1 page)
     PROT_READ|PROT_WRITE,  // Quyền đọc+ghi
     MAP_SHARED,        // Chia sẻ với các process khác
     fd,                // File descriptor của /dev/mem
     0x4804C000)        // Offset trong file = physical address GPIO1
```

Hàm này **trả về con trỏ ảo** mà bạn có thể dùng để đọc/ghi.

```
Kernel sẽ:
1. Tìm vùng virtual address khả dụng trong process
2. Tạo mapping: virtual → physical address
3. Trả về virtual address cho bạn
```

---

## 4. Ví dụ C: Bật/tắt LED GPIO bằng mmap()

### 4.1 Code đầy đủ: led_mmap.c

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

// GPIO1 base address
#define GPIO1_BASE      0x4804C000
#define GPIO1_SIZE      0x1000          // 4KB

// Offsets cho GPIO1 registers (từ TRM spruh73q.pdf, Chapter 25)
#define GPIO_OE         0x134           // Output Enable (0=output, 1=input)
#define GPIO_DATAOUT    0x13C           // Output data register
#define GPIO_SETDATAOUT 0x194           // Set output bit (write 1 to set)
#define GPIO_CLEARDATAOUT 0x190         // Clear output bit (write 1 to clear)

// GPIO1_21 là bit 21
#define GPIO_BIT        21
#define GPIO_MASK       (1 << GPIO_BIT)

int main() {
    int fd;
    volatile uint32_t *gpio_base;
    
    // Bước 1: Mở /dev/mem
    fd = open("/dev/mem", O_RDWR);
    if (fd < 0) {
        perror("open /dev/mem");
        return 1;
    }
    printf("Opened /dev/mem\n");
    
    // Bước 2: Ánh xạ vùng GPIO1 từ vật lý → ảo
    gpio_base = (volatile uint32_t *)mmap(
        NULL,                           // Để kernel chọn virtual address
        GPIO1_SIZE,                     // Size: 4KB
        PROT_READ | PROT_WRITE,        // Quyền đọc+ghi
        MAP_SHARED,                     // Chia sẻ với kernel
        fd,                             // File descriptor
        GPIO1_BASE                      // Physical address
    );
    
    if (gpio_base == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    printf("Mapped GPIO1 to virtual address: 0x%p\n", (void *)gpio_base);
    
    // Bước 3: Cấu hình GPIO1_21 là output
    // Đọc GPIO_OE
    volatile uint32_t *gpio_oe = gpio_base + (GPIO_OE / 4);
    uint32_t oe_value = *gpio_oe;
    printf("GPIO_OE before: 0x%08X\n", oe_value);
    
    // Ghi GPIO_OE: set bit 21 = 0 (output)
    oe_value &= ~GPIO_MASK;             // Clear bit 21
    *gpio_oe = oe_value;
    printf("GPIO_OE after: 0x%08X\n", *gpio_oe);
    
    sleep(1);
    
    // Bước 4: Bật LED (set GPIO1_21 HIGH)
    printf("\nTurning LED ON...\n");
    volatile uint32_t *gpio_setdataout = gpio_base + (GPIO_SETDATAOUT / 4);
    *gpio_setdataout = GPIO_MASK;       // Write 1 to bit 21 → output goes HIGH
    
    sleep(2);
    printf("LED should be ON now\n");
    
    // Bước 5: Tắt LED (set GPIO1_21 LOW)
    printf("\nTurning LED OFF...\n");
    volatile uint32_t *gpio_cleardataout = gpio_base + (GPIO_CLEARDATAOUT / 4);
    *gpio_cleardataout = GPIO_MASK;     // Write 1 to bit 21 → output goes LOW
    
    sleep(1);
    printf("LED should be OFF now\n");
    
    // Bước 6: Giải phóng tài nguyên
    munmap((void *)gpio_base, GPIO1_SIZE);
    close(fd);
    printf("\nCleaned up\n");
    
    return 0;
}
```

### 4.2 Giải thích từng dòng

**Phần include**:
```c
#include <sys/mman.h>    // mmap, munmap
#include <fcntl.h>       // open()
#include <unistd.h>      // close(), sleep()
```

**Con trỏ volatile**:
```c
volatile uint32_t *gpio_base;
```
- `volatile`: bảo cho compiler ko optimize away việc đọc/ghi repo phần cứng
- `uint32_t`: 32-bit (kích thước một thanh ghi)
- `*`: con trỏ - sủ dụng để lưu virtual address trả về từ mmap()

**Tính toán offset**:
```c
volatile uint32_t *gpio_oe = gpio_base + (GPIO_OE / 4);
```
- `gpio_base` là con trỏ `uint32_t*`, nên `gpio_base + N` tương đương `gpio_base + 4*N` (byte)
- `GPIO_OE = 0x134` byte, nên cần chia cho 4 để chuyển byte → uint32_t offset

**Ghi/đọc thanh ghi**:
```c
uint32_t oe_value = *gpio_oe;           // Đọc
*gpio_oe = oe_value & ~GPIO_MASK;       // Ghi
```

**Giải phóng**:
```c
munmap((void *)gpio_base, GPIO1_SIZE);  // Giải phóng mapping
close(fd);                               // Đóng /dev/mem
```

### 4.3 Biên dịch và chạy

```bash
# Biên dịch
gcc -o led_mmap led_mmap.c

# Chạy (cần root)
sudo ./led_mmap
```

**Output mong đợi**:
```
Opened /dev/mem
Mapped GPIO1 to virtual address: 0x76fb4000
GPIO_OE before: 0xDFFDFFFF
GPIO_OE after: 0xDFFDFFDF

Turning LED ON...
LED should be ON now

Turning LED OFF...
LED should be OFF now

Cleaned up
```

---

## 5. Lỗi thường gặp và cách khắc phục

### Lỗi 1: Permission denied

```
Opened /dev/mem
mmap: Permission denied
```

**Nguyên nhân**: `/dev/mem` yêu cầu root.

**Cách khắc phục**:
```bash
sudo ./led_mmap
```

### Lỗi 2: Segmentation fault

```c
*gpio_base = 0;     // Crash!
```

**Nguyên nhân**: mmap() không cho phép ghi vào vùng kernel code.

**Cách khắc phục**: Chỉ ghi vào địa chỉ thanh ghi hợp lệ (kiểm tra TRM).

### Lỗi 3: LED không bật/tắt

**Nguyên nhân**: 
- GPIO1 chưa được enable (clock chưa bật)
- Pinmux chưa cấu hình cho P8.3
- Dùng số GPIO sai

**Cách khắc phục**: Dùng sysfs để kiểm tra GPIO hoạt động trước.

---

## 6. So sánh: sysfs vs mmap()

| Khía cạnh | sysfs | mmap() |
|-----------|-------|--------|
| **Dễ dùng** | Rất dễ (shell: `echo`) | Phức tạp hơn (viết C) |
| **Hiểu Register** | Ít nhìn thấy chi tiết | Thấy rõ mỗi thanh ghi |
| **Tốc độ** | Chậm (qua kernel) | Nhanh (trực tiếp) |
| **An toàn** | An toàn (kernel kiểm soát) | Dễ nhầm lẫn, nguy hiểm |
| **Học tập** | Dễ bắt đầu | Nắm vững hardware |
| **Sản phẩm** | Dùng cho ứng dụng | Tránh (không portable) |

### 6.1 Khi nào dùng sysfs?

- Ứng dụng userspace thông thường
- Không cần tốc độ cực cao
- Muốn kernel kiểm soát + bảo vệ

**Ví dụ**:
```bash
echo 1 > /sys/class/leds/usr0/brightness   # Bật LED
```

### 6.2 Khi nào dùng mmap()?

- Học tập, hiểu hardware sâu
- Real-time cần tốc độ cao
- Debug driver
- Bare-metal/firmware code

**Ví dụ**:
```c
*gpio_setdataout = GPIO_MASK;        // Bật LED ngay lập tức
```

---

## 7. Device Tree có liên quan gì?

Device Tree **không can thiệp** trực tiếp vào mmap().

Nhưng Device Tree **rất quan trọng** vì:
- Nó nói cho kernel những GPIO nào được enable
- Nếu GPIO không được declare trong DTS, kernel có thể ko load driver
- Nếu ko load driver, pinmux có thể ko được cấu hình

**Ví dụ**:
```dts
gpio1: gpio@4804c000 {
    status = "okay";        // Enable GPIO1 - quan trọng!
    ...
};
```

Nếu `status = "disabled"`, clock GPIO1 ko được bật → mmap() đọc được lệnh nhưng phần cứng ko chạy.

---

## 8. Ví dụ nâng cao: Bài viết một hàm wrapper

Thường bạn muốn viết code sạch hơn, dùng hàm wrapper:

```c
// Cấu trúc để lưu trữ GPIO mapping
typedef struct {
    int fd;
    volatile uint32_t *base;
    uint32_t size;
    uint32_t phys_addr;
} gpio_mmap_t;

// Khởi tạo GPIO mapping
int gpio_mmap_init(gpio_mmap_t *gpio, uint32_t phys_addr, uint32_t size) {
    gpio->phys_addr = phys_addr;
    gpio->size = size;
    
    gpio->fd = open("/dev/mem", O_RDWR);
    if (gpio->fd < 0) {
        perror("open /dev/mem");
        return -1;
    }
    
    gpio->base = (volatile uint32_t *)mmap(
        NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
        gpio->fd, phys_addr
    );
    
    if (gpio->base == MAP_FAILED) {
        perror("mmap");
        close(gpio->fd);
        return -1;
    }
    
    return 0;
}

// Đọc thanh ghi
uint32_t gpio_read_reg(gpio_mmap_t *gpio, uint32_t offset) {
    return *(gpio->base + (offset / 4));
}

// Ghi thanh ghi
void gpio_write_reg(gpio_mmap_t *gpio, uint32_t offset, uint32_t value) {
    *(gpio->base + (offset / 4)) = value;
}

// Giải phóng
void gpio_mmap_cleanup(gpio_mmap_t *gpio) {
    munmap((void *)gpio->base, gpio->size);
    close(gpio->fd);
}
```

**Cách dùng**:
```c
gpio_mmap_t gpio1;
gpio_mmap_init(&gpio1, GPIO1_BASE, GPIO1_SIZE);

// Bật output
uint32_t oe = gpio_read_reg(&gpio1, GPIO_OE);
gpio_write_reg(&gpio1, GPIO_OE, oe & ~GPIO_MASK);

// Set output HIGH
gpio_write_reg(&gpio1, GPIO_SETDATAOUT, GPIO_MASK);

gpio_mmap_cleanup(&gpio1);
```

---

## 9. Kiến thức cốt lõi cần nhớ

1. **Virtual memory**: Userspace dùng virtual address, kernel dùng MMU để dịch → physical.
2. **`/dev/mem`**: File đặc biệt cho phép truy cập RAM vật lý.
3. **`mmap()`**: Ánh xạ physical address → virtual address trong userspace.
4. **`volatile`**: Bảo cho compiler ko optimize away việc đọc/ghi phần cứng.
5. **Thanh ghi = con trỏ**: `*gpio_base` đó chính là giá trị thanh ghi.
6. **Device Tree**: Nói cho kernel GPIO nào được enable - ảnh hưởng tới khả năng truy cập.

---

## 10. Câu hỏi kiểm tra

1. Giải thích tại sao userspace ko thể dùng trực tiếp physical address để truy cập RAM.
2. Hàm `mmap()` làm gì? Tham số chính là gì?
3. Tại sao cần dùng `volatile` khi khai báo con trỏ thanh ghi?
4. Để bật LED GPIO1_21 bằng mmap(), bạn thực hiện bao nhiêu bước? Liệt kê.
5. So sánh `echo` vào sysfs vs viết code C mmap(): cách nào nhanh hơn? Tại sao?

---

## 11. Chuẩn bị cho bài sau

Bài 5 sẽ đi vào:

**GPIO - Bật/tắt LED qua C và lập trình interrupt**

Ta sẽ:
- Hiểu Control Module (pinmux) là gì
- Cấu hình chân P8/P9 thành GPIO
- Bật GPIO interrupt để nhận sự kiện từ nút bấm
- So sánh polling vs interrupt

---

## 12. Ghi nhớ một câu

Trên Linux embedded, **mmap() là cầu nối từ userspace tới phần cứng** - biết dùng mmap() tức là bạn nắm được cách phần cứng hoạt động ở mức nền tảng.
