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

**Giải thích kỹ hơn - tại sao "cùng virtual address nhưng khác physical address"?**

Kernel duy trì một **page table** (bảng trang) riêng cho mỗi process. Khi CPU thực thi lệnh truy cập bộ nhớ, MMU tra page table của process đang chạy để dịch virtual → physical.

```
Process A:  0x80000000 (virtual)  ──page table A──→  0x0010A000 (physical)
Process B:  0x80000000 (virtual)  ──page table B──→  0x0020B000 (physical)
```

- Cùng con số `0x80000000` nhưng hai process trỏ tới **hai vùng RAM vật lý hoàn toàn khác nhau**.
- Process A không thể đọc/ghi vùng nhớ của Process B → đây là cơ chế **cô lập bộ nhớ** (memory isolation), giúp hệ thống ổn định và bảo mật.
- Nếu muốn chia sẻ dữ liệu giữa hai process, phải dùng `mmap(MAP_SHARED)` hoặc các cơ chế IPC — kernel sẽ ánh xạ cùng một physical page vào page table của cả hai process.

**Liên hệ với `/dev/mem` + `mmap()`**: Khi bạn gọi `mmap()` trên `/dev/mem`, kernel thêm một entry vào page table của process hiện tại, ánh xạ physical address phần cứng (ví dụ thanh ghi GPIO) vào một vùng virtual address — mapping này chỉ có tác dụng trong process đã gọi.

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

**Cách xác định địa chỉ `0x4804C13C`**: Tra từ **TRM (Technical Reference Manual)** của AM335x:

1. **Tra Memory Map** (TRM Chapter 2) → tìm base address của GPIO1:
   - GPIO1 base = `0x4804C000`
2. **Tra Register Map của GPIO module** (TRM Chapter 25) → tìm offset của thanh ghi DATAOUT:
   - `GPIO_DATAOUT` offset = `0x13C`
3. **Cộng lại**: `0x4804C000 + 0x13C = 0x4804C13C`

Bạn cũng có thể kiểm chứng trên board:
```bash
# Xem vùng I/O mà kernel đã ghi nhận
sudo grep -i gpio /proc/iomem

# Đọc trực tiếp giá trị thanh ghi (nếu có devmem2)
sudo devmem2 0x4804C13C w
```

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

### 4.2 Giải thích chi tiết từng dòng code

#### a) Phần `#include` — khai báo thư viện

```c
#include <stdio.h>       // printf(), perror()
#include <stdlib.h>      // exit(), macros
#include <stdint.h>      // uint32_t — kiểu số nguyên 32-bit chính xác
#include <fcntl.h>       // open(), O_RDWR — mở file với quyền đọc+ghi
#include <unistd.h>      // close(), sleep() — đóng file, chờ N giây
#include <sys/mman.h>    // mmap(), munmap(), MAP_SHARED, PROT_READ...
#include <string.h>      // memset() (dự phòng, không bắt buộc ở đây)
```

#### b) Phần `#define` — hằng số địa chỉ và offset

```c
#define GPIO1_BASE      0x4804C000      // Địa chỉ vật lý bắt đầu của module GPIO1
                                        // (tra từ TRM Chapter 2 - Memory Map)
#define GPIO1_SIZE      0x1000          // 0x1000 = 4096 byte = 4KB
                                        // Đây là kích thước vùng thanh ghi của GPIO1
                                        // mmap() yêu cầu size là bội số của page (4KB)
```

```c
#define GPIO_OE         0x134           // Offset của thanh ghi Output Enable
                                        // Bit = 0 → chân đó là output
                                        // Bit = 1 → chân đó là input (mặc định)
#define GPIO_DATAOUT    0x13C           // Offset của thanh ghi Data Output
                                        // Đọc thanh ghi này để biết trạng thái output hiện tại
#define GPIO_SETDATAOUT 0x194           // Ghi bit = 1 → set chân tương ứng lên HIGH
                                        // Ghi bit = 0 → không ảnh hưởng (không clear)
#define GPIO_CLEARDATAOUT 0x190         // Ghi bit = 1 → clear chân tương ứng xuống LOW
                                        // Ghi bit = 0 → không ảnh hưởng (không set)
```

