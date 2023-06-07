SRC_FILES := $(shell find . -type f -name "*.asm")
OBJ_FILES := $(patsubst %.asm,%.o,$(SRC_FILES))

all: stage1

stage1: $(OBJ_FILES)
	ld --script=linker.ld --orphan-handling="error" -o $@ $^

%.o: %.asm
	nasm -f elf64 -o $@ $<

.PHONY: clean
clean:
	find . -type f -name "*.o" -delete
	rm -f stage1
