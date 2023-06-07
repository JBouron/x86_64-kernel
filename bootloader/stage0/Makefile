all: stage0

stage0: entry.asm
	nasm -f bin -o $@ $<

.PHONY: clean
clean:
	rm -f stage0
