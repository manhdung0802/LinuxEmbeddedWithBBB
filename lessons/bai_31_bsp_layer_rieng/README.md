# Bài 31 - BSP Layer Riêng (meta-bbb-custom)

## Tạo layer nhanh

```bash
# Cách nhanh nhất dùng bitbake-layers
cd ~/yocto-bbb/build-bbb
bitbake-layers create-layer ~/yocto-bbb/meta-bbb-custom
bitbake-layers add-layer ~/yocto-bbb/meta-bbb-custom
```

## Cấu trúc layer.conf

```bitbake
BBPATH .= ":${LAYERDIR}"
BBFILES += "${LAYERDIR}/recipes-*/*/*.bb \
             ${LAYERDIR}/recipes-*/*/*.bbappend"
BBFILE_COLLECTIONS += "bbb-custom"
BBFILE_PATTERN_bbb-custom = "^${LAYERDIR}/"
BBFILE_PRIORITY_bbb-custom = "10"
LAYERVERSION_bbb-custom = "1"
LAYERSERIES_COMPAT_bbb-custom = "kirkstone scarthgap"
LAYERDEPENDS_bbb-custom = "core openembedded-layer meta-ti-bsp"
```

## Cấu trúc thư mục chuẩn

```
meta-bbb-custom/
├── conf/
│   ├── layer.conf
│   └── machine/
│       └── bbb-custom.conf     ← Custom machine
├── recipes-bsp/
│   └── u-boot/
│       └── u-boot-ti-staging_%.bbappend
├── recipes-kernel/
│   └── linux/
│       ├── linux-ti_%.bbappend
│       └── files/
│           └── bbb-custom.cfg  ← Kernel fragment
└── recipes-bbb/
    ├── images/
    │   └── my-bbb-image.bb
    └── myapp/
        └── myapp_1.0.bb
```

## Kiểm tra layer

```bash
bitbake-layers show-layers       # Hiển thị tất cả layers
bitbake-layers show-recipes "*" | grep myrecipe  # Tìm recipe
bitbake -e | grep "^MACHINE ="   # Kiểm tra machine
bitbake -e linux-ti | grep "^SRC_URI"  # Xem SRC_URI sau merge
```
