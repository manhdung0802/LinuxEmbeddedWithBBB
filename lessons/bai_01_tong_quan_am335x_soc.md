# Bài 1 - Tổng quan AM335x SoC

## 1. Mục tiêu bài học
- Hiểu AM335x là gì và vì sao BeagleBone Black dùng chip này.
- Nắm được các khối chính bên trong SoC: CPU, bộ nhớ, bus, ngoại vi.
- Đọc được memory map ở mức tổng quan.
- Phân biệt góc nhìn bare-metal và Linux khi làm việc với phần cứng.

---

## 2. SoC là gì?

SoC là viết tắt của System on Chip, tức là cả một hệ thống được tích hợp vào một chip duy nhất.

Với AM335x, bên trong chip không chỉ có CPU mà còn có:
- Bộ điều khiển DDR
- GPIO
- UART, I2C, SPI
- Timer, PWM, ADC
- DMA
- Bộ điều khiển clock và reset
- Bộ điều khiển ngắt
- PRU-ICSS cho tác vụ real-time

Điểm quan trọng nhất cần nhớ:

CPU không "nói chuyện" với ngoại vi bằng phép màu. CPU chỉ đọc và ghi vào các địa chỉ nhớ đặc biệt. Các địa chỉ đó chính là thanh ghi của phần cứng.

Đây là nền tảng của toàn bộ embedded Linux trên AM335x.

---

## 3. AM335x trên BeagleBone Black

BeagleBone Black dùng họ TI Sitara AM335x, thường gặp bản AM3358BZCZ100.

Các thông tin tổng quan:
- Kiến trúc CPU: ARM Cortex-A8
- Kiến trúc lệnh: ARMv7-A
- Xung nhịp: tới 1 GHz tùy phiên bản
- Có MMU, cache, NEON, VFP
- Có DDR ngoài để chạy Linux
- Có nhiều ngoại vi tích hợp sẵn nên rất phù hợp để học register-level programming

Trên BeagleBone Black, AM335x là trung tâm của cả board:
- Nhận boot từ eMMC, microSD hoặc các nguồn boot khác
- Chạy U-Boot, Linux kernel, root filesystem
- Điều khiển các chân P8/P9 thông qua các module bên trong chip

---

## 4. Bên trong AM335x có gì?

Hãy hình dung chip theo sơ đồ tư duy sau:

1. CPU Cortex-A8
2. Hệ thống bus/interconnect
3. Bộ nhớ trong và bộ nhớ ngoài
4. Các peripheral memory-mapped
5. Clock, reset, interrupt

### 4.1 CPU Cortex-A8

Cortex-A8 là application processor, khác với các Cortex-M thường gặp ở vi điều khiển.

Nó có:
- Pipeline sâu hơn
- MMU để chạy Linux và virtual memory
- Cache L1/L2 để tăng tốc
- Chế độ User và Kernel
- Khả năng chạy hệ điều hành đầy đủ

Ý nghĩa thực tế:
- Nếu viết bare-metal, bạn làm việc gần phần cứng nhất.
- Nếu viết trên Linux userspace, bạn làm việc thông qua kernel, driver, sysfs, mmap(), /dev/mem.

### 4.2 Interconnect

Các khối phần cứng không nối trực tiếp kiểu "dây thẳng" với CPU. Chúng được nối qua hệ thống interconnect nội bộ.

Vai trò của interconnect:
- Chuyển truy cập đọc/ghi từ CPU tới peripheral
- Điều phối truy cập bộ nhớ
- Kết nối DMA, CPU, ngoại vi

Hiểu đơn giản:

Khi bạn ghi một giá trị vào địa chỉ GPIO, CPU phát sinh giao dịch bus. Interconnect sẽ chuyển giao dịch đó tới module GPIO tương ứng.

### 4.3 Bộ nhớ

AM335x có nhiều loại vùng nhớ:
- Boot ROM: chứa mã khởi động ban đầu của chip
- On-chip SRAM: RAM trong chip, tốc độ cao nhưng dung lượng nhỏ
- DDR ngoài: nơi Linux kernel, rootfs, chương trình userspace hoạt động
- Vùng thanh ghi peripheral: mỗi module có một dải địa chỉ riêng

Điều rất quan trọng:

Thanh ghi peripheral không phải RAM thông thường. Ghi vào đó là bạn đang điều khiển phần cứng.

### 4.4 Peripheral

Đây là phần bạn sẽ dùng thường xuyên nhất trong các bài tới:
- GPIO: xuất/nhập số
- UART: serial console, giao tiếp thiết bị khác
- I2C: cảm biến, EEPROM
- SPI: ADC, DAC, display, IC ngoài
- Timer/PWM: tạo thời gian, phát xung
- ADC: đọc tín hiệu analog
- PRU: xử lý real-time độc lập với CPU chính

### 4.5 Clock, reset, interrupt

Ba khối nền tảng mà người mới thường bỏ sót:

- Clock: module không có clock thì gần như không hoạt động
- Reset: module có thể đang ở trạng thái reset
- Interrupt: phần cứng báo sự kiện cho CPU

Sau này khi gặp lỗi "ghi thanh ghi mà không thấy tác dụng", nguyên nhân rất hay gặp là:
- Chưa bật clock
- Chưa cấu hình pinmux
- Ghi sai bank GPIO
- Đọc/ghi sai offset thanh ghi

---

## 5. Memory Map tổng quan

Memory map là bản đồ địa chỉ của chip. Mỗi khối phần cứng chiếm một vùng địa chỉ riêng.

Khi đọc TRM `BBB_docs/datasheets/spruh73q.pdf`, bạn sẽ thấy mỗi peripheral có:
- Base address
- Offset của từng thanh ghi
- Ý nghĩa bit-field

