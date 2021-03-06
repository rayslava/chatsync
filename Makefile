BINARY=chatsync
JOBS=$(shell grep -c '^processor' /proc/cpuinfo)
VERBOSE=1
MAKE=cmake --build . -- -j $(JOBS)
all: stylecheck doxygen asan clang release clang-release analyzed

doxygen:
	doxygen Doxyfile

build-dir = \
	rm -rf $1-build && mkdir $1-build && cd $1-build

debug:
	$(call build-dir, $@) && cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=True -DCMAKE_BUILD_TYPE=Debug && $(MAKE) -j $(JOBS) && ctest -j 1

release: debug
	$(call build-dir, $@) && cmake .. -DCMAKE_BUILD_TYPE=Release && $(MAKE) && ctest -j 1

static-release:
	$(call build-dir, $@) && cmake .. -DCMAKE_BUILD_TYPE=Release -DSTATIC=True && $(MAKE) $(BINARY) -j $(JOBS)

asan:
	$(call build-dir, $@) && cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address" -DCMAKE_BUILD_TYPE=RelWithDebInfo && $(MAKE) && ctest -j 1 -R unit

tsan:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=thread"  -DCMAKE_BUILD_TYPE=Debug && $(MAKE) && ctest -j 1 -R unit

msan:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=memory -fsanitize-memory-track-origins=2 -fsanitize-memory-use-after-dtor -fno-omit-frame-pointer -fno-optimize-sibling-calls -O0 -fsanitize-blacklist=$(shell pwd)/scripts/msan_blacklist.txt -stdlib=libc++ -L/home/v.barinov/work/btrwork/soft/llvm-build/lib -lc++abi -I/home/v.barinov/work/btrwork/soft/llvm-build/include" -DGTEST_BOTH_LIBRARIES="/home/v.barinov/work/btrwork/soft/gtest-build/libgtest_main.so /home/v.barinov/work/btrwork/soft/gtest-build/libgtest.so" -DCMAKE_BUILD_TYPE=Debug && $(MAKE) -j $(JOBS) VERBOSE=1 && MSAN_OPTIONS=poison_in_dtor=1 ./unit_tests

lsan:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=leak"  -DCMAKE_BUILD_TYPE=RelWithDebInfo && $(MAKE)

clang:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. && $(MAKE) $(BINARY)

clang-release:
	$(call build-dir, $@) && CXX=clang++ CC=clang cmake .. -DCMAKE_BUILD_TYPE=Release && $(MAKE) $(BINARY) -j $(JOBS)

analyzed:
	$(call build-dir, $@) && scan-build cmake ..  && scan-build $(MAKE) $(BINARY)

tidy:
	$(call build-dir, $@) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=True ..  && clang-tidy -p debug-build ../src/*.cpp

clean:
	rm -rf *-build doc; find -name '*~' -delete; rm -f dockerbuild/$(BINARY).tar.gz; rm -f dockerbuild/$(BINARY)

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

memcheck-logging: memcheck
	cd memcheck-build && $(call memcheck-binary,logging_tests)

memcheck-unit: memcheck
	cd memcheck-build && $(call memcheck-binary,unit_tests)

valgrind: memcheck-tox memcheck-hub memcheck-channel memcheck-config

dockerimage:
	cd dockerbuild && (docker images | grep $(BINARY)-deploy) || docker build -t $(BINARY)-deploy .
	cd dockerbuild && (docker images | grep $(BINARY)-ctoxcore-deploy) || docker build --build-arg TOXSRC="TokTok/c-toxcore" -t $(BINARY)-ctoxcore-deploy .

dockerimage-clean:
	docker rm $(shell docker ps -a -q) || true
	docker rmi $(shell docker images -a --filter=dangling=true -q) || true
	docker rmi $(BINARY)-deploy || true
	docker rmi $(BINARY)-ctoxcore-deploy || true

dockerbuild/$(BINARY).tar.gz:
	rm -f dockerbuild/$(BINARY).tar.gz
	git archive --format=tar.gz -o dockerbuild/$(BINARY).tar.gz --prefix=$(BINARY)/ HEAD

stylecheck:
	uncrustify -c uncrustify.cfg src/*.cpp src/*.hpp test/*.cpp --check

stylefix:
	uncrustify -c uncrustify.cfg src/*.cpp src/*.hpp test/*.cpp --replace

full-deploy: debug release clang clang-release static-release analyzed memcheck deploy deploy-ctoxcore

deploy: dockerbuild/$(BINARY).tar.gz
	cd dockerbuild && docker run -v "$(shell pwd)/dockerbuild:/mnt/host" $(BINARY)-deploy /bin/bash -c 'cd /root && tar xfz /mnt/host/$(BINARY).tar.gz && cd $(BINARY) && mkdir build && cd build && cmake -DSTATIC=True -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && make $(BINARY) -j $(JOBS) && cp $(BINARY) /mnt/host/$(BINARY)'
	rm dockerbuild/$(BINARY).tar.gz

deploy-ctoxcore: dockerbuild/$(BINARY).tar.gz dockerimage
	cd dockerbuild && docker run -v "$(shell pwd)/dockerbuild:/mnt/host" $(BINARY)-ctoxcore-deploy /bin/bash -c 'cd /root && tar xfz /mnt/host/$(BINARY).tar.gz && cd $(BINARY) && mkdir build && cd build && cmake -DSTATIC=True -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && make $(BINARY) -j $(JOBS) && cp $(BINARY) /mnt/host/$(BINARY)-ctoxcore'
	rm dockerbuild/$(BINARY).tar.gz

coverage:
	$(call build-dir, $@) &&  cmake .. -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=True && $(MAKE) coverage

.PHONY: dockerbuild/$(BINARY).tar.gz
