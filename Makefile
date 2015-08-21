all: doxygen asan tsan clang release clang-release analyzed

doxygen:
	doxygen Doxyfile

build-dir = \
	rm -rf $1-build && mkdir $1-build && cd $1-build

debug:
	$(call build-dir, $@) && cmake .. -DCMAKE_BUILD_TYPE=Debug && $(MAKE)

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
	$(call build-dir, $@) && scan-build cmake ..  && scan-build $(MAKE) chatsync

clean:
	rm -rf *-build

memcheck:
	$(call build-dir, $@) && cmake .. -DCMAKE_BUILD_TYPE=Debug && $(MAKE) &&  (valgrind --tool=memcheck --track-origins=yes --leak-check=full --trace-children=yes --db-attach=yes --show-reachable=yes ./unit_tests 2>/tmp/unit-test-valg.log)</dev/null && sed '/in use/!d;s/.*exit:\s\([[:digit:]]\+\)\sbytes.*/\1/' /tmp/unit-test-valg.log | sort -n | uniq


dockerimage:
	cd dockerbuild && (docker images | grep chatsync-deploy) || docker build -t chatsync-deploy .

chatsync.tar.gz: dockerimage
	git archive --format=tar.gz -o dockerbuild/chatsync.tar.gz --prefix=chatsync/ HEAD

deploy: debug release clang clang-release analyzed memcheck chatsync.tar.gz
	cd dockerbuild && docker run -v "$(shell pwd):/mnt/host" chatsync-deploy /bin/bash -c 'cd /root && tar xfz /root/chatsync.tar.gz && cd chatsync && mkdir build && cd build && cmake -DSTATIC=True -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && make chatsync && cp chatsync /mnt/host'
