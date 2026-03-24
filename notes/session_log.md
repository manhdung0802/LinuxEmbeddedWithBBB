# 📝 Session Log - Lịch sử buổi học

> File này lưu tóm tắt mỗi buổi học để AI có thể đọc lại và tiếp tục từ đúng chỗ đã dừng.
> Khi bắt đầu session mới, AI sẽ đọc file này trước tiên.

---

## Trạng thái hiện tại
- **Bài học tiếp theo**: Bài 3 - Linux cơ bản cho embedded
- **Giai đoạn**: 1 - Nền tảng
- **Tổng số buổi học**: 2

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
