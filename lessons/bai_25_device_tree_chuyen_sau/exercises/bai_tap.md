# Bài Tập - Bài 25: Device Tree Chuyên Sâu

## Bài tập 1: gpio-leds via Device Tree

Thêm overlay để sử dụng `gpio-leds` driver cho LED USR0-USR3 của BBB:

**BBB built-in USR LEDs**:
- USR0 = GPIO1_21
- USR1 = GPIO1_22
- USR2 = GPIO1_23
- USR3 = GPIO1_24

Viết overlay active các LED với triggers:
- USR0: `heartbeat`
- USR1: `mmc0` (blink khi SD card hoạt động)
- USR2: `cpu0` (blink theo CPU load)
- USR3: `none`

Sau khi apply:
```bash
ls /sys/class/leds/
cat /sys/class/leds/beaglebone:green:usr3/trigger
echo 1 > /sys/class/leds/beaglebone:green:usr3/brightness  # bật LED3
```

---

## Bài tập 2: Custom DTS Binding

Viết DTS binding cho driver `myled` từ bài 19 theo chuẩn YAML:

1. Tạo file `mycompany,myled.yaml` với:
   - `compatible`: `mycompany,myled`
   - `reg`: required, địa chỉ GPIO bank
   - `my-gpio-pin`: required, u32, range 0-31
   - `led-label`: optional, string

2. Viết DTS overlay dùng node này

3. Verify bằng `dtc -W`:
```bash
dtc -I dts -O dtb -o out.dtb \
    -W no-unit_address_vs_reg \
    my_overlay.dts
# Không được có warning hay error
```

---

## Bài tập 3: Phân tích BBB DTS

Xem DTS của BBB hiện tại và trả lời:

```bash
dtc -I dtb -O dts /boot/dtbs/$(uname -r)/am335x-boneblack.dtb -o /tmp/bbb.dts 2>/dev/null
```

**Câu hỏi**:
1. Node `uart0` có `status = "okay"` hay `"disabled"`? Tại sao?
2. I2C2 dùng để làm gì? (xem compatible của các child nodes)
3. `pinctrl-0` của node `mmc1` trỏ đến label nào?
4. Có bao nhiêu GPIO bank được enabled?
5. Node nào có `interrupt-controller` và `#interrupt-cells = <2>`?
