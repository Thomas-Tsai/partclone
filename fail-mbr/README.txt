== How to disassemble fail-mbr.bin ==

objdump -D -b binary -mi386 -Maddr16,data16 fail-mbr.bin > fail-mbr.src

this feeds the file fail-mbr.src with opcodes. However some opcodes are
irrelevant, since they are never executed. Those stand for ascii strings
or space-filling zeros.

== How to build fail-mbr.bin ==

"make" does it.

The Makefile has an other target "clean" to clean the source.

The file Makefile has been inspired by the build logfile of the package
grub2. In that package, assembly sources are compiled once, then linked
and finally stripped down. The switches for gxx and objcopy were
shamelessly copied from this log file.
