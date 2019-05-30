FROM ubuntu:18.10

RUN apt-get update && apt-get install -y \
  build-essential autogen autoconf git pkg-config \
  automake libtool curl make g++-8 unzip cmake libgflags-dev valgrind lcov \
  && apt-get clean

RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 20

ARG GOOGLE_TEST_RELEASE_TAG=release-1.8.1
ARG PROTOBUF_RELEASE_TAG=v3.8.0
ARG GRPC_RELEASE_TAG=v1.21.1

# Install googletest
RUN    echo "--- installing Google test ---" && \
    git clone -b ${GOOGLE_TEST_RELEASE_TAG} --single-branch  https://github.com/google/googletest.git /var/local/src/gtest && \
    cd /var/local/src/gtest && \
    git submodule update --init --recursive && \
    mkdir _bld && cd _bld && \
    cmake .. && make -j$(nproc) && make install && \
    rm -rf /var/local/src/gtest



RUN git clone -b ${PROTOBUF_RELEASE_TAG} --single-branch https://github.com/protocolbuffers/protobuf.git /var/local/src/protobuf && \
    cd /var/local/src/protobuf && \
    echo "--- installing protobuf ---" && \
    git submodule update --init --recursive && \
    ./autogen.sh && ./configure --enable-shared --disable-Werror && \
    make CXXFLAGS="-w" CFLAGS="-w" -j$(nproc) && make -j$(nproc) check && make install && make clean && ldconfig  && \
    rm -rf /var/local/src/protobuf
    

# install protobuf first, then grpc
RUN git clone -b ${GRPC_RELEASE_TAG} --single-branch https://github.com/grpc/grpc /var/local/src/grpc && \
    cd /var/local/src/grpc && \
    echo "--- installing grpc ---" && \
    cd /var/local/src/grpc && \
    git submodule update --init && \
    make CXXFLAGS="-w" CFLAGS="-w" -j$(nproc) && \
    make install && make clean && ldconfig && \
    rm -rf /var/local/src/grpc




