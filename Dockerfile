FROM ubuntu:latest AS builder

# Prevent interactive tool from blocking package installations
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y build-essential cmake git && \
    apt-get install -y libboost-all-dev protobuf-compiler

WORKDIR /git
RUN git clone --recurse-submodules -b v1.41.1 https://github.com/grpc/grpc /git/grpc
# Run script to build grpc from source
RUN cd /git/grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DBUILD_DEPS=ON -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF ../.. && \
    make -j8 && \
    make install

COPY . /usr/src
WORKDIR /usr/src

RUN cmake -S . -B build
RUN cmake --build build

FROM ubuntu:latest AS runtime

COPY --from=builder /usr/src/build/bin /usr/src/app
WORKDIR /usr/src/app
RUN ldconfig
