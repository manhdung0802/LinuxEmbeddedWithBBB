# 📝 Session Log - Lịch sử buổi học

> File này lưu tóm tắt mỗi buổi học để AI có thể đọc lại và tiếp tục từ đúng chỗ đã dừng.
> Khi bắt đầu session mới, AI sẽ đọc file này trước tiên.

---

## Trạng thái hiện tại
- **Bài học tiếp theo**: Bài 3 - Thiết lập môi trường phát triển (cross-compile, kernel source, build)
- **Giai đoạn**: 1 - Nền tảng
- **Tổng số buổi học**: 2
- **Lộ trình**: Linux Device Driver cho AM335x (chuyển từ register-level sang driver-focused từ bài 3)

---

## Lịch sử các buổi học

### Buổi 1 - 2026-03-23
- **Chủ đề**: Tổng quan AM335x SoC
- **Bài học số**: 1
- **Nội dung chính**:
- Khái niệm SoC và vị trí của AM335x trên BeagleBone Black
- Kiến trúc tổng quan Cortex-A8, interconnect, bộ nhớ và peripheral
- Memory map và khái niệm base address + offset
- **Kiến thức đã nắm**:
- Peripheral trên AM335x là memory-mapped
- Mỗi module có base address riêng
- Công thức: register_address = base_address + offset
- Ba thứ cần kiểm tra khi module không hoạt động: pin mode, clock, set value
- **Bài tiếp theo**: Bài 2

### Buổi 2 - 2026-03-24
- **Chủ đề**: BeagleBone Black Hardware Overview
- **Bài học số**: 2
- **Nội dung chính**:
- Thành phần chính trên board: DDR3, eMMC, PMIC, Ethernet PHY, USB, oscillator, header P8/P9
- Pinmux và vai trò của Control Module
- Cách tra bảng P8/P9 để ánh xạ chân board sang GPIO module và bit tương ứng
- **Kiến thức đã nắm**:
- BBB là một board hoàn chỉnh, không chỉ có AM335x
- Pinmux quyết định chức năng thực tế của từng chân
- Mỗi GPIO module có vùng địa chỉ riêng và cùng một layout thanh ghi
- **Bài tiếp theo**: Bài 3 - Thiết lập môi trường phát triển

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
