# Bài 28 - Cài Đặt Môi Trường Yocto

## Setup nhanh (từ đầu)

```bash
# 1. Install dependencies
sudo apt-get install -y gawk wget git diffstat unzip texinfo gcc \
    build-essential chrpath socat cpio python3 python3-pip \
    python3-pexpect xz-utils debianutils iputils-ping zstd liblz4-tool

# 2. Clone (branch: kirkstone)
mkdir ~/yocto-bbb && cd ~/yocto-bbb
git clone -b kirkstone https://git.yoctoproject.org/poky.git
git clone -b kirkstone https://github.com/openembedded/meta-openembedded.git
git clone -b kirkstone https://git.yoctoproject.org/meta-ti.git

# 3. Init build environment
source poky/oe-init-build-env build-bbb

# 4. Cấu hình (xem bên dưới)

# 5. Build
bitbake core-image-minimal
```

## local.conf (key settings)

```bitbake
MACHINE = "beaglebone"
DISTRO ?= "poky"
BB_NUMBER_THREADS ?= "8"
PARALLEL_MAKE ?= "-j 8"
PACKAGE_CLASSES ?= "package_ipk"
EXTRA_IMAGE_FEATURES ?= "debug-tweaks ssh-server-openssh"
```

## bblayers.conf

```bitbake
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

## Lệnh thường dùng

```bash
bitbake-layers show-layers         # Kiểm tra layers
bitbake-layers show-recipes | grep X  # Tìm recipe
bitbake core-image-minimal         # Build image
bitbake <recipe> -c clean          # Clean recipe
bitbake -e <recipe> | grep ^VAR    # Xem giá trị biến
cat tmp/work/.../temp/log.do_<task>  # Xem log lỗi
```

## Flash lên SD

```bash
# bmaptool (nhanh nhất)
sudo bmaptool copy *.wic /dev/sdX

# dd (luôn có sẵn)
sudo dd if=*.wic of=/dev/sdX bs=4M status=progress && sudo sync
```

## Build time ước tính (8 core)
- Lần 1: ~2-3 tiếng (full build)
- Lần 2+: ~5-15 phút (sstate cache)
