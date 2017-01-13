#!/bin/bash

file=$1
func=$2
clang_version=$3
if [[ -z "$clang_version" ]]; then
    clang_version=4.0.0
fi

cmd="clang -cc1 -triple x86_64-unknown-linux-gnu -analyze -disable-free \
    -disable-llvm-verifier -main-file-name $file -analyzer-store=region \
    -analyzer-opt-analyze-nested-blocks -analyzer-eagerly-assume \
    -analyzer-checker=core -analyzer-checker=unix \
    -analyzer-checker=security.insecureAPI.UncheckedReturn \
    -analyzer-checker=security.insecureAPI.getpw \
    -analyzer-checker=security.insecureAPI.gets \
    -analyzer-checker=security.insecureAPI.mktemp \
    -analyzer-checker=security.insecureAPI.mkstemp \
    -analyzer-checker=security.insecureAPI.vfork -analyzer-output plist -w \
    -mrelocation-model static -mthread-model posix -fmath-errno -masm-verbose \
    -mconstructor-aliases -munwind-tables -fuse-init-array -target-cpu x86-64 \
    -momit-leaf-frame-pointer -dwarf-column-info \
    -resource-dir /usr/bin/../lib/clang/$clang_version \
    -D HAVE_CONFIG_H -D VERSION=\"0.3.5a\" -D PACKAGE=\"partclone\" \
    -D LOCALEDIR=\"/usr/share/locale\" -D _FILE_OFFSET_BITS=64 -D BTRFS \
    -D BTRFS_FLAT_INCLUDES -I . -I .. -internal-isystem /usr/include \
    -internal-isystem /usr/bin/../lib/clang/$clang_version/include \
    -internal-externc-isystem /include -internal-externc-isystem /usr/include \
    -O2 -ferror-limit 19 -fmessage-length 0 -mstackrealign -fobjc-runtime=gcc \
    -fdiagnostics-show-option -vectorize-loops -vectorize-slp -x c $file"
if [[ -n "$func" ]]; then
    cmd+="-analyze-function $func"
fi
echo $cmd
echo "================================================================="
$cmd

