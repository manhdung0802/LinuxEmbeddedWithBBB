#!/bin/bash
# Build & Load kernel module to BBB
# Usage: ./build_loadToBBB.sh <module_name> [BBB_IP]
#
# Ví dụ:
#   ./build_loadToBBB.sh hello_module
#   ./build_loadToBBB.sh gpio_driver 192.168.1.100

MODULE_NAME=${1:?"Usage: $0 <module_name> [BBB_IP]"}
BBB_IP=${2:-"10.42.0.206"}
BBB_USER="debian"
KERNEL_DIR=${KERNEL_DIR:-"/lib/modules/$(uname -r)/build"}

echo "=== Building kernel module: ${MODULE_NAME} ==="

# Build kernel module (assumes Makefile exists in current dir)
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C ${KERNEL_DIR} M=$(pwd) modules
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed!"
    exit 1
fi

echo "=== Build OK. Deploying to BBB (${BBB_USER}@${BBB_IP}) ==="

# Copy .ko file to BBB
scp ${MODULE_NAME}.ko ${BBB_USER}@${BBB_IP}:/home/${BBB_USER}/ && sync

echo "=== Done! ==="
echo "On BBB, run:"
echo "  sudo insmod ${MODULE_NAME}.ko"
echo "  dmesg | tail"
echo "  sudo rmmod ${MODULE_NAME}"