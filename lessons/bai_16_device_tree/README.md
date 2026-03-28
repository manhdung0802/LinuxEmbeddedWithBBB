# Bài 16 - Device Tree: Tham Khảo Nhanh

## Cấu trúc DTS
```
/dts-v1/;
/ {
    node-name@address {
        compatible = "vendor,device";
        reg = <base_addr size>;
        status = "okay" | "disabled";
    };
};
```

## Lệnh dtc
```bash
dtc -O dtb -o output.dtb input.dts          # DTS → DTB
dtc -I dtb -O dts -o output.dts input.dtb   # DTB → DTS (decompile)
```

## Apply Overlay
```bash
config-pin P9.17 i2c                         # set pinmux via config-pin
echo BBOverlay:00A0 > /sys/devices/platform/bone_capemgr/slots
```

## Kiểm tra DT đang chạy
```bash
ls /proc/device-tree/
fdtdump /boot/dtbs/$(uname -r)/am335x-boneblack.dtb | less
```

## References
- [bai_16_device_tree.md](bai_16_device_tree.md)
- spruh73q.pdf, Table 9-7 (pinmux offsets)
