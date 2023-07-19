# Top-level Makefile of the project doing the following:
# 	- Build bootloader.
# 	- Build kernel.
# 	- Create disk image to be booted in Qemu.
# Most of this Makefile is meant to be running within the kernelbuilder docker
# container. For convenience the default target executes the build target within
# the container so that only `make` needs to be executed on the host.
# FIXME: Having to enter the password to run the container is _very_ annoying,
# but the other alternative is to have special docker permission which are
# pretty much root permissions. This should be reworked.

# Flags
# =====

# Make sure to use the cross-compiler toolchain. 
CC := x86_64-elf-gcc
CXX := x86_64-elf-g++
AS := x86_64-elf-as
LD := x86_64-elf-ld
NASM := nasm

CXXFLAGS := -ffreestanding -Wall -Wextra -fno-exceptions -fno-rtti \
	-mno-red-zone -nostdlib -I./include -std=c++20 -g

KERNEL_IMG_NAME := kernel.img
DISK_IMG_NAME := disk.img

QEMU_FLAGS := -drive file=$(DISK_IMG_NAME),format=raw -s -no-shutdown -no-reboot

# Source files
# ============

# All directories that contain kernel cpp files.
SOURCE_DIRS := ./kernel ./misc

# All CPP files that should be part of the kernel image.
CPP_SOURCES := $(shell find $(SOURCE_DIRS) -name "*.cpp")

# All ASM files that should be part of the kernel image. Note that we exclude
# the crt*.asm files (e.g. crt0.asm, crti.asm and crtn.asm) as those need to be
# linked in a specific order.
ASM_SOURCES := $(shell find $(SOURCE_DIRS) -name "*.asm" -not -name "crt*.asm")

# CRT*.o files coming from the compiler:
CRTI := $(shell find $(SOURCE_DIRS) -name "crti.asm")
CRTI_O := $(CRTI:.asm=.o)
CRTBEGIN_O := $(shell $(CXX) $(CXXFLAGS) -print-file-name=crtbegin.o)
CRTEND_O := $(shell $(CXX) $(CXXFLAGS) -print-file-name=crtend.o)
CRTN := $(shell find $(SOURCE_DIRS) -name "crtn.asm")
CRTN_O := $(CRTN:.asm=.o)

# All the object files making up the kernel image. The order of the object files
# is extremely important: crti.o and crtbegin.o followed by all object files
# followed by crtend.o and crtn.o.
OBJ_FILES := $(CRTI_O) $(CRTBEGIN_O) \
	$(CPP_SOURCES:.cpp=.o) $(ASM_SOURCES:.asm=.o) \
	$(CRTEND_O) $(CRTN_O)

# Rules
# =====

# Build and run the kernel.
all:
	sudo docker run -ti -v $$PWD/:/src/ --user $$UID:$$GID \
		kernelbuilder make -j32 -C /src build
	qemu-system-x86_64 $(QEMU_FLAGS)

# Build the bootloader and the kernel, must be executed within the kernelbuilder
# docker container since this require using the cross-compiler.
.PHONY: build
build: $(KERNEL_IMG_NAME)
	$(MAKE) -C bootloader
	bootloader/createDiskImage.py \
		bootloader/stage0/stage0 \
		bootloader/stage1/stage1 \
		$(KERNEL_IMG_NAME) \
		$(DISK_IMG_NAME)

# FIXME: For now manually compile the kernel stub, eventually we should have
# nicer rules here.
# FIXME: The bootloader does not create an ID map of the entire RAM yet, hence
# the kernel must be loaded at vaddr < 4MiB.
$(KERNEL_IMG_NAME): $(OBJ_FILES)
	$(LD) --script=linker.ld $^ -o $@

%.o: %.asm
	$(NASM) -felf64 -o $@ $^

.PHONY: clean
clean:
	$(MAKE) -C bootloader clean
	rm -rf $(OBJ_FILES) $(KERNEL_IMG_NAME) $(DISK_IMG_NAME)
