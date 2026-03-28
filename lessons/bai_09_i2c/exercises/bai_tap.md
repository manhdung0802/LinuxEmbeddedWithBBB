# Bài tập - Bài 9 I2C

1. Dùng `i2cdetect` để scan I2C bus 0 và giải thích kết quả (device nào ở địa chỉ nào)
2. Đọc 256 byte từ EEPROM 24C32 (addr 0x50, register pointer là địa chỉ 16-bit)
3. Viết hàm `i2c_write_reg(slave, reg, data)` để ghi 1 byte vào register của slave
