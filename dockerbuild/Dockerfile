FROM ubuntu:16.04
MAINTAINER rayslava@gmail.com
ARG DOXYGEN_VER
ARG UNCRUSTIFY_VER
ARG NETTLE_VER
ARG GNUTLS_VER
ARG LIBSODIUM_VER

RUN apt-get update -y
RUN apt-get --yes --force-yes install software-properties-common python-software-properties
RUN add-apt-repository --yes ppa:ubuntu-toolchain-r/test
RUN apt-get --yes --force-yes install curl
RUN add-apt-repository --yes 'deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-6.0 main'
RUN curl https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN apt-get update -y
# Setting up build environment
RUN apt-get --yes --force-yes install libclang-9.0-dev clang-9.0 gcc-9 g++-9 valgrind lcov cmake cmake-data build-essential libtool autotools-dev automake checkinstall check git yasm libgnutls-dev libgmp-dev flex bison python-pip
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 40 --slave /usr/bin/g++ g++ /usr/bin/g++-9
RUN update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-9 40
RUN update-alternatives --config gcc
RUN update-alternatives --config gcov
# Setting up libcxx
RUN cd $(mktemp -d) && git clone https://github.com/llvm-mirror/libcxx.git && mkdir -p libcxx/build && cd libcxx/build && export LD_LIBRARY_PATH=/usr/lib/llvm-6.0/lib/ && export PATH=/usr/lib/llvm-6.0/bin:$PATH && cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && make && checkinstall --install --pkgname libcxx --pkgversion 1.0 --nodoc < /dev/null && ldconfig
RUN cd $(mktemp -d) && git clone https://github.com/llvm-mirror/libcxxabi.git && mkdir -p libcxxabi/build && cd libcxxabi/build && export LD_LIBRARY_PATH=/usr/lib/llvm-6.0/lib/ && export PATH=/usr/lib/llvm-6.0/bin:$PATH && cmake .. -DLIBCXXABI_LIBCXX_INCLUDES=/usr/local/include/c++/v1 -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && make && checkinstall --install --pkgname libcxxabi --pkgversion 1.0 --nodoc < /dev/null && ldconfig
# Setting up tools
RUN pip install cpp-coveralls
RUN cd $(mktemp -d) && git clone https://github.com/google/googletest.git && cd googletest && mkdir build && cd build && cmake -DCMAKE_INSTALL_PREFIX=/usr .. && make VERBOSE=1 && make VERBOSE=1 install && ldconfig
ENV DOXYGEN_VER=${DOXYGEN_VER:-"1.8.14"}
RUN cd $(mktemp -d) && curl -L "http://ftp.stack.nl/pub/users/dimitri/doxygen-${DOXYGEN_VER}.src.tar.gz" | tar xfz - && cd doxygen-${DOXYGEN_VER} && mkdir build && cd build && cmake -G "Unix Makefiles" .. && make -j4 && checkinstall --install --pkgname doxygen --pkgversion ${DOXYGEN_VER} --nodoc < /dev/null && ldconfig
ENV UNCRUSTIFY_VER=${UNCRUSTIFY_VER:-"0.66.1"}
RUN cd $(mktemp -d) && curl -L https://github.com/uncrustify/uncrustify/archive/uncrustify-${UNCRUSTIFY_VER}.tar.gz | tar xfz - && cd uncrustify-uncrustify-${UNCRUSTIFY_VER} && mkdir build && cd build && cmake .. && make && checkinstall --install --pkgname uncrustify --pkgversion ${UNCRUSTIFY_VER} --nodoc < /dev/null && ldconfig
# Building rapidjson
RUN cd $(mktemp -d) && git clone https://github.com/Tencent/rapidjson.git && cd rapidjson && mkdir build && cd build && cmake .. && make && checkinstall --install --pkgname rapidjson --pkgversion 1.1.0 && sudo ldconfig
# Building nettle
ARG NETTLE_VER
ENV NETTLE_VER=${NETTLE_VER:-"3.5.1"}
RUN cd $(mktemp -d) && curl -L "https://ftp.gnu.org/gnu/nettle/nettle-${NETTLE_VER}.tar.gz" | tar xfz - && cd nettle* && mkdir build && cd build && ../configure --enable-static && make && checkinstall --install --pkgname nettle --pkgversion ${NETTLE_VER} --nodoc && ldconfig
RUN apt-get --yes --force-yes install autogen
# Building gnutls
ARG GNUTLS_VER
ENV GNUTLS_VER=${GNUTLS_VER:-"3.6.9"}
RUN cd $(mktemp -d) && curl -L "https://www.gnupg.org/ftp/gcrypt/gnutls/v3.6/gnutls-${GNUTLS_VER}.tar.xz" | tar xfJ - && cd gnutls* && mkdir build && cd build && ../configure --disable-shared --enable-static --with-included-libtasn1 --with-included-unistring --without-p11-kit  --disable-hardware-acceleration --disable-padlock && make && checkinstall --install --pkgname gnutls --pkgversion ${GNUTLS_VER} --nodoc && ldconfig
# Building sodium
ARG LIBSODIUM_VER
ENV LIBSODIUM_VER=${LIBSODIUM_VER:-"1.0.18"}
RUN cd $(mktemp -d) && git clone https://github.com/jedisct1/libsodium.git && cd libsodium &&  git checkout tags/${LIBSODIUM_VER} &&  ./autogen.sh &&  ./configure --enable-static && make check && checkinstall --install --pkgname libsodium --pkgversion ${LIBSODIUM_VER} --nodoc && ldconfig
# Building tox

# For c-toxcore pass
ARG TOXSRC
ENV TOXSRC ${TOXSRC:-"TokTok/c-toxcore"}
RUN cd $(mktemp -d) && git clone https://github.com/${TOXSRC}.git c-toxcore && cd c-toxcore &&  autoreconf -i && ./configure --enable-static && make && checkinstall --install --pkgname libtoxcore --pkgversion 1.0.0 --nodoc && ldconfig

RUN rm -rf /tmp/*
