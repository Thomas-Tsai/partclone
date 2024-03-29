
srcdir="$(dirname "$0")"

hostarch="$(dpkg-architecture -q DEB_BUILD_ARCH 2>/dev/null)" \
|| hostarch="$(rpm --eval '%{_arch}' 2>/dev/null)" \
|| hostarch="$(arch 2>/dev/null)" \
|| hostarch=''

case "${hostarch:-}" in
'i386'|'i486'|'i586'|'i686'|'amd64'|'x86_64')

    ############### compiles source file for x86 architectures ##################
    # compile the file fail-mbr.bin
    #echo -n "Compiling: fail-mbr.S -> fail-mbr.o -> "
    #gcc -Wall -Werror -m32 -nostdlib -static -Wl,--relocatable -o fail-mbr.o fail-mbr.S

    #echo -n "fail-mbr.image -> "
    #gcc -Os -Wall -W -Wshadow -Wpointer-arith -Wmissing-prototypes -Wundef -Wstrict-prototypes -g -falign-jumps=1 -falign-loops=1 -falign-functions=1 -mno-mmx -mno-sse -mno-sse2 -mno-3dnow -fno-dwarf2-cfi-asm -fno-asynchronous-unwind-tables -m32 -fno-stack-protector -mno-stack-arg-probe -Werror -Wno-trampolines -DUSE_ASCII_FAILBACK=1 -DHAVE_UNIFONT_WIDTHSPEC=1  -mrtd -mregparm=3       -fno-builtin   -m32 -Wl,--build-id=none   -nostdlib -Wl,-N,-S -Wl,-N -Wl,-Ttext,0x7C00   -Wl,--no-dynamic-linker   -o fail-mbr.image fail-mbr.o

    #echo "fail-mbr.bin [Done]. "
    gcc -c "${srcdir}/"fail-mbr.S -o fail-mbr.image
    objcopy  -O binary  --strip-unneeded -R .note -R .comment -R .note.gnu.build-id -R .note.gnu.property -R .reginfo -R .rel.dyn fail-mbr.image fail-mbr.bin

    ######################### Checks the build #############################
    # checks that this file does the same than the original fail-mbr
    echo "Checking the file:"
    objdump -D -b binary -mi386 -Maddr16,data16 fail-mbr.bin > f1.obj
    objdump -D -b binary -mi386 -Maddr16,data16 "${srcdir}/"fail-mbr.bin.orig > f2.obj

    ######################### Checks the build #############################
    # the regular expression 'fail-mbr|---|xor    %ax,%ax|^[a-f0-9]*$' is used
    # to reject lines which contain either:
    # - the name of the file, "fail-mbr"
    # - a line separator output by diff, "---"
    # - the OP code "xor    %ax,%ax" which may begin by 0x31 or 0x33
    # - a numerical offset output by diff, ^[a-f0-9]*$
    report=$(diff f1.obj f2.obj | grep -Ev 'fail-mbr|---|xor    %ax,%ax|^[a-f0-9]*$')
    if [ -n "$report" ]; then
        echo "files fail-mbr.bin and fail-mbr.bin.orig differ significantly:"
        diff f1.obj f2.obj
        exit 1
    else
        echo "'fail-mbr.bin' and 'fail-mbr.bin.orig' have no significant differences."
        rm f1.obj f2.obj
        exit 0
    fi

   ;;

*)

    echo "The architecture is not x86, so the file 'fail-mbr.bin' is not compiled"
    echo "Copying 'fail-mbr.bin.orig' to 'fail-mbr.bin'"
    cp -p "${srcdir}/"fail-mbr.bin.orig fail-mbr.bin

esac
