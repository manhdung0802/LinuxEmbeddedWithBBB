# Bài Tập - Bài 16: Device Tree

## Bài tập 1: Đọc DTS của BBB

Trên BBB đang chạy, hãy dùng các lệnh sau để khám phá Device Tree hiện tại:

```bash
# 1. Liệt kê các node gốc
ls /proc/device-tree/

# 2. Đọc model của board
cat /proc/device-tree/model

# 3. Liệt kê các node memory-mapped peripheral
ls /proc/device-tree/ocp/

# 4. Xem status của node uart1
cat /proc/device-tree/ocp/serial@48022000/status
```

**Yêu cầu**: Liệt kê 5 node peripheral bạn tìm thấy trong `/proc/device-tree/ocp/` và nhận xét `status` của các node đó.

---

## Bài tập 2: Viết Overlay bật UART4

Viết một Device Tree Overlay để bật UART4 trên BBB với:
- P9.13 = UART4_TX (mode 6)
- P9.11 = UART4_RX (mode 6)
- Pinmux offset P9.13: `0x074`, P9.11: `0x070`

**Template overlay:**
```dts
/dts-v1/;
/plugin/;
/ {
    compatible = "ti,beaglebone-black";
    part-number = "BBB-UART4";
    version = "00A0";

    fragment@0 {
        target = <&am33xx_pinmux>;
        __overlay__ {
            /* TODO: thêm pinmux config */
        };
    };

    fragment@1 {
        target = <&uart4>;
        __overlay__ {
            /* TODO: enable uart4 */
        };
    };
};
```

**Yêu cầu**: Hoàn thiện overlay trên. Explain tại sao dùng `PIN_OUTPUT` cho TX và `PIN_INPUT_PULLUP` cho RX.

---

## Bài tập 3: Debug Device Tree Overlap

Bạn có một overlay muốn dùng P9.21 và P9.22 cho SPI0. Nhưng sau khi apply, lệnh `i2cdetect -y 0` không còn thấy thiết bị I2C.

**Phân tích**:
1. Kiểm tra P9.21 và P9.22 map đến peripheral nào (xem BBB_docs header table)
2. Tại sao SPI0 overlay có thể conflict với I2C0?
3. Bạn sẽ làm gì để dùng cả SPI0 và I2C0 cùng lúc?

**Hint**: 
```bash
# Xem chân P9.21, P9.22 đang config như nào
config-pin -q P9.21
config-pin -q P9.22
```
