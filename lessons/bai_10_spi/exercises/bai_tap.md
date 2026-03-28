# Bài tập - Bài 10 SPI

1. Viết code đọc ADC MCP3204 liên tục mỗi 100ms, in kết quả dạng bảng cho cả 4 channel
2. Dùng `/dev/spidev0.0` thay vì mmap để đọc MCP3204 (cần device tree overlay)
3. Viết code ghi giá trị ra DAC MCP4821 (12-bit, 1-channel, SPI Mode 0):
   - Byte 1: `0x10 | (val >> 8)` (bit 12..8)
   - Byte 2: `val & 0xFF` (bit 7..0)
