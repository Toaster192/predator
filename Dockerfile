# --------------------------------------------------
# Base container
# --------------------------------------------------
FROM docker.io/ubuntu:22.04 AS base

RUN set -e

ENV GCC_HOST_VERSION=11
ENV LLVM_HOST_VERSION=14

# Install packages
ENV DEBIAN_FRONTEND=noninteractive
ENV CFLAGS="-Werror"
ENV CXXFLAGS="-Werror"

RUN apt-get update
RUN apt install -y clang-$LLVM_HOST_VERSION
RUN apt-get update
RUN apt install -y gcc-$GCC_HOST_VERSION-multilib libboost-dev
RUN apt-get update
RUN apt install -y clang-$LLVM_HOST_VERSION llvm-$LLVM_HOST_VERSION-dev
RUN apt-get update
RUN apt install -y git make cmake
#ENV GCC_HOST=/usr/bin/gcc-$GCC_HOST_VERSION

# Can be used to specify which git ref to checkout
ARG GIT_REF=master
ARG GIT_REPO=kdudka/predator
ARG PASSES_GIT_REF=master
ARG PASSES_GIT_REPO=VeriFIT/ProStatA

# Clone
RUN git clone https://github.com/$GIT_REPO
WORKDIR /predator

RUN git clone https://github.com/$PASSES_GIT_REPO ./passes-src/


ENV CC="/usr/bin/clang-$LLVM_HOST_VERSION"
ENV CXX="/usr/bin/clang++-$LLVM_HOST_VERSION"

# Use ASAN and UBSAN
ENV CFLAGS="$CFLAGS -fsanitize=address,undefined"
ENV CXXFLAGS="$CXXFLAGS -fsanitize=address,undefined"

# Recommended for better error traces
ENV CFLAGS="$CFLAGS -fno-omit-frame-pointer"
ENV CXXFLAGS="$CXXFLAGS -fno-omit-frame-pointer"

# Make UBSAN reports fatal
ENV CFLAGS="$CFLAGS -fno-sanitize-recover=all"
ENV CXXFLAGS="$CXXFLAGS -fno-sanitize-recover=all"

# Use shared libasan for sanitization of shared libraries loaded
# by non-instrumented binaries.
# For details see https://systemd.io/TESTING_WITH_SANITIZERS/#clang.
ENV CFLAGS="$CFLAGS -shared-libasan"
ENV CXXFLAGS="$CXXFLAGS -shared-libasan"

# Needed for testing of shared libraries loaded by non-instrumented
# binaries.

# Hardcoding as setting ENV to the result of a command doesn't work
# and RUN export doesn't work for the entire file
# https://github.com/docker/docker/issues/29110
#ENV LD_PRELOAD="/usr/lib/llvm-14/lib/clang/14.0.0/lib/linux/libclang_rt.asan-x86_64.so"
#ENV LD_PRELOAD="$(clang-$LLVM_HOST_VERSION -print-file-name=libclang_rt.asan-x86_64.so)"

# LD_PRELOAD workaround? With it set everthing crashes (even git clones etc. which is super weird)
RUN cp /usr/lib/llvm-14/lib/clang/14.0.0/lib/linux/libclang_rt.asan-x86_64.so /usr/lib/

# Due to LD_PRELOAD above, leak sanitizer was reporting leaks
# literally in everything that was executed, e.g. make, shell,
# python and other tools that are not under our control.
ENV ASAN_OPTIONS="detect_leaks=0"

# Do not detect ODR violations as we can't easily fix this problem in
# CL's compile-self-02-var-killer test.
ENV ASAN_OPTIONS="$ASAN_OPTIONS,detect_odr_violation=0"

# Make UBSAN print whole stack traces
ENV UBSAN_OPTIONS="print_stacktrace=1"

ENV LLVM_CMAKE_FLAGS="-D LLVM_DIR='/usr/lib/llvm-$LLVM_HOST_VERSION/cmake'"
ENV LLVM_CMAKE_FLAGS="$LLVM_CMAKE_FLAGS -D ENABLE_LLVM=ON"

# FIXME: Passes seem to fail somehow - maybe fork it and add better debug printouts?
RUN make build_passes -s -j$(nproc) "CMAKE=cmake $LLVM_CMAKE_FLAGS"

RUN make -C cl -s -j$(nproc) "CMAKE=cmake -D CL_DEBUG=ON $LLVM_CMAKE_FLAGS"
RUN patch -p1 < build-aux/llvm.patch

RUN make -C sl -s -j$(nproc) "CMAKE=cmake -D SL_DEBUG=ON $LLVM_CMAKE_FLAGS" check

RUN ./switch-host-llvm.sh "/usr/lib/llvm-$LLVM_HOST_VERSION/cmake"
