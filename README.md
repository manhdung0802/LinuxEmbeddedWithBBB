# 📘 Ghi Chú Kiến Thức - AM335x & Embedded Linux

> File này được cập nhật sau mỗi buổi học để ôn tập lại kiến thức.
> Mỗi phần sẽ tóm tắt: khái niệm, thanh ghi quan trọng, câu lệnh Linux, code mẫu.

---

## 📋 Mục lục
- [Giai đoạn 1: Nền tảng](#giai-đoạn-1-nền-tảng)
- [Giai đoạn 2: Ngoại vi cơ bản](#giai-đoạn-2-ngoại-vi-cơ-bản)
- [Giai đoạn 3: Ngoại vi nâng cao](#giai-đoạn-3-ngoại-vi-nâng-cao)
- [Giai đoạn 4: Hệ thống](#giai-đoạn-4-hệ-thống)
- [Câu lệnh Linux thường dùng](#câu-lệnh-linux-thường-dùng)
- [Địa chỉ thanh ghi quan trọng](#địa-chỉ-thanh-ghi-quan-trọng)

---

## Giai đoạn 1: Nền tảng

### Bài 1 - Tổng quan AM335x SoC

- AM335x là một SoC: bên trong có CPU Cortex-A8, interconnect, bộ nhớ, clock/reset, interrupt và nhiều peripheral.
- Peripheral được điều khiển bằng memory-mapped I/O: CPU đọc/ghi vào địa chỉ nhớ đặc biệt để tác động phần cứng.
- Công thức cơ bản khi làm việc với thanh ghi:

```text
register_address = base_address + offset
```

- Khi debug một peripheral không hoạt động, luôn kiểm tra theo thứ tự:
	1. Clock của module
	2. Pinmux trong Control Module
	3. Đúng base address, offset và bit-field

- So sánh nhanh:
	- Bare-metal: truy cập trực tiếp phần cứng, timing tốt hơn, nhưng phải tự xử lý nhiều thứ.
	- Linux: có kernel quản lý driver và virtual memory, thuận tiện hơn cho hệ thống phức tạp.

---

## Giai đoạn 2: Ngoại vi cơ bản

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 3: Ngoại vi nâng cao

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 4: Hệ thống

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Câu lệnh Linux thường dùng

| Lệnh | Mô tả |
|-------|--------|
| *(sẽ cập nhật)* | |

---

## Địa chỉ thanh ghi quan trọng

| Module | Thanh ghi | Địa chỉ | Mô tả |
|--------|-----------|----------|--------|
| CM_PER | Base | 0x44E00000 | Clock module cho nhiều peripheral |
| GPIO0 | Base | 0x44E07000 | GPIO bank 0 |
| UART0 | Base | 0x44E09000 | UART console chính trên BBB |
| CONTROL MODULE | Base | 0x44E10000 | Pinmux và pad control |
| GPIO1 | Base | 0x4804C000 | GPIO bank 1 |

---

> 💡 **Tip**: Luôn mở file `BBB_docs/datasheets/spruh73q.pdf` (AM335x TRM) khi học để tra cứu thanh ghi.