> **Lưu ý**: `SETDATAOUT` và `CLEARDATAOUT` là hai thanh ghi riêng biệt. Chỉ cần ghi 1 vào bit muốn thay đổi, các bit khác giữ nguyên (không cần đọc-sửa-ghi). Đây là thiết kế set/clear phổ biến trên ARM SoC.

```c
#define GPIO_BIT        21              // LED usr0 trên BBB nối tới GPIO1, bit 21
#define GPIO_MASK       (1 << GPIO_BIT) // = (1 << 21) = 0x00200000
                                        // Đây là mặt nạ bit: chỉ bit 21 = 1, còn lại = 0
```

**Phép toán `1 << 21`**:
```
  Số 1 dạng nhị phân 32-bit:
  0000 0000 0000 0000 0000 0000 0000 0001

  Dịch trái 21 vị trí (1 << 21):
  0000 0000 0010 0000 0000 0000 0000 0000
  = 0x00200000
```

#### c) Khai báo biến

```c
int fd;                         // File descriptor — số nguyên đại diện cho file đã mở
                                // Sẽ nhận giá trị từ open("/dev/mem", ...)
volatile uint32_t *gpio_base;   // Con trỏ tới vùng nhớ GPIO1 sau khi mmap()
```

- **`volatile`**: Từ khóa bắt buộc khi truy cập thanh ghi phần cứng. Nó nói với compiler: "Giá trị tại địa chỉ này có thể thay đổi bất cứ lúc nào (do phần cứng), **đừng tối ưu hóa** bằng cách cache giá trị trong register CPU". Nếu thiếu `volatile`, compiler có thể bỏ qua lệnh đọc/ghi thanh ghi → chương trình chạy sai.
- **`uint32_t *`**: Con trỏ tới giá trị 32-bit. Mỗi thanh ghi GPIO là 32-bit (4 byte), nên kiểu `uint32_t*` là phù hợp.

#### d) Bước 1 — Mở `/dev/mem`

```c
fd = open("/dev/mem", O_RDWR);
```

- `open()`: syscall mở file, trả về file descriptor (số nguyên ≥ 0 nếu thành công, -1 nếu lỗi)
- `"/dev/mem"`: file đặc biệt đại diện cho toàn bộ bộ nhớ vật lý
- `O_RDWR`: mở với quyền đọc **và** ghi (cần cả hai vì ta sẽ đọc thanh ghi OE và ghi thanh ghi SET/CLEAR)

```c
if (fd < 0) {           // fd < 0 nghĩa là open() thất bại
    perror("open /dev/mem");  // In lỗi kèm mô tả (ví dụ: "Permission denied")
    return 1;            // Thoát chương trình với mã lỗi
}
```

#### e) Bước 2 — Gọi `mmap()` ánh xạ vùng GPIO1

```c
gpio_base = (volatile uint32_t *)mmap(
    NULL,                           // addr: NULL = để kernel tự chọn virtual address
    GPIO1_SIZE,                     // length: 0x1000 = 4096 byte cần ánh xạ
    PROT_READ | PROT_WRITE,        // prot: quyền đọc + ghi trên vùng nhớ này
    MAP_SHARED,                     // flags: ghi vào vùng này sẽ ảnh hưởng tới
                                    //        phần cứng thật (không phải bản sao riêng)
    fd,                             // fd: file descriptor của /dev/mem
    GPIO1_BASE                      // offset: 0x4804C000 = physical address GPIO1
);
```

**Kết quả**: Kernel tạo ánh xạ trong page table:
```
gpio_base (virtual, ví dụ 0x76fb4000) → 0x4804C000 (physical, GPIO1 trên chip)
```

Sau lệnh này, ghi vào `*gpio_base` tương đương ghi vào thanh ghi phần cứng GPIO1 tại địa chỉ `0x4804C000`.

```c
if (gpio_base == MAP_FAILED) {   // MAP_FAILED = (void *)-1, nghĩa là mmap() thất bại
    perror("mmap");               // In lỗi (thường là "Permission denied" nếu ko root)
    close(fd);                    // Đóng file descriptor trước khi thoát
    return 1;
}
```

