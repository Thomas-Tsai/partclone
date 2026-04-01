#!/bin/bash
set -ex

ARCH=$1
export DEBIAN_FRONTEND=noninteractive

# 1. Install dependencies for minimal build
apt-get update
apt-get install -y build-essential autoconf automake libtool gettext pkg-config e2fslibs-dev uuid-dev libblkid-dev libxxhash-dev autopoint intltool libtool-bin libssl-dev zlib1g-dev libzstd-dev e2fsprogs xsltproc docbook-xsl

# 2. Compile partclone (minimal ext4 build to save CI time on emulated architectures)
cd /workspace
./autogen
./configure --enable-extfs --enable-xxhash --disable-ncursesw
make clean || true
make -j $(nproc)

# 3. Create dummy ext4 filesystem
cd /workspace
truncate -s 50M base_${ARCH}.img
mkfs.ext4 -F base_${ARCH}.img

# 4. Mount and populate with random data (to ensure xxhash128bit is fully tested on payload data)
mkdir -p /mnt/base
mount -o loop base_${ARCH}.img /mnt/base
# Generate random data and write it to the image
dd if=/dev/urandom of=/mnt/base/random_data.bin bs=1M count=10

# Record the MD5 of the random data as the gold standard for restoration
md5sum /mnt/base/random_data.bin | awk '{print $1}' > /workspace/payload_md5_${ARCH}.txt

umount /mnt/base

# 5. Save image using partclone
src/partclone.extfs -c -s base_${ARCH}.img -O test_img_${ARCH}.img -F -q

# Cleanup dummy source to save space
rm base_${ARCH}.img
