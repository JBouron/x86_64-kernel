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

# Make sure to use the cross-compiler toolchain. 
CC := x86_64-elf-gcc
CXX := x86_64-elf-g++
AS := x86_64-elf-as
LD := x86_64-elf-ld

CXXFLAGS := -ffreestanding -Wall -Wextra -fno-exceptions -fno-rtti \
	-mno-red-zone -nostdlib

QEMU_FLAGS:=-drive file=disk.img,format=raw -s -no-shutdown -no-reboot

# Build and run the kernel.
all:
	sudo docker run -ti -v $$PWD/:/src/ --user $$UID:$$GID \
		kernelbuilder make -j32 -C /src build
	qemu-system-x86_64 $(QEMU_FLAGS)

# Build the bootloader and the kernel, must be executed within the kernelbuilder
# docker container since this require using the cross-compiler.
.PHONY: build
build: kernel
	$(MAKE) -C bootloader
	bootloader/createDiskImage.py \
		bootloader/stage0/stage0 \
		bootloader/stage1/stage1 \
		kernel \
		disk.img

# FIXME: For now manually compile the kernel stub, eventually we should have
# nicer rules here.
# FIXME: The bootloader does not create an ID map of the entire RAM yet, hence
# the kernel must be loaded at vaddr < 4MiB.
kernel: kernel.o
	$(LD) --entry=kernelEntry -Ttext=0x100000 -Tdata=0x101000 -Tbss=0x102000 \
		-Trodata-segment=0x103000 $^ -o $@	

.PHONY: clean
clean:
	$(MAKE) -C bootloader clean
	rm -rf kernel kernel.o disk.img
