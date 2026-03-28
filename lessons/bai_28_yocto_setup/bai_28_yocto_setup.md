# Bài 28 - Cài Đặt Môi Trường Yocto cho BeagleBone Black

## 1. Mục tiêu bài học

- Chuẩn bị host system đúng cách (Ubuntu 20.04/22.04)
- Clone Poky, meta-ti, meta-openembedded (branch Kirkstone)
- Khởi tạo build environment và hiểu các biến môi trường
- Cấu hình `local.conf` và `bblayers.conf` cho BBB
- Chạy build lần đầu và hiểu cấu trúc output

---

## 2. Yêu Cầu Host System

### Hệ điều hành hỗ trợ

```
✅ Ubuntu 22.04 LTS (Jammy)  ← Khuyến nghị nhất
✅ Ubuntu 20.04 LTS (Focal)  ← Được test tốt
✅ Debian 11/12
✅ Windows WSL2 + Ubuntu 22.04  ← Hoạt động được, chậm hơn ~20%
❌ Windows native             ← KHÔNG hỗ trợ (case-sensitive FS cần thiết)
❌ macOS native               ← KHÔNG hỗ trợ
```

### Phần cứng tối thiểu

| Tài nguyên | Tối thiểu | Khuyến nghị | Ghi chú |
|-----------|----------|-------------|---------|
| RAM | 4 GB | 16 GB | BitBake dùng nhiều RAM để parse |
| CPU cores | 2 | 8+ | Parallel builds |
| Disk (build) | 50 GB | 100+ GB | tmp/ có thể 50GB+ |
| Disk (DL cache) | 10 GB | 30 GB | Source downloads dùng lại |
| Internet | Cần | Băng thông tốt | Tải hàng GB source |

---

## 3. Cài đặt Dependencies

```bash
# Ubuntu 20.04 / 22.04
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev xterm python3-subunit mesa-common-dev \
    zstd liblz4-tool file locales libacl1

# Cài đặt locale (Yocto yêu cầu UTF-8)
sudo locale-gen en_US.UTF-8
sudo update-locale LANG=en_US.UTF-8

# Đặt git identity (Yocto cần khi patch)
git config --global user.name "Your Name"
git config --global user.email "your@email.com"

# Verify
python3 --version   # phải ≥ 3.8
git --version
gcc --version
```

---

## 4. Clone Repositories

```bash
# Tạo thư mục dự án
mkdir -p ~/yocto-bbb
cd ~/yocto-bbb

# 1. Poky (Yocto reference distribution)
#    Chứa: BitBake, OE-Core, meta-poky, meta-yocto-bsp
git clone -b kirkstone https://git.yoctoproject.org/poky.git

# 2. meta-openembedded (community recipes)
#    Chứa: meta-oe, meta-python, meta-networking, meta-multimedia
git clone -b kirkstone https://github.com/openembedded/meta-openembedded.git

# 3. meta-ti (Texas Instruments BSP)
#    Chứa: AM335x kernel config, U-Boot, PMIC drivers
git clone -b kirkstone https://git.yoctoproject.org/meta-ti.git

# Kiểm tra
ls ~/yocto-bbb/
# meta-openembedded/  meta-ti/  poky/

# Kiểm tra branch
cd poky && git branch -a | grep remotes/origin/kirkstone
cd ../meta-ti && git log --oneline -3
```

---

## 5. Khởi Tạo Build Environment

```bash
cd ~/yocto-bbb

# Script này làm 3 việc:
#  1. Set các environment variables (PATH, BBPATH, BUILDDIR...)
#  2. Tạo thư mục build-bbb/ nếu chưa có
#  3. cd vào build-bbb/
source poky/oe-init-build-env build-bbb

# Shell sẽ hiện:
# ### Shell environment set up for builds. ###
# Common targets are:
#   core-image-minimal
#   core-image-sato
#   ...
#
# You can also run generated qemu images with a command like 'runqemu qemux86'

# Xác nhận vị trí
pwd
# /home/user/yocto-bbb/build-bbb
```

> **QUAN TRỌNG:** Mỗi khi mở terminal mới, phải source lại:
> ```bash
> source ~/yocto-bbb/poky/oe-init-build-env build-bbb
> ```

