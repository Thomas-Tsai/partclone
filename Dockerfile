FROM ubuntu:latest
MAINTAINER Thomas-Tsai <tlinux.tsai@gmail.com>
RUN sed -i s/archive.ubuntu.com/free.nchc.org.tw/g /etc/apt/sources.list
RUN apt-get update
RUN apt-get install -y f2fs-tools hfsprogs exfatprogs gddrescue libfuse-dev jfsutils reiser4progs hfsprogs hfsplus reiserfsprogs btrfs-progs wget gnupg2 git
RUN echo '# drbl repository' >> /etc/apt/sources.list 
RUN echo 'deb http://free.nchc.org.tw/drbl-core drbl stable testing unstable dev' >> /etc/apt/sources.list 
RUN echo 'deb-src http://free.nchc.org.tw/drbl-core drbl stable testing unstable dev' >> /etc/apt/sources.list
RUN wget http://drbl.nchc.org.tw/GPG-KEY-DRBL -O - | apt-key add -
RUN apt-get update
RUN apt-get -y build-dep partclone
RUN git clone https://github.com/Thomas-Tsai/partclone.git /partclone
WORKDIR /partclone
RUN ./autogen && ./configure --enable-fs-test --enable-feature-test --enable-extfs --enable-ntfs --enable-fat --enable-exfat --enable-hfsp --enable-apfs --enable-btrfs --enable-minix --enable-f2fs --enable-reiser4 --enable-xfs && make && make install
WORKDIR /partclone/tests

