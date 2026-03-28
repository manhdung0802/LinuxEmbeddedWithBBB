# Bài 27 - Tổng Quan Yocto Project

## Kiến trúc

```
Poky = BitBake + OE-Core + meta-poky
  + meta-ti           (TI AM335x BSP)
  + meta-openembedded (community packages)
  + meta-bbb-custom   (layer của bạn)
```

## Khái niệm cốt lõi

| Khái niệm | File | Ý nghĩa |
|----------|------|---------|
| Recipe | `*.bb` | Công thức build 1 package |
| bbappend | `*.bbappend` | Override recipe có sẵn |
| Layer | `conf/layer.conf` | Nhóm recipes theo chủ đề |
| Image | `*-image.bb` | Recipe tạo rootfs |
| Machine | `conf/machine/*.conf` | Mô tả phần cứng |
| DISTRO | `conf/distro/*.conf` | Cấu hình distribution |

## BitBake task pipeline

```
do_fetch → do_unpack → do_patch → do_configure
→ do_compile → do_install → do_package → do_image
```

## Lệnh BitBake hay dùng

```bash
bitbake <recipe>                    # Build đầy đủ
bitbake <recipe> -c <task>          # Chạy task cụ thể
bitbake <recipe> -c clean           # Clean artifacts
bitbake <recipe> -c compile -f      # Force compile
bitbake <recipe> -c devshell        # Debug shell
bitbake <recipe> -c listtasks       # Xem tất cả tasks
bitbake-layers show-layers          # Liệt kê layers
bitbake -e <recipe> | grep ^VAR     # Xem giá trị biến
```

## Release LTS khuyến nghị: Kirkstone (4.0 LTS)

## Build output: `tmp/deploy/images/beaglebone/`
- `*.wic` — SD card image (all-in-one)
- `zImage` — Kernel
- `*.dtb` — Device tree
- `MLO` + `u-boot.img` — Bootloader
