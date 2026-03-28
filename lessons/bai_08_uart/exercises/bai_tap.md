# Bài tập - Bài 8 UART

1. Tính divisor cho baud rate 57600 và viết code cấu hình tương ứng
2. Viết hàm `uart_getline(buf, maxlen)` đọc chuỗi kết thúc bằng `\n`
3. Thêm timeout cho `uart_getc()`: nếu sau 1s không nhận được dữ liệu thì trả về -1
