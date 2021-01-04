FROM ubuntu:20.04 as builder
ENV DEBIAN_FRONTEND noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN true
RUN echo 'tzdata tzdata/Areas select Etc' | debconf-set-selections; \
    echo 'tzdata tzdata/Zones/Etc select UTC' | debconf-set-selections; \
    apt-get update && apt-get upgrade -y && apt-get install --no-install-recommends -y \
    cmake \
    ninja-build \
    gcc \
    g++ \
    maven \
    flex \
    bison \
    libxml2-utils \
    dpkg-dev
COPY . /tmp/cbmc
WORKDIR /tmp/cbmc
RUN cmake -S . -Bbuild -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ && cd build; ninja -j2

FROM ubuntu:20.04 as runner
COPY --from=builder /tmp/cbmc/build/bin/* /usr/local/bin/
WORKDIR /tmp/cbmc/
RUN apt-get update && apt-get install -y gcc
