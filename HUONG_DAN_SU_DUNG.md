# 📖 Hướng Dẫn Sử Dụng Agent "BBB Linux Device Driver Tutor"

## Agent này là gì?
Đây là một agent tùy chỉnh trong VS Code Copilot, được thiết kế để **dạy viết Linux Device Driver cho AM335x (BeagleBone Black)** theo lộ trình từ dễ đến khó.

## Khi nào dùng agent này?
- Khi bạn muốn **học bài mới** trong lộ trình device driver
- Khi cần **giải thích kernel API**, subsystem architecture, data flow trong kernel
- Khi muốn hiểu **cách viết driver** (kernel module, char device, platform driver, bus driver...)
- Khi cần tra cứu **thanh ghi AM335x** từ TRM để viết driver truy cập phần cứng
- Khi muốn viết **Device Tree binding** cho driver
- Khi muốn **build, load và test driver** trên BeagleBone Black

## Cách sử dụng

### Bắt đầu buổi học mới
Chọn agent **"BBB Embedded Linux Tutor"** trong Copilot Chat, rồi gõ:
```
Tiếp tục bài học
```
Agent sẽ tự đọc `notes/session_log.md` để biết bạn đã học tới đâu và dạy bài tiếp theo.

### Hỏi thắc mắc
```
Giải thích probe() trong platform driver hoạt động thế nào?
Tại sao phải dùng devm_ioremap thay vì ioremap?
Cho tôi ví dụ viết GPIO driver đơn giản
Cách viết Device Tree binding cho driver tự viết?
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
├── .claude/agents/bbb.agent.md ← Cấu hình agent (không cần sửa)
├── README.md              ← 📘 Ghi chú kiến thức để ôn tập
├── HUONG_DAN_SU_DUNG.md   ← 📖 File này - hướng dẫn sử dụng
├── BBB_docs/              ← Tài liệu gốc
│   ├── datasheets/        ← TRM (spruh73q.pdf), SRM, pin tables
│   └── schematics/        ← Sơ đồ nguyên lý BBB
├── lessons/               ← Nội dung chi tiết từng bài học
├── projects/              ← Driver projects thực hành
├── scripts/               ← Build scripts cho kernel module
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
- Tập trung vào **Linux kernel device driver** cho AM335x
- Kiến thức phần cứng (thanh ghi, pinmux) được dạy trong ngữ cảnh driver kernel, không phải bare-metal
- Mỗi bài đi **từng bước**, có thể hỏi bất cứ lúc nào
- Sau mỗi buổi, kiến thức tự động được ghi vào `README.md` và `session_log.md`
- Code driver sử dụng **ngôn ngữ C**, tuân thủ Linux kernel coding style
- Mỗi driver ví dụ kèm **Makefile** và **Device Tree snippet** (nếu cần)
