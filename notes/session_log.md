# 📝 Session Log - Lịch sử buổi học

> File này lưu tóm tắt mỗi buổi học để AI có thể đọc lại và tiếp tục từ đúng chỗ đã dừng.
> Khi bắt đầu session mới, AI sẽ đọc file này trước tiên.

---

## Trạng thái hiện tại
- **Bài học tiếp theo**: Bài 5 - GPIO (tiếp theo sau Bài 4 hoàn thành)
- **Giai đoạn**: 1 - Nền tảng
- **Tổng số buổi học**: 3

---

## Lịch sử các buổi học

### Buổi 1 - 2026-03-23
- **Chủ đề**: Tổng quan AM335x SoC
- **Bài học số**: 1
- **Nội dung chính**:
	- Khái niệm SoC và vị trí của AM335x trên BeagleBone Black
	- Kiến trúc tổng quan Cortex-A8, interconnect, bộ nhớ và peripheral
	- Memory map và khái niệm base address + offset
	- So sánh bare-metal và Linux khi truy cập phần cứng
- **Kiến thức đã nắm**:
	- Peripheral trên AM335x là memory-mapped
	- Mỗi module có base address riêng
	- Công thức: địa_chỉ_thanh_ghi = base_address + offset
	- Ba thứ cần kiểm tra khi module không hoạt động: pin mode, clock, set value
	- Vì sao code phải theo khuôn khổ kernel trên Linux userspace
- **Câu hỏi kiểm tra**: ✅ Đã trả lời, nắm rõ cốt lõi bài
- **Bài tiếp theo**: Bài 2 - BeagleBone Black hardware overview

### Buổi 2 - 2026-03-24
- **Chủ đề**: BeagleBone Black Hardware Overview
- **Bài học số**: 2
- **Nội dung chính**:
	- Thành phần chính trên board: DDR3, eMMC, PMIC, Ethernet PHY, USB, oscillator, header P8/P9
	- Pinmux và vai trò của Control Module
	- Cách tra bảng P8/P9 để ánh xạ chân board sang GPIO module và bit tương ứng
	- Khái niệm GPIO module, thanh ghi 32-bit, và mối quan hệ giữa module, thanh ghi, bit và chân
- **Kiến thức đã nắm**:
	- BBB là một board hoàn chỉnh, không chỉ có AM335x
	- Pinmux quyết định chức năng thực tế của từng chân
	- Có thể tra P8/P9 header table để xác định GPIO module và bit
	- Hiểu GPIO1 là một khối chứa nhiều thanh ghi, không phải một thanh ghi đơn lẻ
	- Hiểu bit số 6 là vị trí bit trong thanh ghi data, không phải thanh ghi thứ 6
	- Hiểu mỗi GPIO module có vùng địa chỉ riêng và cùng một layout thanh ghi
- **Câu hỏi kiểm tra**: ✅ Đã trả lời tốt; câu PMIC được bổ sung trong phần giải thích
- **Bài tiếp theo**: Bài 3 - Linux cơ bản cho embedded

### Buổi 3 - 2026-03-26
- **Chủ đề**: Linux cơ bản cho Embedded
- **Bài học số**: 3
- **Nội dung chính**:
	- Hệ thống thư mục Linux embedded: `/dev`, `/sys`, `/proc`
	- Kết nối BBB qua serial/SSH và các lệnh shell nền tảng
	- Cơ chế sysfs GPIO và cách chuyển đổi `GPIOx_y` sang số GPIO Linux
	- Device Tree ở mức khái niệm: `.dts` -> `.dtb` -> U-Boot nạp -> kernel đọc
	- So sánh các cách truy cập phần cứng: sysfs, device file, mmap, kernel module
- **Kiến thức đã nắm**:
	- Xác định đúng công thức GPIO Linux: `module * 32 + bit` (ví dụ `GPIO2_3 = 67`)
	- Hiểu lỗi quyền ghi sysfs: `Permission denied` khi chưa có quyền root
	- Hiểu tình huống thực tế khi chỉ thấy `export`, `unexport`, `gpiochip*` trong `/sys/class/gpio`
	- Biết rằng thư mục `gpioN` chỉ xuất hiện sau khi export thành công
	- Hiểu đúng vai trò Device Tree: mô tả phần cứng cho kernel, thường do vendor/dev embedded tạo và duy trì
- **Thắc mắc/Lưu ý**:
	- Gặp lỗi `invalid GPIO 53` khi export, cần kiểm tra GPIO hợp lệ theo `gpiochip` base, pinmux và trạng thái driver/kernel
	- Có thể dùng interface LED (`/sys/class/leds/...`) để kiểm chứng nhanh trước khi đi sâu GPIO generic
- **Câu hỏi kiểm tra**: ✅ Đã trả lời tốt; cần chuẩn hóa thêm thuật ngữ ở `/dev/mem` và vai trò sysfs/driver
- **Bài tiếp theo**: Bài 4 - Memory-mapped I/O (`/dev/mem`, `mmap()`)

---

