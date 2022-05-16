NGINX_ROOT = $(dir $(abspath $(lastword ($MAKEFILE_LIST))))

# The root of the IA2-Phase2 repo
REPO_ROOT = $(abspath $(NGINX_ROOT)/../..)
IA2_INC = $(REPO_ROOT)/include/
IA2_LIB = $(REPO_ROOT)/libia2/

HEADER_REWRITER ?= $(REPO_ROOT)/build/header-rewriter/ia2-header-rewriter

BUILD_DIR ?= $(NGINX_ROOT)/build

PERL_SCRIPT_DIR = $(BUILD_DIR)/perl/lib
PERL_SCRIPT = $(NGINX_ROOT)/src/http/modules/perl/hello.pm

C_SYSTEM_INCLUDE = $(shell $(CC) -print-file-name=include)
C_SYSTEM_INCLUDE_FIXED = $(shell $(CC) -print-file-name=include-fixed)

# This directory is for the main binary's shim which the module must link
# against.
MAIN_SHIM_SRC = $(BUILD_DIR)/main_shim_src

# This directory is for the module's shim. The main binary doesn't call the
# module directly, but we need to generate an output header.
MOD_SHIM_SRC = $(BUILD_DIR)/mod_shim_src

ia2_all: main_shim mod_shim_src

main_shim_src:
	rm -rf $(MAIN_SHIM_SRC)
	cp -r $(NGINX_ROOT)/src/ $(MAIN_SHIM_SRC)
	mkdir -p $(PERL_SCRIPT_DIR)
	cp -r $(PERL_SCRIPT) $(PERL_SCRIPT_DIR)
	$(HEADER_REWRITER) --compartment-pkey=1 \
		$(MAIN_SHIM_SRC)/main_shim.c \
		$(MAIN_SHIM_SRC)/core/ngx_config.h \
		$(MAIN_SHIM_SRC)/core/ngx_core.h \
		$(MAIN_SHIM_SRC)/http/ngx_http.h \
		--shared-headers \
		$(MAIN_SHIM_SRC)/http/modules/perl/ngx_http_perl_module.h \
		--output-header \
		$(MAIN_SHIM_SRC)/main_shim_fn_ptr.h \
		-- \
		-fgnuc-version=6 \
		-I $(MAIN_SHIM_SRC)/core \
		-I $(MAIN_SHIM_SRC)/event \
		-I $(MAIN_SHIM_SRC)/event/modules \
		-I $(MAIN_SHIM_SRC)/os/unix \
		-I $(MAIN_SHIM_SRC)/http/modules/perl \
		-I $(MAIN_SHIM_SRC)/http/modules \
		-I $(MAIN_SHIM_SRC)/http \
		-I $(BUILD_DIR) \
		-isystem $(C_SYSTEM_INCLUDE) \
		-isystem $(C_SYSTEM_INCLUDE_FIXED) \

main_shim: main_shim_src
	gcc -shared $(MAIN_SHIM_SRC)/main_shim.c -Wl,-z,now \
		-Wl,--version-script,$(MAIN_SHIM_SRC)/main_shim.c.syms \
		-DCALLER_PKEY=2 -I $(IA2_INC) \
		-o $(BUILD_DIR)/libmain_shim.so

mod_shim_src:
	rm -rf $(MOD_SHIM_SRC)
	cp -r $(NGINX_ROOT)/src/ $(MOD_SHIM_SRC)
	$(HEADER_REWRITER) \
		$(MOD_SHIM_SRC)/core/ngx_config.h \
		$(MOD_SHIM_SRC)/core/ngx_core.h \
		$(MOD_SHIM_SRC)/core/ngx_conf_file.h \
		$(MOD_SHIM_SRC)/http/ngx_http.h \
		$(MOD_SHIM_SRC)/event/ngx_event.h \
		--omit-wrappers \
		--output-header \
		$(MOD_SHIM_SRC)/mod_shim_fn_ptr.h \
		-- \
		-fgnuc-version=6 \
		-I $(MOD_SHIM_SRC)/core \
		-I $(MOD_SHIM_SRC)/event \
		-I $(MOD_SHIM_SRC)/event/modules \
		-I $(MOD_SHIM_SRC)/os/unix \
		-I $(MOD_SHIM_SRC)/http/modules/perl \
		-I $(MOD_SHIM_SRC)/http/modules \
		-I $(MOD_SHIM_SRC)/http \
		-I $(BUILD_DIR) \
		-isystem $(C_SYSTEM_INCLUDE) \
		-isystem $(C_SYSTEM_INCLUDE_FIXED) \

