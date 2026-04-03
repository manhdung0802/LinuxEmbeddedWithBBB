## 13. Pin Conflicts

### 13.1 Pins bị chiếm bởi eMMC (nếu boot từ eMMC)

Nếu BBB boot từ eMMC, các chân sau **KHÔNG** dùng được as GPIO:

| P8 Pins | GPIO | Lý do |
|---------|------|-------|
| P8.3–P8.6 | GPIO1_6 – GPIO1_9 | gpmc_ad6-9 (eMMC data) |
| P8.20–P8.25 | GPIO1_31, GPIO1_26, ... | gpmc_csn (eMMC CS) |

### 13.2 Pins bị chiếm bởi HDMI Cape

| P8 Pins | GPIO | Lý do |
|---------|------|-------|
| P8.27–P8.46 | GPIO2_x range | LCD data/ctrl (HDMI framer) |

> Tắt HDMI bằng DT: xóa/vô hiệu hóa `hdmi` node trong DTS hoặc dùng `/boot/uEnv.txt`: `dtb=am335x-boneblack-emmc-overlay.dtb`

### 13.3 Pins Xung đột Hai Chức Năng

| Pin | Chức năng 1 | Chức năng 2 | Giải pháp |
|-----|------------|------------|---------|
| P9.17 | SPI0_CS0 | I2C1_SCL | Chọn 1; dùng pinmux |
| P9.18 | SPI0_MOSI | I2C1_SDA | Chọn 1; dùng pinmux |
| P9.21 | SPI0_MISO | UART2_TXD | Chọn 1 |
| P9.22 | SPI0_CLK | UART2_RXD | Chọn 1 |
| P9.28 | SPI1_CS0 | ECAP2 | Chọn 1 |
| P9.42 | UART3_TX | SPI1_CS1 | Chọn 1 |

---