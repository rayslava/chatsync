all: doxygen asan tsan clang release clang-release analyzed

doxygen:
	doxygen Doxyfile

build-dir = \
	rm -rf $1-build && mkdir $1-build && cd $1-build

release:
	$(call build-dir, $@) && cmake .. -DCMAKE_BUILD_TYPE=Release && $(MAKE) chatsync

asan:
	$(call build-dir, $@) && CXXFLAGS="-fsanitize=address" CXX=clang++ CC=clang cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo && $(MAKE) chatsync

tsan:
	$(call build-dir, $@) && CXXFLAGS="-fsanitize=thread" CXX=clang++ CC=clang cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo && $(MAKE) chatsync

clang:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. && $(MAKE) chatsync

clang-release:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. -DCMAKE_BUILD_TYPE=Release && $(MAKE) chatsync

analyzed:
	$(call build-dir, $@) && scan-build cmake ..  && scan-build $(MAKE) all

clean:
	rm -rf *-build
