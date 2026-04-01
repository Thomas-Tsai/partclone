#!/bin/bash
set -ex

export DEBIAN_FRONTEND=noninteractive

# 1. Install dependencies for minimal build
apt-get update
apt-get install -y build-essential autoconf automake libtool gettext pkg-config e2fslibs-dev uuid-dev libblkid-dev libxxhash-dev bc autopoint intltool libtool-bin libssl-dev zlib1g-dev libzstd-dev e2fsprogs xsltproc docbook-xsl

# 2. Compile partclone
cd /workspace
# 確保上一次編譯架構遺留下來的 object files 被徹底清除乾淨
find . -name "*.o" -delete
find . -name "*.lo" -delete
find . -name "*.la" -delete
rm -rf .deps src/.deps

./autogen
./configure --enable-extfs --enable-xxhash --disable-ncursesw
make clean || true
make -j $(nproc)

# 3. Restore and verify EACH image
cd /workspace
mkdir -p /mnt/restored

declare -a RESULTS
FAILED=0

# Disable exit on error for the loop execution to collect all results
set +e

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
    if ! src/partclone.extfs -r -s "$img" -O target.img -F -q; then
        echo "❌ FATAL: partclone restore failed for image from ${ARCH_OF_IMG}!"
        RESULTS+=("❌ FAIL (Restore): Image from ${ARCH_OF_IMG}")
        FAILED=1
        rm -f target.img
        continue
    fi
    
    # Run fsck to verify filesystem integrity
    if ! e2fsck -fn target.img; then
        echo "❌ FATAL: e2fsck failed for image from ${ARCH_OF_IMG}!"
        RESULTS+=("❌ FAIL (fsck): Image from ${ARCH_OF_IMG}")
        FAILED=1
        rm -f target.img
        continue
    fi
    
    # Mount and verify that the MD5 Hash of the file matches
    mkdir -p /mnt/target
    if ! mount -o loop target.img /mnt/target; then
        echo "❌ FATAL: mount failed for image from ${ARCH_OF_IMG}!"
        RESULTS+=("❌ FAIL (mount): Image from ${ARCH_OF_IMG}")
        FAILED=1
        rm -f target.img
        continue
    fi
    
    EXPECTED_MD5=$(cat "/workspace/downloaded-images/payload_md5_${ARCH_OF_IMG}.txt")
    ACTUAL_MD5=$(md5sum /mnt/target/random_data.bin | awk '{print $1}')
    
    echo "Verifying MD5 checksum..."
    if [ "$EXPECTED_MD5" != "$ACTUAL_MD5" ]; then
        echo "❌ FATAL: MD5 mismatch for image created by ${ARCH_OF_IMG}!"
        echo "Expected: $EXPECTED_MD5"
        echo "Actual:   $ACTUAL_MD5"
        RESULTS+=("❌ FAIL (MD5 mismatch): Image from ${ARCH_OF_IMG}")
        FAILED=1
    else
        echo "✅ SUCCESS: MD5 checksum perfectly matches! ($ACTUAL_MD5)"
        RESULTS+=("✅ PASS: Image from ${ARCH_OF_IMG}")
    fi
    umount /mnt/target
    
    # Clean up
    rm -f target.img
done

# Re-enable exit on error
set -e

echo
echo "========================================"
echo "          RESTORE TEST SUMMARY          "
echo "========================================"
for res in "${RESULTS[@]}"; do
    echo "$res"
done

# Write the summary to a file so GitHub Actions can append it to GITHUB_STEP_SUMMARY
{
    echo "### Restore Results (Host: $(uname -m))"
    for res in "${RESULTS[@]}"; do
        echo "- $res"
    done
    echo ""
} > /workspace/restore_summary.txt

if [ $FAILED -ne 0 ]; then
    echo "❌ Some restore tests failed!"
    exit 1
else
    echo "✅ All restore tests passed on this architecture!"
    exit 0
fi
