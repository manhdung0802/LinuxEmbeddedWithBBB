# Bài tập - Bài 27: Tổng Quan Yocto Project

## Bài tập 1: Sơ đồ kiến trúc (Conceptual)

Vẽ và giải thích sơ đồ mối quan hệ giữa các thành phần:
`BitBake`, `OE-Core`, `Poky`, `meta-ti`, `meta-openembedded`, `meta-bbb-custom`

**Các câu hỏi cụ thể:**
1. Thành phần nào chịu trách nhiệm tải code từ GitHub về?
2. Nếu bạn muốn thêm `nginx` vào image BBB, bạn tìm recipe đó ở layer nào?
3. Khi nâng lên Scarthgap (4.4), bạn cần thay đổi gì trong `bblayers.conf`?

---

## Bài tập 2: Phân tích recipe

Cho recipe dưới đây, hãy trả lời các câu hỏi:

```bitbake
SUMMARY = "I2C temperature sensor reader for BBB"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

SRC_URI = "git://github.com/example/temp-reader.git;protocol=https;branch=main"
SRCREV = "d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3"

S = "${WORKDIR}/git"

DEPENDS = "libi2c"
RDEPENDS:${PN} = "libi2c python3-smbus"

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${bindir}
    install -d ${D}${sysconfdir}/temp-reader
    install -m 0755 temp-reader ${D}${bindir}/temp-reader
    install -m 0644 temp-reader.conf ${D}${sysconfdir}/temp-reader/
}
```

**Câu hỏi:**
1. Tại sao `SRC_URI` dùng `git://` và có `SRCREV`? Lợi ích so với dùng URL tarball?
2. `S = "${WORKDIR}/git"` thay vì `S = "${WORKDIR}"` — vì sao?
3. `DEPENDS = "libi2c"` vs `RDEPENDS:${PN} = "libi2c python3-smbus"` — giải thích sự khác nhau
4. File `temp-reader.conf` sau khi install sẽ nằm ở đường dẫn nào trong rootfs?

---

## Bài tập 3: So sánh Yocto vs Buildroot cho BBB

Bạn đang làm một thiết bị IoT dùng BBB với các yêu cầu sau:
- Chạy Python 3 + một số thư viện (numpy, requests)
- Kết nối WiFi (USB dongle)
- Auto-update firmware qua internet
- Cần maintain 3 năm, có security patches
- Team 4 developer, 2 board models khác nhau

**Phân tích:**
1. Hãy liệt kê 3 lý do chọn Yocto thay Buildroot cho dự án này
2. Hãy liệt kê 2 nhược điểm của Yocto mà team sẽ phải đối mặt
3. Với sstate-cache được share qua NFS, build lần đầu của developer mới vào team mất bao lâu?
