SRC_FILES := $(shell find . -type f -name "*.asm")
OBJ_FILES := $(patsubst %.asm,%.o,$(SRC_FILES))

all: stage1 stage1Syms

# Raw binary file for stage1. This file is always a size that is a multiple of
# the disk block size and is meant to immediately follow stage0's sector on
# disk. Contains the absolute minimum for correct operation, e.g. code and data
# only.
stage1: $(OBJ_FILES)
	ld --script=linker.ld --oformat binary --orphan-handling="discard" -o $@ $^

# ELF file containing stage1's code and debug information. This is not meant to
# be used on disk but rather to be loaded by GDB to get information on symbols.
stage1Syms: $(OBJ_FILES)
	ld --script=linker.ld --orphan-handling="place" -o $@ $^

%.o: %.asm
	nasm -f elf64 -g -o $@ $<

.PHONY: clean
clean:
	find . -type f -name "*.o" -delete
	rm -f stage1
