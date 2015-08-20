FROM besn0847/ubuntu32
MAINTAINER rayslava@gmail.com

RUN apt-get -qq dist-upgrade -y
RUN apt-get -qq update -y
RUN apt-get -qq --yes --force-yes install python-software-properties
RUN add-apt-repository --yes ppa:ubuntu-toolchain-r/test
RUN add-apt-repository --yes ppa:yjwong/cmake
RUN apt-get -qq update -y
# Setting up build environment
RUN apt-get -qq --yes --force-yes install gcc-5 g++-5 libgtest-dev valgrind lcov cmake cmake-data curl build-essential libtool autotools-dev automake checkinstall check git yasm
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 40 --slave /usr/bin/g++ g++ /usr/bin/g++-5
RUN update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-5 40
RUN update-alternatives --config gcc
RUN update-alternatives --config gcov
RUN cd $(mktemp -d) && cmake /usr/src/gtest &&  make -j4 && sudo mv libgtest.a libgtest_main.a /usr/lib/
# Building sodium
RUN cd $(mktemp -d) && git clone git://github.com/jedisct1/libsodium.git && cd libsodium &&  git checkout tags/1.0.3 &&  ./autogen.sh &&  ./configure --enable-static && make check &&  checkinstall --install --pkgname libsodium --pkgversion 1.0.3 --nodoc && ldconfig
# Building tox
RUN cd $(mktemp -d) && git clone git://github.com/irungentoo/toxcore.git && cd toxcore &&  autoreconf -i && ./configure --enable-static && make && checkinstall --install --pkgname libtoxcore --pkgversion 1.0.0 --nodoc && ldconfig
# Now release is preparing
COPY chatsync.tar.gz /root/chatsync.tar.gz
