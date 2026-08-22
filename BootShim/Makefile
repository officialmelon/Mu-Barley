ARCH              = arm64
TARGET            = aarch64-linux-gnu
CC                = clang
CFLAGS            = --target=$(TARGET)
OBJCOPY           = llvm-objcopy

# Preserve the historical one-address layout unless a platform explicitly
# separates the Linux kernel load address from the final UEFI FD address.
KERNEL_HEADER_TEXT_OFFSET ?= $(FD_BASE)
EARLY_VISUAL_TRACE ?= 0

all: BootShim.elf BootShim.bin

BootShim.bin: BootShim.elf
	$(OBJCOPY) -O binary $< $@

BootShim.elf: BootShim.S
	$(CC) ${CFLAGS} -c $< -o $@ -DREQUIRES_KERNEL_HEADER=$(REQUIRES_KERNEL_HEADER) -DKERNEL_HEADER_TEXT_OFFSET=$(KERNEL_HEADER_TEXT_OFFSET) -DFD_BASE=$(FD_BASE) -DFD_SIZE=$(FD_SIZE) -DEARLY_VISUAL_TRACE=$(EARLY_VISUAL_TRACE)

BootShim.S:
