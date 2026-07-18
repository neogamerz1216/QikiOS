CC = gcc
LD = ld
NASM = nasm

CFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -mno-red-zone -mcmodel=large -I. -nostdlib -fno-builtin -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -mno-avx
LDFLAGS = -nostdlib

SRCDIR = kernel
LIBCDIR = libc
BOOTDIR = boot
BUILDDIR = build

C_SOURCES = $(wildcard $(SRCDIR)/*.c) $(wildcard $(LIBCDIR)/*.c)
ASM_SOURCES = $(wildcard $(SRCDIR)/*.asm) $(wildcard $(BOOTDIR)/*.asm)
OBJECTS = $(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(C_SOURCES)))
ASM_OBJECTS = $(patsubst %.asm,$(BUILDDIR)/%.o,$(notdir $(ASM_SOURCES)))

TARGET = $(BUILDDIR)/qiki-os.bin
ISO = $(BUILDDIR)/qiki-os.iso

all: $(ISO)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(LIBCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.asm | $(BUILDDIR)
	$(NASM) -f elf64 $< -o $@

$(BUILDDIR)/%.o: $(BOOTDIR)/%.asm | $(BUILDDIR)
	$(NASM) -f elf64 $< -o $@

$(TARGET): $(OBJECTS) $(ASM_OBJECTS) $(SRCDIR)/linker.ld
	$(LD) -n -T $(SRCDIR)/linker.ld -o $@ $(OBJECTS) $(ASM_OBJECTS) $(LDFLAGS)

$(ISO): $(TARGET)
	mkdir -p $(BUILDDIR)/iso/boot/grub
	cp $(TARGET) $(BUILDDIR)/iso/boot/qiki-os.bin
	printf 'set timeout=0\nset default=0\nset gfxpayload=keep\ninsmod all_video\ninsmod vbe\n\nmenuentry "Qiki OS" {\n    multiboot2 /boot/qiki-os.bin\n    boot\n}\n' > $(BUILDDIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILDDIR)/iso 2>&1

run: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -serial stdio -m 128M -vga std -boot d

run-kernel: $(TARGET)
	qemu-system-x86_64 -kernel $(TARGET) -serial stdio -m 128M -vga std

run-debug: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -serial stdio -m 128M -vga std -boot d -d int,cpu_reset -D build/qemu.log

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean run
