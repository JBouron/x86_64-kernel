FROM ubuntu:23.04

RUN apt-get -y update && apt-get -y upgrade

# Install build dependencies.
RUN apt install -y build-essential bison flex libgmp3-dev libmpc-dev \
    libmpfr-dev texinfo curl nasm vim

# Download GCC and Binutils sources.
RUN mkdir /build
WORKDIR /build
# GCC
RUN curl -LO https://ftp.gnu.org/gnu/gcc/gcc-13.1.0/gcc-13.1.0.tar.xz
RUN mkdir gcc-sources && \
    tar xvf gcc-13.1.0.tar.xz -C gcc-sources --strip-components=1
# Binutils
RUN curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.40.tar.xz
RUN mkdir binutils-sources && \
    tar xvf binutils-2.40.tar.xz -C binutils-sources --strip-components=1

# Copy the build script and run it to build and install the cross-compiler and
# binutils.
COPY buildGccAndBinutils.sh /build/
RUN /build/buildGccAndBinutils.sh

# Delete /build to reduce the image's size.
RUN rm -rf /build

# Setup PATH to contains the bin dir of the cross-compiler.
ENV PATH="${PATH}:/usr/local/cross/bin"
