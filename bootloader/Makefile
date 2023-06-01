DISK_NAME:=bootloader
QEMU_FLAGS:=-drive file=$(DISK_NAME),format=raw -s -no-shutdown -no-reboot

all: run

debug: $(DISK_NAME)
	qemu-system-x86_64 $(QEMU_FLAGS) -S

run: $(DISK_NAME)
	qemu-system-x86_64 $(QEMU_FLAGS)

$(DISK_NAME): stage0/entry.o stage1/entry.o
	cat $^ > $@

stage1/entry.o: $(shell find stage1/ -type f -name "*.asm")

%.o: %.asm
	nasm -f bin -o $@ $<

.PHONY: clean
clean:
	find . -type f -name "*.o" -delete
	rm -rf bootloader
