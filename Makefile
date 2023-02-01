# 二进制

build/server: *.cpp pages/*.html
	g++ main.cpp -o ./build/server -lhttplib -ljson11 -g

build/server-linux-x64: *.cpp pages/*.html
	g++ main.cpp ./third_party/*.cpp -o ./build/server-linux-x64 -Ithird_party -O2 -s -static

build/server-windows-x86.exe: *.cpp pages/*.html
	i686-w64-mingw32-g++ main.cpp ./third_party/*.cpp -o ./build/server-windows-x86.exe -Ithird_party -lws2_32 -O2 -s -static

build/server-windows-x64.exe: *.cpp pages/*.html
	x86_64-w64-mingw32-g++ main.cpp ./third_party/*.cpp -o ./build/server-windows-x64.exe -Ithird_party -lws2_32 -O2 -s -static

# 伪指令

.PHONY: clean
clean:
	rm -r ./build/
	mkdir ./build/

.PHONY: run
run: build/server
	sudo ./build/server

.PHONY: pack
pack: build/server-linux-x64 build/server-windows-x86.exe build/server-windows-x64.exe
	cp -r ./pages/ ./build/pages
	cd ./build/ && zip -q -r ./server-linux-x64.zip ./server-linux-x64 ./pages/
	cd ./build/ && zip -q -r ./server-windows-x86.zip ./server-windows-x86.exe ./pages/
	cd ./build/ && zip -q -r ./server-windows-x64.zip ./server-windows-x64.exe ./pages/