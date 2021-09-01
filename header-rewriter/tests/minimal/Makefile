CC = clang
CLANG_VERSION = 12.0.1
HEADER_REWRITER = ../../build/ia2-header-rewriter
IA2_HEADER_PATH = ../../../include/
LDFLAGS = -shared

LIB_SRC = minimal.c
HEADER = minimal.h
ORIGINAL_LIB = libminimal.so

WRAPPER_SRC = wrapper.c
WRAPPER_LIB = libwrapper.so
VERSION_SCRIPT = $(WRAPPER_SRC).syms

SRC = main.c
BIN = main

$(BIN): $(WRAPPER_LIB) $(SRC)
	$(CC) $^ -Wl,-rpath=$(dir $(abspath $(WRAPPER_LIB))) -I$(IA2_HEADER_PATH) -o $@

$(WRAPPER_LIB): $(ORIGINAL_LIB) $(WRAPPER_SRC)
	$(CC) $(WRAPPER_SRC) $(LDFLAGS) -Wl,--version-script,$(VERSION_SCRIPT) -l$(basename $(LIB_SRC)) -L. -o $@

$(WRAPPER_SRC): $(HEADER)
	$(HEADER_REWRITER) $@ $^ -- -I /usr/lib/clang/$(CLANG_VERSION)/include

$(ORIGINAL_LIB): $(LIB_SRC)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(BIN) $(ORIGINAL_LIB) $(WRAPPER_LIB) $(WRAPPER_SRC) $(VERSION_SCRIPT)
	mv $(HEADER).orig $(HEADER)

