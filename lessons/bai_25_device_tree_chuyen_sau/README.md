# Bài 25 - Device Tree Chuyên Sâu: Tham Khảo Nhanh

## Cells quy ước
```
#address-cells = <1>  + #size-cells = <1>  → reg = <addr size>     (SoC peripherals)
#address-cells = <1>  + #size-cells = <0>  → reg = <addr>           (I2C, SPI slaves)
#interrupt-cells = <1>                     → interrupts = <irq_num>
#interrupt-cells = <2>                     → interrupts = <pin type>
#clock-cells = <2>                         → clocks = <&clkctrl offset type>
```

## gpio-leds (tự động create /sys/class/leds/)
```dts
leds { compatible = "gpio-leds";
    myled { label = "board:color:function";
            gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
            linux,default-trigger = "heartbeat"; }; };
```

## gpio-keys (tự động tạo input event)
```dts
buttons { compatible = "gpio-keys";
    btn0 { gpios = <&gpio1 14 GPIO_ACTIVE_LOW>;
           linux,code = <KEY_ENTER>; }; };
```

## Apply overlay via configfs
```bash
mkdir /sys/kernel/config/device-tree/overlays/NAME
cat overlay.dtbo > /sys/kernel/config/device-tree/overlays/NAME/dtbo
echo 1 > /sys/kernel/config/device-tree/overlays/NAME/status
# Remove:
rmdir /sys/kernel/config/device-tree/overlays/NAME
```

## Debug DT
```bash
dtc -I dtb -O dts /boot/dtbs/$(uname -r)/*.dtb -o /tmp/cur.dts
grep -r "compatible" /proc/device-tree/ | head -20
cat /proc/device-tree/ocp/i2c@4802a000/tmp102@48/compatible
```

## References
- [bai_25_device_tree_chuyen_sau.md](bai_25_device_tree_chuyen_sau.md)
- spruh73q.pdf Chapter 9 (pinmux offsets)
