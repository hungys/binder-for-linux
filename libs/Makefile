default: all

include cutils/Makefile
include utils/Makefile
include binder/Makefile

CFLAGS := -I.. -Iinclude -DHAVE_PTHREADS -DHAVE_SYS_UIO_H -DHAVE_ENDIAN_H -DHAVE_ANDROID_OS=1
CXXFLAGS := $(CFLAGS) -std=c++11

all: $(all_libs)

clean:
	rm -f $(all_libs) `find -name \*.o`

%.o : %.cpp
	gcc $(CXXFLAGS) -c -o $@ $<

%.o : %.c
	gcc $(CFLAGS) -c -o $@ $<

%.a:
	ar cr $@ $^
