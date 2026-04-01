#!/bin/bash
set -ex

export DEBIAN_FRONTEND=noninteractive

# 1. Install dependencies for minimal build
apt-get update
apt-get install -y build-essential autoconf automake libtool gettext pkg-config e2fslibs-dev uuid-dev libblkid-dev libxxhash-dev bc autopoint intltool libtool-bin

# 2. Compile partclone
cd /workspace
./autogen
./configure --enable-extfs --enable-xxhash --disable-ncursesw
make -j $(nproc)

# 3. Restore and verify EACH image
cd /workspace
mkdir -p /mnt/restored

for img in /workspace/downloaded-images/test_img_*.img; do
    # Extract the architecture that created this image
    filename=$(basename -- "$img")
    ARCH_OF_IMG="${filename#test_img_}"
    ARCH_OF_IMG="${ARCH_OF_IMG%.img}"

    echo "========================================"
    echo "Restoring image created by: $ARCH_OF_IMG ($img)"
    echo "========================================"
    
    # Create empty target
    truncate -s 50M target.img
    
    # Restore
    src/partclone.extfs -r -s $img -O target.img -F -q
    
    # Run fsck to verify filesystem integrity
    e2fsck -fn target.img
    
    # Mount and verify that the MD5 Hash of the file matches
    mkdir -p /mnt/target
    mount -o loop target.img /mnt/target
    
    EXPECTED_MD5=$(cat "/workspace/downloaded-images/payload_md5_${ARCH_OF_IMG}.txt")
    ACTUAL_MD5=$(md5sum /mnt/target/random_data.bin | awk '{print $1}')
    
    echo "Verifying MD5 checksum..."
    if [ "$EXPECTED_MD5" != "$ACTUAL_MD5" ]; then
        echo "❌ FATAL: MD5 mismatch for image created by ${ARCH_OF_IMG}!"
        echo "Expected: $EXPECTED_MD5"
        echo "Actual:   $ACTUAL_MD5"
        umount /mnt/target
        exit 1
    else
        echo "✅ SUCCESS: MD5 checksum perfectly matches! ($ACTUAL_MD5)"
    fi
    umount /mnt/target

    
    # Clean up
    rm target.img
done

echo "All restore tests passed on this architecture!"
