#!/usr/bin/env python3

# Construct a disk image from the different stages of the bootloader and the
# kernel image.
# The resulting disk image is bootable and boots into the provided kernel image.

import sys
import os

# Log a message in stdout.
# @param msg: The message to log.
# @param endl: String to be appended to the message, by default this adds a new
# line to each message.
def cout(msg, endl="\n"):
    print(msg, end=endl)

# Log a message in stderr.
# @param msg: The message to log.
# @param endl: String to be appended to the message, by default this adds a new
# line to each message.
def cerr(msg, endl="\n"):
    sys.stderr.write(msg + endl)

# Check if a file exists on disk. If it does not exist, print an error message
# and exit with status 1, otherwise simply return to the caller.
# @param filePath: The path to test.
def assertFileExists(filePath):
    if not os.path.exists(filePath):
        cerr(filePath + ": file does not exist")
        sys.exit(1)

# Get the size of a file in units of bytes.
# @param filePath: Path to the file.
# @return: The size of `filePath` in bytes.
def fileSize(filePath):
    stats = os.stat(filePath)
    return stats.st_size

if __name__ == "__main__":
    if len(sys.argv) < 5:
        cerr("Not enough arguments")
        cerr("Usage:")
        cerr("  createDiskImage.py <stage 0 image> <stage 1 image> "
            +"<kernel ELF> <output file>")
        sys.exit(1)

    stage0ImagePath = sys.argv[1]
    stage1ImagePath = sys.argv[2]
    kernelImagePath = sys.argv[3]
    outputPath = sys.argv[4]

    assertFileExists(stage0ImagePath)
    assertFileExists(stage1ImagePath)
    assertFileExists(kernelImagePath)

    stage0Size = fileSize(stage0ImagePath)
    stage1Size = fileSize(stage1ImagePath)

    if stage0Size != 512:
        cerr("Stage 0's image must be of size 512 bytes")
        sys.exit(1)

    fd = open(outputPath, "wb")

    # First sector: stage 0's image.
    fd.write(open(stage0ImagePath, "rb").read())

    # Stage 1 follows stage 0 immediately.
    fd.write(open(stage1ImagePath, "rb").read())

    # Create the metadata sector containing information about the kernel image.
    metadataSec = b''
    # Magic number/signature for the metadata sector.
    metadataSec += 0x617461646174656d.to_bytes(8, byteorder="little")
    # LBA index of the first sector containing the kernel image.
    # +1 due to the metadata sector.
    kernelImageDiskOffset = stage0Size + stage1Size + 1
    kernelImageStartSec = (kernelImageDiskOffset // 512) + \
                          (kernelImageDiskOffset % 512)
    metadataSec += kernelImageStartSec.to_bytes(8, byteorder="little")
    # Size of the kernel image in bytes.
    kernelImageSize = fileSize(kernelImagePath)
    metadataSec += kernelImageSize.to_bytes(8, byteorder="little")
    # Add padding to get to a size of 512 bytes.
    if len(metadataSec) > 512:
        # This should literally never happen, at least not by accident.
        cerr("Metadata is bigger than a sector")
        sys.exit(1)
    metadataSec += 0x0.to_bytes(512 - len(metadataSec))
    assert len(metadataSec) == 512
    fd.write(metadataSec)

    # The kernel image, padded to have a size multiple of sector size.
    fd.write(open(kernelImagePath, "rb").read())
    paddingLen = 512 - (kernelImageSize % 512)
    fd.write(0x0.to_bytes(paddingLen))

    fd.close()

    # Make sure the disk image is a multiple of sector size.
    assert fileSize(outputPath) % 512 == 0

    sys.exit(0)
