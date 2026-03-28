# Bài 29 - Viết Yocto Recipe

## Recipe tối giản

```bitbake
SUMMARY = "My app"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://myapp.c"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} myapp.c -o myapp
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 myapp ${D}${bindir}/myapp
}
```

## Recipe + Systemd service

```bitbake
inherit systemd
SYSTEMD_SERVICE:${PN} = "myapp.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 myapp ${D}${bindir}/myapp
    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/myapp.service \
        ${D}${systemd_unitdir}/system/myapp.service
}
```

## bbappend

```bitbake
# openssh_%.bbappend
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append = " file://my-config.conf"

do_install:append() {
    install -m 0644 ${WORKDIR}/my-config.conf \
        ${D}${sysconfdir}/my-config.conf
}
```

## devtool workflow

```bash
devtool add myapp /path/to/source          # Tạo recipe
devtool build myapp                         # Build
devtool deploy-target myapp root@BBB_IP     # Deploy lên board
devtool undeploy-target myapp root@BBB_IP   # Undeploy
devtool finish myapp meta-bbb-custom        # Finalize vào layer
```

## Tìm recipe/package

```bash
bitbake -s | grep <name>                   # Tìm recipe
oe-pkgdata-util find-path /usr/lib/lib.so  # Tìm package có file
bitbake <recipe> -c listtasks              # Liệt kê tasks
bitbake <recipe> -c devshell               # Debug shell
```