---

## 6. Cấu Hình local.conf

```bash
nano ~/yocto-bbb/build-bbb/conf/local.conf
```

Tìm và sửa/thêm các dòng sau:

```bitbake
# ─── MACHINE: Chọn board target ──────────────────────────────────
# Mặc định: MACHINE ??= "qemuarm"  ← comment hoặc xóa
MACHINE = "beaglebone"      # BBB với AM335x (meta-ti)
# MACHINE = "beaglebone-yocto"  ← dùng nếu chỉ có meta-yocto-bsp

# ─── DISTRO: Distribution ────────────────────────────────────────
DISTRO ?= "poky"            # Giữ mặc định

# ─── PARALLEL BUILDS ─────────────────────────────────────────────
# Chỉnh theo số core CPU của bạn
BB_NUMBER_THREADS ?= "8"    # BitBake tasks song song
PARALLEL_MAKE ?= "-j 8"     # make -j8

# ─── PACKAGE FORMAT ───────────────────────────────────────────────
PACKAGE_CLASSES ?= "package_ipk"  # Nhỏ hơn rpm, phù hợp embedded

# ─── IMAGE FEATURES ───────────────────────────────────────────────
# debug-tweaks: root login không cần password
# ssh-server-openssh: cài openssh vào image
EXTRA_IMAGE_FEATURES ?= "debug-tweaks ssh-server-openssh"

# ─── CACHES (tuỳ chọn nhưng khuyến nghị) ─────────────────────────
# Đặt downloads và sstate ra ngoài build dir để share giữa projects
# DL_DIR ?= "/data/yocto/downloads"        # Chia sẻ downloads
# SSTATE_DIR ?= "/data/yocto/sstate-cache" # Chia sẻ build cache

# ─── DISK SPACE CHECK ────────────────────────────────────────────
# Cảnh báo khi disk < 1GB còn trống
BB_DISKMON_DIRS ??= "\
    STOPTASKS,${TMPDIR},1G,100K \
    STOPTASKS,${DL_DIR},1G,100K \
    STOPTASKS,${SSTATE_DIR},1G,100K \
    ABORT,${TMPDIR},100M,1K \
    ABORT,${DL_DIR},100M,1K \
    ABORT,${SSTATE_DIR},100M,1K"
```

---

## 7. Cấu Hình bblayers.conf

```bash
nano ~/yocto-bbb/build-bbb/conf/bblayers.conf
```

```bitbake
# bblayers.conf — Khai báo tất cả layers cần dùng
# CẢNH BÁO: Dùng đường dẫn tuyệt đối

BBLAYERS ?= " \
  /home/user/yocto-bbb/poky/meta \
  /home/user/yocto-bbb/poky/meta-poky \
  /home/user/yocto-bbb/poky/meta-yocto-bsp \
  /home/user/yocto-bbb/meta-openembedded/meta-oe \
  /home/user/yocto-bbb/meta-openembedded/meta-python \
  /home/user/yocto-bbb/meta-ti/meta-ti-bsp \
  /home/user/yocto-bbb/meta-ti/meta-ti-extras \
  "
```

```bash
# Kiểm tra layers đã được nhận diện
bitbake-layers show-layers

# Output mong đợi:
# layer             path                              priority
# ================================================================
# meta              .../poky/meta                     5
# meta-poky         .../meta-poky                     5
# meta-yocto-bsp    .../meta-yocto-bsp                5
# meta-oe           .../meta-openembedded/meta-oe     6
# meta-python       .../meta-python                   7
# meta-ti-bsp       .../meta-ti/meta-ti-bsp           8

# Kiểm tra machine beaglebone có sẵn
bitbake-layers show-recipes | grep "beaglebone"
```

---

## 8. Build Lần Đầu

```bash
# Đảm bảo đang ở build dir và đã source env
cd ~/yocto-bbb/build-bbb

# Build core-image-minimal cho BBB
# LẦN ĐẦU: mất 2-6 tiếng tùy máy!
bitbake core-image-minimal

# Theo dõi progress chi tiết hơn (log vào file)
bitbake core-image-minimal 2>&1 | tee build_$(date +%Y%m%d).log
```

