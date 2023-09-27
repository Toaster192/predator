# --------------------------------------------------
# Base container
# --------------------------------------------------
FROM docker.io/ubuntu:focal AS base

RUN set -e

# Install packages
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -yq --no-install-recommends clang-10 llvm-dev llvm-10-dev \
						ca-certificates cmake git \
						make libboost-all-dev vim

# Can be used to specify which git ref to checkout
ARG GIT_REF=master
ARG GIT_REPO=kdudka/predator

# Install build dependencies

# Clone
RUN git clone https://github.com/$GIT_REPO
WORKDIR /predator

RUN git apply ./build-aux/llvm.patch

ENV CC=/bin/clang-10
ENV CXX=/bin/clang++-10
#ENV CC=/bin/clang
#ENV CXX=/bin/clang++

#RUN ./switch-host-gcc.sh /usr/bin/gcc
RUN make llvm
RUN make check
