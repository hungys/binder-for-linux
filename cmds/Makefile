all: service

# https://stackoverflow.com/questions/1662909/undefined-reference-to-pthread-create-in-linux
service: service.cpp
	g++ -std=c++11 -o $@ -I../libs/include -I.. -L../libs -DHAVE_PTHREADS -DHAVE_SYS_UIO_H -DHAVE_ENDIAN_H -DHAVE_ANDROID_OS=1 $< -lpthread -lbinder -lutils -lcutils

clean:
	rm -f service