#### f) Bước 3 — Cấu hình GPIO1_21 là output

```c
volatile uint32_t *gpio_oe = gpio_base + (GPIO_OE / 4);
```

**Phép toán con trỏ — tại sao chia cho 4?**

- `gpio_base` có kiểu `uint32_t*` → mỗi đơn vị `+1` tương đương **+4 byte** (vì `sizeof(uint32_t) = 4`)
- `GPIO_OE = 0x134` là offset tính bằng **byte**
- Để chuyển byte offset → phần tử `uint32_t`: `0x134 / 4 = 0x4D = 77`
- Vậy `gpio_base + 77` = virtual address của thanh ghi `GPIO_OE`

```
gpio_base          → virtual addr + 0x000  (thanh ghi đầu tiên)
gpio_base + 1      → virtual addr + 0x004  (thanh ghi thứ 2)
...
gpio_base + 77     → virtual addr + 0x134  (thanh ghi GPIO_OE)
```

```c
uint32_t oe_value = *gpio_oe;    // Đọc giá trị hiện tại của thanh ghi GPIO_OE
                                 // Ví dụ: 0xFFFFFFFF (tất cả 32 chân đều là input)
```

```c
oe_value &= ~GPIO_MASK;          // Clear bit 21 → set chân đó thành output
```

**Phép toán bit chi tiết**:
```
GPIO_MASK  = 0x00200000 = 0000 0000 0010 0000 0000 0000 0000 0000
~GPIO_MASK = 0xFFDFFFFF = 1111 1111 1101 1111 1111 1111 1111 1111

oe_value (trước) = 0xFFFFFFFF = 1111 1111 1111 1111 1111 1111 1111 1111
oe_value & ~MASK = 0xFFDFFFFF = 1111 1111 1101 1111 1111 1111 1111 1111
                                              ↑ bit 21 = 0 → output
```

- `~` (NOT): đảo tất cả bit → tạo mặt nạ "tất cả 1 trừ bit 21"
- `&=` (AND-assign): giữ nguyên tất cả bit khác, chỉ xóa bit 21 về 0
- Trong thanh ghi `GPIO_OE`: bit = 0 → output, bit = 1 → input

```c
*gpio_oe = oe_value;              // Ghi giá trị mới vào thanh ghi GPIO_OE
                                  // Từ lúc này, GPIO1_21 được cấu hình là output
```

#### g) Bước 4 — Bật LED (set chân HIGH)

```c
volatile uint32_t *gpio_setdataout = gpio_base + (GPIO_SETDATAOUT / 4);
// gpio_base + (0x194 / 4) = gpio_base + 0x65 = virtual addr của thanh ghi SETDATAOUT

*gpio_setdataout = GPIO_MASK;     // Ghi 0x00200000 vào thanh ghi SETDATAOUT
                                  // → bit 21 = 1 → chân GPIO1_21 lên mức HIGH → LED sáng
                                  // Các bit khác = 0 → không ảnh hưởng các chân còn lại
```

#### h) Bước 5 — Tắt LED (clear chân LOW)

```c
volatile uint32_t *gpio_cleardataout = gpio_base + (GPIO_CLEARDATAOUT / 4);
// gpio_base + (0x190 / 4) = gpio_base + 0x64 = virtual addr của thanh ghi CLEARDATAOUT

*gpio_cleardataout = GPIO_MASK;   // Ghi 0x00200000 vào thanh ghi CLEARDATAOUT
                                  // → bit 21 = 1 → clear chân GPIO1_21 xuống LOW → LED tắt
```

#### i) Bước 6 — Giải phóng tài nguyên

```c
munmap((void *)gpio_base, GPIO1_SIZE);
// Xóa ánh xạ virtual → physical ra khỏi page table
// Sau lệnh này, truy cập gpio_base sẽ gây segfault
// (void *): ép kiểu vì munmap() nhận void*, không phải volatile uint32_t*

close(fd);
// Đóng file descriptor /dev/mem
// Giải phóng tài nguyên kernel đã cấp cho file này
```

> **Quy tắc**: Luôn gọi `munmap()` trước `close()`. Nếu chương trình chạy trong vòng lặp vô hạn, cần bắt tín hiệu `SIGINT` (Ctrl+C) bằng `signal()` để cleanup trước khi thoát.

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

