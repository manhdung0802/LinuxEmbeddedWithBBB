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

- **Các file đã thay đổi (chính)**:
	- lessons/bai_04_memory_mapped_io/bai_04_memory_mapped_io.md
	- lessons/bai_04_memory_mapped_io/examples/led_mmap.c
	- lessons/bai_05_gpio/bai_05_gpio.md
	- lessons/bai_06_clock_module/bai_06_clock_module.md
	- lessons/bai_07_gpio_nang_cao/bai_07_gpio_nang_cao.md
	- lessons/bai_08_uart/bai_08_uart.md
	- lessons/bai_09_i2c/bai_09_i2c.md
	- lessons/bai_10_spi/bai_10_spi.md
	- lessons/bai_11_timer_pwm/bai_11_timer_pwm.md
	- lessons/bai_12_adc/bai_12_adc.md
	- lessons/bai_13_interrupt_controller/bai_13_interrupt_controller.md
	- lessons/bai_14_dma_edma/bai_14_dma_edma.md
	- lessons/bai_15_pru_icss/bai_15_pru_icss.md
	- lessons/bai_16_device_tree/bai_16_device_tree.md
	- lessons/bai_17_kernel_module/bai_17_kernel_module.md
	- lessons/bai_18_char_device_driver/bai_18_char_device_driver.md
	- lessons/bai_19_platform_driver/bai_19_platform_driver.md
	- lessons/bai_20_debug_techniques/bai_20_debug_techniques.md
	- lessons/bai_21_process_management/bai_21_process_management.md
	- lessons/bai_22_multithreading/bai_22_multithreading.md
	- lessons/bai_23_file_io_nang_cao/bai_23_file_io_nang_cao.md
	- lessons/bai_24_event_handling/bai_24_event_handling.md
	- lessons/bai_25_device_tree_chuyen_sau/bai_25_device_tree_chuyen_sau.md
	- lessons/bai_26_watchdog_timer/bai_26_watchdog_timer.md
	- lessons/bai_27_yocto_overview/bai_27_yocto_overview.md
	- lessons/bai_28_yocto_setup/bai_28_yocto_setup.md
	- lessons/bai_29_yocto_recipe/bai_29_yocto_recipe.md
	- lessons/bai_30_custom_bbb_image/bai_30_custom_bbb_image.md
	- lessons/bai_31_bsp_layer_rieng/bai_31_bsp_layer_rieng.md
	- lessons/bai_32_tich_hop_driver_ung_dung/bai_32_tich_hop_driver_ung_dung.md

- **Tổng quan kết quả**:
	- Đã hoàn thành chèn giải thích chi tiết từng dòng cho mọi bài có ví dụ code (Bài 05 → Bài 32).
	- `notes/session_log.md` được cập nhật để lần tới AI/nhóm biết chính xác điểm dừng và file đã chỉnh sửa.

- **Bước tiếp theo đề xuất**:
	1. Kiểm tra git status, commit các thay đổi với message súc tích.
	2. Đẩy (push) lên remote `origin` (cần quyền/SSH credential trên máy của bạn).
	3. Nếu muốn, tôi có thể tạo PR (nếu repo có branch riêng) hoặc mở issue tóm tắt thay đổi.
