# Bài tập - Bài 13 Interrupt Controller

1. Viết kernel module đếm số lần nhấn nút bằng interrupt và lưu vào `/proc/gpio_count`
2. Thêm workqueue vào module: bottom half in thời gian (jiffies) mỗi khi có interrupt
3. Thử đăng ký cùng IRQ hai lần — điều gì xảy ra? Giải thích error message
