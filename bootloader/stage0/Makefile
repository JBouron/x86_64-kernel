all: run

debug: bootsector
	qemu-system-x86_64 -drive file=bootsector,format=raw -S -s

run: bootsector
	qemu-system-x86_64 -drive file=bootsector,format=raw -s

bootsector: entry.asm
	nasm -f bin -o $@ $^

.PHONY: clean
clean:
	rm -rf bootsector
