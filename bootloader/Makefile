DISK_NAME:=bootloader
QEMU_FLAGS:=-drive file=$(DISK_NAME),format=raw -s -no-shutdown -no-reboot

all: run

debug: $(DISK_NAME)
	qemu-system-x86_64 $(QEMU_FLAGS) -S & echo $$! > .qemu.pid
	gdb -ex "target remote localhost:1234" -ex "symbol-file ./stage1/stage1Syms" || true
	kill -9 $$(cat .qemu.pid)

run: $(DISK_NAME)
	qemu-system-x86_64 $(QEMU_FLAGS)

$(DISK_NAME): stage0/stage0 stage1/stage1
	cat $^ > $@

.PHONY: stage0/stage0
stage0/stage0:
	$(MAKE) -C stage0

.PHONY: stage1/stage1
stage1/stage1:
	$(MAKE) -C stage1

%.o: %.asm
	nasm -f bin -o $@ $<

.PHONY: clean
clean:
	$(MAKE) -C stage0 clean
	$(MAKE) -C stage1 clean
