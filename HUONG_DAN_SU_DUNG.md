# 📖 Hướng Dẫn Sử Dụng Agent "BBB Embedded Linux Tutor"

## Agent này là gì?
Đây là một agent tùy chỉnh trong VS Code Copilot, được thiết kế để **dạy lập trình embedded AM335x (BeagleBone Black)** kết hợp **lập trình Linux** theo lộ trình từ dễ đến khó.

## Khi nào dùng agent này?
- Khi bạn muốn **học bài mới** trong lộ trình AM335x
- Khi cần **giải thích thanh ghi**, cách config phần cứng
- Khi muốn hiểu **code embedded Linux** (mmap, /dev/mem, driver, ...)
- Khi cần tra cứu thông tin từ **AM335x TRM** hoặc **BBB schematic**
- Khi muốn viết **code thực hành** chạy trên BeagleBone Black

## Cách sử dụng

### Bắt đầu buổi học mới
Chọn agent **"BBB Embedded Linux Tutor"** trong Copilot Chat, rồi gõ:
```
Tiếp tục bài học
```
Agent sẽ tự đọc `notes/session_log.md` để biết bạn đã học tới đâu và dạy bài tiếp theo.

### Hỏi thắc mắc
```
Giải thích thanh ghi GPIO_OE là gì?
Tại sao phải set bit 18 trong CM_PER?
Cho tôi ví dụ code blink LED bằng mmap
```

### Ôn bài
```
Tóm tắt lại bài hôm qua
Cho tôi câu hỏi kiểm tra về GPIO
```

### Xem lộ trình
```
Lộ trình học hiện tại tôi đang ở đâu?
Còn bao nhiêu bài nữa trong giai đoạn 1?
```

## Cấu trúc thư mục

```
LinuxEmbedded/
├── .agent.md              ← Cấu hình agent (không cần sửa)
├── README.md              ← 📘 Ghi chú kiến thức để ôn tập
├── HUONG_DAN_SU_DUNG.md   ← 📖 File này - hướng dẫn sử dụng
├── BBB_docs/              ← Tài liệu gốc
│   ├── datasheets/        ← TRM (spruh73q.pdf), SRM, pin tables
│   └── schematics/        ← Sơ đồ nguyên lý BBB
├── lessons/               ← Nội dung chi tiết từng bài học
├── projects/              ← Code thực hành, bài tập
└── notes/                 ← Ghi chú & lịch sử
    └── session_log.md     ← 📝 Tóm tắt buổi học (AI đọc file này)
```

## Tài liệu tham khảo có sẵn
| File | Nội dung |
|------|----------|
| `spruh73q.pdf` | AM335x Technical Reference Manual - **tài liệu chính** |
| `BBB_SRM_C.pdf` | BeagleBone Black System Reference Manual |
| `BBB_SCH.pdf` | Sơ đồ nguyên lý BeagleBone Black |
| `beaglebone-black.pdf` | Tổng quan về board |
| `BeagleboneBlackP8HeaderTable.pdf` | Bảng pin header P8 |
| `BeagleboneBlackP9HeaderTable.pdf` | Bảng pin header P9 |

## Lưu ý
- Agent dạy bằng **tiếng Việt**
- Tập trung vào **register-level** programming, hạn chế thư viện
- Mỗi bài đi **từng bước**, có thể hỏi bất cứ lúc nào
- Sau mỗi buổi, kiến thức tự động được ghi vào `README.md` và `session_log.md`
- Code ví dụ sử dụng **ngôn ngữ C** là chính