Ví dụ các base address rất hay dùng trên AM335x:

| Khối | Base address |
|------|--------------|
| CM_PER | 0x44E00000 |
| GPIO0 | 0x44E07000 |
| UART0 | 0x44E09000 |
| CONTROL MODULE | 0x44E10000 |
| GPIO1 | 0x4804C000 |

Ý nghĩa:
- Nếu muốn truy cập một thanh ghi của GPIO1, bạn lấy base address của GPIO1 cộng với offset của thanh ghi đó.
- Công thức chung:

```text
địa_chỉ_thanh_ghi = base_address + offset
```

Ví dụ tư duy:
- `GPIO1 base = 0x4804C000`
- Nếu thanh ghi nào đó có offset `0x134`
- Địa chỉ thật sẽ là `0x4804C134`

Từ bài 4 trở đi, bạn sẽ dùng đúng kiểu tính địa chỉ này khi `mmap()` hoặc khi viết bare-metal.

---

## 6. Linux nhìn AM335x như thế nào?

Trên Linux, bạn không trực tiếp sở hữu toàn bộ phần cứng như ở bare-metal.

Linux kernel đứng giữa:
- Kernel quản lý virtual memory
- Kernel quản lý driver
- Kernel quyết định module nào được truy cập theo cách nào

Vì vậy có 3 mức làm việc phổ biến:

### Cách 1: Dùng interface có sẵn của kernel
Ví dụ:
- `/dev/ttyS0`
- sysfs
- libgpiod

Ưu điểm:
- An toàn hơn
- Dễ dùng
- Phù hợp ứng dụng Linux thông thường

Nhược điểm:
- Ít thấy rõ bản chất thanh ghi

### Cách 2: Dùng `mmap()` hoặc `/dev/mem` trong userspace

Ưu điểm:
- Học sâu về memory-mapped I/O
- Thấy rõ base address, offset, bit

Nhược điểm:
- Dễ sai
- Có thể xung đột với kernel driver
- Không phải cách tốt nhất cho sản phẩm hoàn chỉnh

### Cách 3: Viết kernel driver

Ưu điểm:
- Đúng kiến trúc Linux
- Quản lý tài nguyên tốt hơn

Nhược điểm:
- Khó hơn nhiều

Trong khóa học này, ta sẽ dùng cả 3 cách, nhưng ưu tiên hiểu thanh ghi trước.

---

## 7. Bare-metal và Linux khác nhau thế nào?

| Chủ đề | Bare-metal | Linux |
|--------|------------|-------|
| Quyền truy cập phần cứng | Trực tiếp | Qua kernel hoặc ánh xạ bộ nhớ |
| Khởi tạo hệ thống | Tự làm | Bootloader + kernel làm sẵn nhiều phần |
| Bộ nhớ | Physical address là chính | Có virtual memory |
| Interrupt | Tự viết vector/ISR | Qua kernel framework |
| Tốc độ phản ứng | Rất tốt | Kém quyết định hơn vì có scheduler |
| Phù hợp | Điều khiển tối giản, deterministic | Hệ thống phức tạp, network, filesystem |

Với BeagleBone Black:
- Nếu cần Linux, network, file system, giao diện phong phú: dùng Linux
- Nếu cần timing rất cứng: cân nhắc PRU hoặc bare-metal

---

## 8. Cách đọc TRM hiệu quả

TRM của AM335x rất dày. Không nên đọc từ đầu tới cuối.

Cách đúng:

1. Xác định module cần học, ví dụ GPIO
2. Tìm chapter của module đó trong `spruh73q.pdf`
3. Ghi lại:
   - Base address
   - Các thanh ghi quan trọng
   - Reset value
   - Bit field cần dùng
4. Kiểm tra module clock có phải bật trước không
5. Kiểm tra pinmux trong Control Module

Tư duy này sẽ lặp lại cho UART, I2C, SPI, Timer, ADC.

---

## 9. Kiến thức cốt lõi cần nhớ sau bài này

1. AM335x là một SoC, không chỉ là CPU.
2. Mọi ngoại vi được điều khiển qua memory-mapped registers.
3. Mỗi module có base address riêng trong memory map.
4. Linux không cho userspace đụng phần cứng một cách tùy tiện như bare-metal.
5. Khi học ngoại vi, luôn nhìn đủ 3 phần:
   - clock
   - pinmux
   - thanh ghi của module

---

## 10. Chuẩn bị cho bài sau

Bài sau sẽ đi vào:

**BeagleBone Black hardware overview**

Ta sẽ học:
- Sơ đồ khối của board
- DDR, eMMC, PMIC, oscillator nằm ở đâu
- Header P8/P9 nối với các module trong AM335x như thế nào
- Vì sao một chân trên header có thể mang nhiều chức năng

---

## 11. Câu hỏi kiểm tra

Trả lời ngắn gọn 5 câu này để tôi biết bạn đã nắm bài chưa:

1. SoC khác vi điều khiển rời rạc ở điểm nào?
2. Vì sao nói GPIO/UART/I2C là memory-mapped peripheral?
3. Nếu biết `base address` và `offset`, bạn tính địa chỉ thanh ghi như thế nào?
4. Trên Linux userspace, vì sao không nên tùy ý truy cập thanh ghi nếu chưa hiểu kernel đang làm gì?
5. Khi một module không hoạt động, ba thứ đầu tiên cần kiểm tra là gì?

---

## 12. Ghi nhớ một câu

Trên AM335x, học phần cứng là học cách biến địa chỉ nhớ thành hành vi vật lý.