### Thứ tự BitBake compile (đơn giản hóa)

```
1. Tạo cross-compiler (arm-linux-gnueabihf-gcc):  ~20 phút
2. Tạo native tools (openssl, python3 cho host):   ~15 phút
3. Compile glibc (C library):                      ~15 phút
4. Compile busybox, util-linux:                    ~5 phút
5. Compile U-Boot (bootloader):                    ~3 phút
6. Compile Linux kernel (AM335x):                  ~10 phút
7. Build tất cả packages còn lại:                  ~20 phút
8. Tạo rootfs và image:                            ~5 phút
```

---

## 9. Kiểm Tra Output

```bash
# Xem các file xuất ra
ls -lh tmp/deploy/images/beaglebone/

# Output:
# MLO                                        ← First stage bootloader
# u-boot.img                                 ← U-Boot
# zImage                                     ← Linux kernel
# am335x-boneblack.dtb                       ← Device Tree (BBB)
# am335x-bone.dtb                            ← Device Tree (Bone)
# core-image-minimal-beaglebone.wic          ← SD card image ← DÙNG CÁI NÀY
# core-image-minimal-beaglebone.wic.bmap     ← Bmap (flash nhanh hơn)
# core-image-minimal-beaglebone.tar.bz2      ← rootfs tarball
# core-image-minimal-beaglebone.ext4         ← rootfs ext4
```

---

## 10. Flash và Boot BBB

```bash
# Xác định SD card device
lsblk | grep -v loop
# sda   8:0  0 500G ...  ← ổ cứng chính, ĐỪNG CHỌN
# sdb   8:16 1  16G ...  ← SD card

# Flash bằng bmaptool (nhanh hơn dd ~3x)
cd tmp/deploy/images/beaglebone/
sudo bmaptool copy core-image-minimal-beaglebone.wic /dev/sdb

# Hoặc dùng dd (chậm hơn nhưng luôn có sẵn)
sudo dd \
    if=core-image-minimal-beaglebone.wic \
    of=/dev/sdb \
    bs=4M \
    status=progress
sudo sync

# Boot BBB từ SD card:
# 1. Cắm SD card vào BBB
# 2. Giữ nút BOOT (S2) → cắm nguồn → thả nút sau ~5s
# 3. Kết nối serial UART0 (P9.21/P9.22) 115200-8N1
```

---

## 11. Debug Khi Gặp Lỗi

```bash
# Hiểu thông báo lỗi BitBake
ERROR: <recipe>-<version>-<rev> do_<task>:  ← Lỗi ở task nào
  ...
# Tìm log chi tiết:
cat tmp/work/<arch>/<recipe>/<version>/temp/log.do_<task>

# Lỗi thường gặp và cách fix:

# 1. "Nothing provides 'xyz'"
bitbake -s | grep xyz              # Tìm recipe cung cấp xyz
# Nếu không có → cần thêm layer hoặc viết recipe

# 2. "Layer is not compatible"
# meta-ti branch không khớp với poky branch
cd ~/yocto-bbb/meta-ti && git checkout kirkstone

# 3. "Fetcher failure" (không tải được source)
# Kiểm tra internet, hoặc dùng mirror
BB_FETCH_PREMIRRORONLY = "1"   # Chỉ dùng mirror (offline)

# 4. "Disk space low"
df -h    # Kiểm tra dung lượng
bitbake -c clean <recipe>      # Xóa build của recipe cụ thể
```

---

## 12. Câu hỏi kiểm tra

1. Tại sao phải `source oe-init-build-env` mỗi khi mở terminal mới thay vì thêm vào `.bashrc`?
2. `BB_NUMBER_THREADS` và `PARALLEL_MAKE` khác nhau như thế nào? Nên đặt bằng bao nhiêu cho máy 8-core?
3. Tại sao không nên đặt `DL_DIR` và `SSTATE_DIR` bên trong thư mục build?
4. `beaglebone` vs `beaglebone-yocto` machine name khác nhau như thế nào?
5. Sau khi flash `.wic` lên SD, lần boot đầu BBB cần làm gì để boot từ SD thay vì eMMC?
