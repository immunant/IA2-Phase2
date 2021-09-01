HEADER_REWRITER = ../../build1/ia2-header-rewriter
CFLAGS = -O2 -g -Werror=incompatible-pointer-types -I../../../include

.PHONY: all clean

all: main libsimple1.so

clean:
	rm -f main.o main simple1.o libsimple1.so simple1_ia2.h
	git checkout -- simple1_public.h

simple1_ia2.h: simple1_public.h
	$(HEADER_REWRITER) --output-header $@ $^ --

main.o: main.c | simple1_public.h simple1_ia2.h

libsimple1.so: simple1.o | simple1_priv.h
	$(CC) $(LDFLAGS) -shared -o $@ $^

main: LDLIBS += -lsimple1
main: LDFLAGS += -L. -Wl,-rpath,$(shell pwd)
main: main.o | libsimple1.so
