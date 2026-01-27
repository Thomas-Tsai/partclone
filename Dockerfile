FROM debian:sid

LABEL maintainer="Thomas-Tsai <tlinux.tsai@gmail.com>"
ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's/deb.debian.org/free.nchc.org.tw/g' /etc/apt/sources.list.d/debian.sources && \
    sed -i 's/Types: deb/Types: deb deb-src/g' /etc/apt/sources.list.d/debian.sources && \
    sed -i 's/Components: main/Components: main non-free/g' /etc/apt/sources.list.d/debian.sources

RUN apt-get update && \
    apt-get install -y wget gnupg2 git && \
    wget -O /etc/apt/trusted.gpg.d/drbl-gpg.asc https://drbl.org/GPG-KEY-DRBL

RUN echo "Types: deb deb-src\n\
URIs: http://free.nchc.org.tw/drbl-core\n\
Suites: drbl\n\
Components: stable testing unstable dev\n\
Signed-By: /etc/apt/trusted.gpg.d/drbl-gpg.asc" > /etc/apt/sources.list.d/drbl.sources

RUN apt-get update && \
    apt-get build-dep -y partclone && \
    apt-get install -y \
    f2fs-tools hfsplus exfatprogs gddrescue libfuse-dev jfsutils \
    hfsprogs reiserfsprogs btrfs-progs libxxhash-dev libfuse3-dev\
    libisal-dev zlib1g-dev libzstd-dev libjfs-dev libufs2 ufsutils \
    vmfs-tools libvmfs libbsd0 libbsd-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /partclone
RUN git clone https://github.com/Thomas-Tsai/partclone.git . && \
    ./autogen && \
    CFLAGS="-std=gnu99 -g -O2" ./configure \
    --enable-fs-test --enable-feature-test --enable-ncursesw --enable-xxhash \
    --enable-extfs --enable-ntfs --enable-fat --enable-exfat \
    --enable-hfsp --enable-apfs --enable-btrfs --enable-minix --enable-f2fs \
    --enable-xfs --enable-nilfs2 --enable-fuse --enable-jfs \
    --enable-ufs --enable-vmfs && \
    make && \
    make install

WORKDIR /partclone/tests
