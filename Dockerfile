FROM ubuntu:18.04

RUN apt-get update && apt-get install -y \
  build-essential autogen autoconf git pkg-config \
  automake libtool curl make g++-8 unzip cmake libgflags-dev \
  && apt-get clean

RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 20

# install protobuf first, then grpc
ARG GOOGLE_TEST_RELEASE_TAG=release-1.8.1
ARG GRPC_RELEASE_TAG=v1.21.1

RUN git clone -b ${GRPC_RELEASE_TAG} --single-branch https://github.com/grpc/grpc /var/local/src/grpc && \
    cd /var/local/src/grpc && \
    git submodule update --init --recursive && \
    echo "--- installing protobuf ---" && \
    cd /var/local/src/grpc/third_party/protobuf && \
    ./autogen.sh && ./configure --enable-shared --disable-Werror && \
    make CXXFLAGS="-w" CFLAGS="-w" -j$(nproc) && make -j$(nproc) check && make install && make clean && ldconfig / && \
    echo "--- installing grpc ---" && \
    cd /var/local/src/grpc && \
    make CXXFLAGS="-w" CFLAGS="-w" -j$(nproc) && \
    make CXXFLAGS="-w" CFLAGS="-w" -j$(nproc) grpc_cli && \
    cp /var/local/src/grpc/bins/opt/grpc_cli /usr/local/bin/ && \
    make install && make clean && ldconfig / && \
    rm -rf /var/local/src/grpc

# Install googletest
RUN    echo "--- installing Google test ---" && \
    git clone -b ${GOOGLE_TEST_RELEASE_TAG} --single-branch  https://github.com/google/googletest.git /var/local/src/gtest && \
    cd /var/local/src/gtest && \
    git submodule update --init --recursive && \
    mkdir _bld && cd _bld && \
    cmake .. && make -j$(nproc) && make install