### Lỗi 4: `sleep(0.5)` — vòng lặp chạy liên tục, terminal bị spam

```c
sleep(0.5);   // ⚠️ BUG: sleep() nhận unsigned int, 0.5 bị truncate thành 0!
```

**Nguyên nhân**: Hàm `sleep()` có prototype `unsigned int sleep(unsigned int seconds)`. Khi truyền `0.5` (double), trình tạo bytecode phát sinh **implicit conversion** `double → unsigned int`, kết quả = **0**. Vòng lặp chạy không chờ → in liên tục.

**Cách khắc phục**: Dùng `usleep()` (microseconds) hoặc `nanosleep()`:

```c
// Cách 1: usleep() — đơn vị microsecond (µs)
usleep(500000);       // 500000 µs = 0.5 giây

// Cách 2: nanosleep() — POSIX, chính xác hơn
#include <time.h>
struct timespec ts = { .tv_sec = 0, .tv_nsec = 500000000L };
nanosleep(&ts, NULL); // 500ms
```

> **Bài học**: Trong C, luôn kiểm tra kiểu tham số của hàm. `sleep()` = giây (unsigned int), `usleep()` = µs, `nanosleep()` = ns.

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

**Đáp án (đã hiệu chỉnh và tóm tắt)**

- **1 — Vì sao userspace không truy cập trực tiếp physical address:** Userspace chạy trong không gian địa chỉ ảo quản lý bởi kernel/MMU. Việc cho phép truy cập trực tiếp physical address phá vỡ tách biệt bộ nhớ giữa các tiến trình và làm mất an toàn hệ thống; kernel sử dụng page table để ánh xạ và kiểm soát quyền truy cập, do đó userspace chỉ được phép thông qua các interface của kernel (ví dụ `mmap` trên `/dev/mem`, driver, sysfs) khi được phép.

- **2 — `mmap()` làm gì và tham số chính:** `mmap()` tạo một ánh xạ giữa một vùng trong file/device (ở đây `/dev/mem` — offset = physical address) và virtual address của tiến trình. Signature có 6 tham số: `addr, length, prot, flags, fd, offset` (tức là địa chỉ đề xuất, kích thước, quyền, cờ, file descriptor, offset trong file).

- **3 — Tại sao cần `volatile`:** Các thanh ghi phần cứng có thể thay đổi độc lập với luồng chương trình (do phần cứng hoặc các CPU khác), và compile có thể tối ưu hóa (cache hoặc reorder) các lần đọc/ghi; `volatile` buộc compiler luôn thực hiện đọc/ghi thực tế tới địa chỉ đó, tránh tối ưu làm sai hành vi với phần cứng.

- **4 — Các bước bật LED GPIO1_21 bằng mmap():**
    - Mở `/dev/mem` với quyền đọc/ghi (cần root).
    - Gọi `mmap()` để ánh xạ vùng chứa `GPIO1` (ví dụ `GPIO1_BASE`, `GPIO1_SIZE`) vào không gian ảo của tiến trình.
    - Đọc thanh ghi `GPIO_OE`, clear bit 21 (set thành 0) để cấu hình chân là output, rồi ghi lại `GPIO_OE`.
    - Ghi `GPIO_MASK` vào `GPIO_SETDATAOUT` để đặt chân HIGH (bật LED); ghi vào `GPIO_CLEARDATAOUT` để clear (tắt LED).
    - `munmap()` và `close(fd)` khi hoàn tất.

- **5 — Tốc độ: mmap() vs sysfs:** `mmap()` thường nhanh hơn so với thao tác qua sysfs vì mã userspace thao tác trực tiếp lên thanh ghi (ghi bộ nhớ ảo đã ánh xạ) và tránh vòng gọi hệ thống/điều khiển kernel cho mỗi thao tác. Sysfs đi qua API kernel (syscalls, xử lý chuỗi, kiểm tra quyền) nên có chi phí overhead cao hơn; tuy nhiên mmap yêu cầu cẩn trọng vì bỏ qua nhiều cơ chế bảo vệ kernel.

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
