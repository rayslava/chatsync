all: stylecheck doxygen asan tsan clang release clang-release analyzed

doxygen:
	doxygen Doxyfile

build-dir = \
	rm -rf $1-build && mkdir $1-build && cd $1-build

debug:
	$(call build-dir, $@) && cmake .. -DCMAKE_BUILD_TYPE=Debug && $(MAKE) -j $(JOBS) && ctest -j $(JOBS)

release:
	$(call build-dir, $@) && cmake .. -DCMAKE_BUILD_TYPE=Release && $(MAKE) -j $(JOBS) && ctest -j $(JOBS)

asan:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address" -DCMAKE_BUILD_TYPE=RelWithDebInfo && $(MAKE) chatsync

tsan:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=thread"  -DCMAKE_BUILD_TYPE=RelWithDebInfo && $(MAKE) chatsync

clang:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. && $(MAKE) chatsync

clang-release:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. -DCMAKE_BUILD_TYPE=Release && $(MAKE) chatsync

analyzed:
	$(call build-dir, $@) && scan-build cmake ..  && scan-build $(MAKE) chatsync

tidy:
	$(call build-dir, $@) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=True ..  && clang-tidy -p debug-build ../src/*.cpp

clean:
	rm -rf *-build

memcheck-binary = \
	(valgrind --tool=memcheck --track-origins=yes --leak-check=full --trace-children=yes --show-reachable=yes ./$1 2>/tmp/unit-test-valg-$1.log)</dev/null && sed '/in use/!d;s/.*exit:.*\s\([[:digit:]]\+\)\sblocks.*/\1/' /tmp/unit-test-valg-$1.log | { read lines; test $$lines -eq 1 || cat /tmp/unit-test-valg-$1.log; }

memcheck:
	$(call build-dir, $@) && cmake .. -DCMAKE_BUILD_TYPE=Debug && $(MAKE)

memcheck-config: memcheck
	cd memcheck-build && $(call memcheck-binary,config_tests)

memcheck-channel: memcheck
	cd memcheck-build && $(call memcheck-binary,channel_tests)

memcheck-hub: memcheck
	cd memcheck-build && $(call memcheck-binary,hub_tests)

memcheck-tox: memcheck
	cd memcheck-build && [ -e tox_tests ] && $(call memcheck-binary,tox_tests)

memcheck-unit: memcheck
	cd memcheck-build && $(call memcheck-binary,unit_tests)

valgrind: memcheck-tox memcheck-hub memcheck-channel memcheck-config

dockerimage:
	cd dockerbuild && (docker images | grep chatsync-deploy) || docker build -t chatsync-deploy .

chatsync.tar.gz: dockerimage
	git archive --format=tar.gz -o dockerbuild/chatsync.tar.gz --prefix=chatsync/ HEAD

stylecheck:
	uncrustify -c uncrustify.cfg src/*.cpp src/*.hpp test/*.cpp --check

stylefix:
	uncrustify -c uncrustify.cfg src/*.cpp src/*.hpp test/*.cpp --replace

deploy: debug release clang clang-release analyzed memcheck chatsync.tar.gz
	cd dockerbuild && docker run -v "$(shell pwd):/mnt/host" chatsync-deploy /bin/bash -c 'cd /root && tar xfz /root/chatsync.tar.gz && cd chatsync && mkdir build && cd build && cmake -DSTATIC=True -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && make chatsync && cp chatsync /mnt/host'