### Buổi 4 - 2026-03-31
- **Chủ đề**: Hoàn thiện nội dung bài 4 và mở rộng giải thích cho các bài tiếp theo
- **Bài học số**: 4 → 32 (tài liệu + ví dụ code)
- **Nội dung chính thực hiện hôm nay**:
	- Hoàn chỉnh và mở rộng phần giải thích chi tiết cho `bai_04_memory_mapped_io.md` (virtual memory, page tables, TRM lookup, `led_mmap.c` chi tiết từng dòng).
	- Thêm xử lý `SIGINT` vào `led_mmap.c` để cleanup (munmap/close) khi người dùng nhấn Ctrl+C.
	- Thay `sleep(0.5)` bằng `usleep(500000)` trong ví dụ để tránh truncation bug.
	- Tự động chèn phần **"Giải thích chi tiết từng dòng code"** (line-by-line) cho tất cả các bài từ **Bài 05** tới **Bài 32** nơi có ví dụ code.
	- Cập nhật nhiều file markdown bài học: bài 05 → bài 32 (thêm các mục a), b), c) giải thích các include, macro, pointer arithmetic, register access, copy_to_user etc.).
	- Thêm, chỉnh sửa các ví dụ C nhỏ (ví dụ `led_mmap.c` đã mở rộng điều khiển 4 LED), và sửa các Makefile/recipe mô tả nơi cần.

- **Tổng quan kết quả**:
	- Đã hoàn thành chèn giải thích chi tiết từng dòng cho mọi bài có ví dụ code (Bài 05 → Bài 32).
	- `notes/session_log.md` được cập nhật để lần tới AI/nhóm biết chính xác điểm dừng và file đã chỉnh sửa.

- **Bước tiếp theo đề xuất**:
	1. Kiểm tra git status, commit các thay đổi với message súc tích.
	2. Đẩy (push) lên remote `origin` (cần quyền/SSH credential trên máy của bạn).
	3. Nếu muốn, tôi có thể tạo PR (nếu repo có branch riêng) hoặc mở issue tóm tắt thay đổi.

### Buổi 5 - 2026-04-07
- **Chủ đề**: GPIO — Control Module, Pad Config và Toggle LED
- **Bài học số**: 5
- **Nội dung chính**:
	- Thực hành ánh xạ `mmap()` cho `GPIO1`, `Control Module`, `CM_PER` và blink LED USR0 (GPIO1_21).
	- Hiểu thứ tự cần thiết: enable clock → pinmux → set direction → set/clear data.
	- Trả lời quiz mục 8 trong `bai_05_gpio.md` và đã nhận xét, giải thích nhanh.
- **Kiến thức đã nắm**:
	- Cấu trúc thanh ghi GPIO (`GPIO_OE`, `GPIO_DATAIN`, `GPIO_SETDATAOUT`, `GPIO_CLEARDATAOUT`).
	- Pinmux `conf_gpmc_a5` cho LED USR0 và cách tính Linux GPIO number (module*32 + bit).
	- Tác dụng của clock module (CM_PER_GPIO1_CLKCTRL) và ảnh hưởng khi không enable clock.
- **Files cập nhật**:
	- lessons/bai_05_gpio/bai_05_gpio.md
	- lessons/bai_05_gpio/README.md
	- notes/session_log.md (mục này)
- **Ghi chú**: Quiz Bài 05 đã được thu thập và đánh giá nhanh; sẵn sàng qua Bài 06.

### Buổi 6 - 2026-04-08
- **Chủ đề**: Clock Module (CM) — PRCM, Clock Gating và Enabling Modules
- **Bài học số**: 6
- **Nội dung chính**:
	- Hoàn chỉnh và mở rộng `lessons/bai_06_clock_module/bai_06_clock_module.md`.
	- Thêm mục **Ghi chú thực hành** tóm tắt: lý do chọn `PRCM_SIZE = 0x2000`, cách bật clock qua `CLKCTRL`, ý nghĩa `IDLEST` và `IDLEST_MASK`, vai trò `volatile` cho MMIO, khác biệt `CM_PER` vs `CM_WKUP`, và `OPTFCLKEN` cho `GPIO1`.
	- Nhấn mạnh best practice: poll `IDLEST` với timeout sau khi bật clock trực tiếp.
- **Files cập nhật**:
	- lessons/bai_06_clock_module/bai_06_clock_module.md
- **Ghi chú**: Đã thêm tóm tắt thực hành; có thể bổ sung tham chiếu TRM (số trang) nếu cần.

### Cập nhật nhỏ — 2026-04-01
- **Hành động**: Kiểm tra và hiệu chỉnh đáp án phần "Câu hỏi kiểm tra" của Bài 4; chèn phần **Đáp án (đã hiệu chỉnh và tóm tắt)** vào file bài học.
- **Files**: lessons/bai_04_memory_mapped_io/bai_04_memory_mapped_io.md
- **Ghi chú**: Tóm tắt sửa các điểm: cơ chế virtual memory, tham số `mmap()`, ý nghĩa `volatile`, trình tự ánh xạ và điều khiển GPIO, và lý do mmap nhanh hơn sysfs.

<!--
FORMAT cho mỗi buổi:
### Buổi X - [Ngày]
- **Chủ đề**: ...
- **Bài học số**: ...
- **Nội dung chính**: ...
- **Kiến thức đã nắm**: ...
- **Thắc mắc/Lưu ý**: ...
- **Bài tiếp theo**: ...
-->
