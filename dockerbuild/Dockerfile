FROM besn0847/ubuntu32
MAINTAINER rayslava@gmail.com

RUN apt-get -qq dist-upgrade -y
RUN apt-get -qq update -y
RUN apt-get -qq --yes --force-yes install python-software-properties
RUN add-apt-repository --yes ppa:ubuntu-toolchain-r/test
RUN add-apt-repository --yes ppa:george-edison55/precise-backports
RUN apt-get -qq update -y
# Setting up build environment
RUN apt-get -qq --yes --force-yes install gcc-5 g++-5 libgtest-dev valgrind lcov cmake cmake-data curl build-essential libtool autotools-dev automake checkinstall check git yasm pkg-config libncurses5-dev libgmp-dev
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 40 --slave /usr/bin/g++ g++ /usr/bin/g++-5
RUN update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-5 40
RUN update-alternatives --config gcc
RUN update-alternatives --config gcov
RUN cd $(mktemp -d) && cmake /usr/src/gtest &&  make -j4 && sudo mv libgtest.a libgtest_main.a /usr/lib/
# Building rapidjson
RUN cd $(mktemp -d) && git clone git://github.com/miloyip/rapidjson.git && cd rapidjson && mkdir build && cd build && cmake .. && make && checkinstall --install --pkgname rapidjson --pkgversion 1.1.0 && sudo ldconfig
# Building nettle
RUN cd $(mktemp -d) && curl -L 'https://ftp.gnu.org/gnu/nettle/nettle-3.3.tar.gz' | tar xfz - && cd nettle* && mkdir build && cd build && ../configure --enable-static && make && checkinstall --install --pkgname nettle --pkgversion 3.3 --nodoc && ldconfig
# Building gnutls
RUN cd $(mktemp -d) && curl -L 'https://www.gnupg.org/ftp/gcrypt/gnutls/v3.5/gnutls-3.5.13.tar.xz' | tar xfJ - && cd gnutls* && mkdir build && cd build && ../configure --disable-shared --enable-static --with-included-libtasn1 --with-included-unistring --without-p11-kit  --disable-hardware-acceleration --disable-padlock && make && checkinstall --install --pkgname gnutls --pkgversion 3.5.13 --nodoc && ldconfig
# Building sodium
RUN cd $(mktemp -d) && git clone git://github.com/jedisct1/libsodium.git && cd libsodium &&  git checkout tags/1.0.3 &&  ./autogen.sh &&  ./configure --enable-static && make check && checkinstall --install --pkgname libsodium --pkgversion 1.0.3 --nodoc && ldconfig
# Building tox

# For c-toxcore pass TokTok/c-toxcore
ARG TOXSRC
ENV TOXSRC ${TOXSRC:-"irungentoo/toxcore"}
RUN cd $(mktemp -d) && git clone git://github.com/${TOXSRC}.git toxcore && cd toxcore &&  autoreconf -i && ./configure --enable-static && make && checkinstall --install --pkgname libtoxcore --pkgversion 1.0.0 --nodoc && ldconfig
