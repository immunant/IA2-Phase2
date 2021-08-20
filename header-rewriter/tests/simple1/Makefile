CFLAGS = -O2 -g

.PHONY: all clean

all: main libsimple1.so

clean:
	rm main.o main simple1.o libsimple1.so

libsimple1.so: simple1.o
	$(LD) $(CFLAGS) -shared -o $@ $^

main: LDLIBS += -lsimple1
main: LDFLAGS += -L. -Wl,-rpath,$(shell pwd)
main: main.o | libsimple1.so
