
# Copyright (C) Igor Sysoev
# Copyright (C) Nginx, Inc.

use 5.006001;
use ExtUtils::MakeMaker;

$fixed_include = $ENV{NGX_INCS};
$fixed_include =~ s/src/main_shim_src/gm;
# for ia2.h
$fixed_include .= " ../../../libia2/include";
$BUILD_DIR = "../../../..";

WriteMakefile(
    NAME              => 'nginx',
    VERSION_FROM      => 'nginx.pm',     # finds $VERSION
    PREREQ_PM         => {},             # e.g., Module::Name => 1.1

    ABSTRACT_FROM     => 'nginx.pm',     # retrieve abstract from module
    AUTHOR            => 'Igor Sysoev',

    CC                => "clang",
    LD                => "clang",
    CCFLAGS           => "$ENV{NGX_PM_CFLAGS}",
    OPTIMIZE          => '-O',

    LDDLFLAGS         => "$ENV{NGX_PM_LDFLAGS} -L${BUILD_DIR} ${BUILD_DIR}/libmain_shim.so -Wl,-z,now -Wl,-rpath=${BUILD_DIR} -Wl,\@${BUILD_DIR}/main_shim_src/main_shim.c.args",

    INC               => join(" ", map {
                             m#^/# ? "-I $_" : "-I ../../../../$_"
                         } (split /\s+/, $fixed_include)),

    depend => {
        'nginx.c'     => join(" ", map {
                             m#^/# ? $_ : "../../../../../$_"
                         } (split(/\s+/, $ENV{NGX_DEPS}),
                            "src/http/modules/perl/ngx_http_perl_module.h"))
    },

    PM => {
        'nginx.pm'    => '$(INST_LIBDIR)/nginx.pm'
    }
);
