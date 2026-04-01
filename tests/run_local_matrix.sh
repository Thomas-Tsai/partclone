#!/bin/bash
set -e

# 切換到專案根目錄
cd "$(dirname "$0")/.."

echo "=========================================================="
echo "Setting up QEMU user-mode emulation for local Docker..."
echo "=========================================================="
# 這部會向 Linux 核心註冊 binfmt_misc，讓 Docker 可以跑非本機 CPU 架構的映像檔
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

ARCHS=("amd64" "386" "arm64" "riscv64")

# 建立給 Restore 階段專用的資料夾
rm -rf downloaded-images
mkdir -p downloaded-images

# -----------------
# 階段一：生成映像檔
# -----------------
for ARCH in "${ARCHS[@]}"; do
    echo "=========================================================="
    echo "Phase 1: Generating image on architecture [ ${ARCH} ]..."
    echo "=========================================================="
    
    docker run --rm --privileged \
        --platform linux/${ARCH} \
        -v "$(pwd)":/workspace \
        debian:trixie \
        bash /workspace/tests/cross_arch_save.sh ${ARCH}
        
    # 將產生的備份檔跟 MD5 簽章搬移到共用資料夾
    mv test_img_${ARCH}.img downloaded-images/
    mv payload_md5_${ARCH}.txt downloaded-images/
done

# -----------------
# 階段二：還原並驗證
# -----------------
for ARCH in "${ARCHS[@]}"; do
    echo "=========================================================="
    echo "Phase 2: Restoring and verifying on architecture [ ${ARCH} ]..."
    echo "=========================================================="
    
    docker run --rm --privileged \
        --platform linux/${ARCH} \
        -v "$(pwd)":/workspace \
        debian:trixie \
        bash /workspace/tests/cross_arch_restore.sh
done

echo "=========================================================="
echo "🎉 All local cross-architecture matrix tests passed successfully!"
echo "=========================================================="
