SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-bfin"
TEXT_START_ADDR=0x0
MAXPAGESIZE=0x1000
TARGET_PAGE_SIZE=0x1000
NONPAGED_TEXT_START_ADDR=${TEXT_START_ADDR}
ARCH=bfin
MACHINE=
ENTRY=__start
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
EMBEDDED=yes
DATA_END_SYMBOLS="__edata = .; PROVIDE (_edata = .);"
END_SYMBOLS="__end = .; PROVIDE (_end = .);"
