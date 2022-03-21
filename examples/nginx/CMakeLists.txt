set(CONFIGURE_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/auto/configure)

add_custom_target(nginx-config
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CONFIGURE_SCRIPT}
        --prefix=${CMAKE_CURRENT_BINARY_DIR}
        --builddir=${CMAKE_CURRENT_BINARY_DIR}
        --with-http_perl_module=dynamic
        --conf-path=${CMAKE_CURRENT_SOURCE_DIR}/conf/nginx.conf
        --pid-path=${CMAKE_CURRENT_BINARY_DIR}/nginx.pid
        --error-log-path=${CMAKE_CURRENT_BINARY_DIR}/err.log
        --http-log-path=${CMAKE_CURRENT_BINARY_DIR}/http.log
        --http-client-body-temp-path=${CMAKE_CURRENT_BINARY_DIR}/client_body_temp
        --http-proxy-temp-path=${CMAKE_CURRENT_BINARY_DIR}/proxy_temp
        --http-fastcgi-temp-path=${CMAKE_CURRENT_BINARY_DIR}/fastcgi_temp
        --http-uwsgi-temp-path=${CMAKE_CURRENT_BINARY_DIR}/uwsgi_temp
        --http-scgi-temp-path=${CMAKE_CURRENT_BINARY_DIR}/scgi_temp)

set(NGINX_PERL_DIR src/http/modules/perl)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${NGINX_PERL_DIR}/Makefile.PL.in
    ${CMAKE_CURRENT_BINARY_DIR}/${NGINX_PERL_DIR}/Makefile.PL @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/perl/hello.pm
    ${CMAKE_CURRENT_BINARY_DIR}/perl/lib/hello.pm)

add_custom_target(nginx-build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND make -j8 -f ${CMAKE_CURRENT_BINARY_DIR}/Makefile
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/nginx)

add_dependencies(nginx-build nginx-config)

add_custom_target(nginx-run
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/nginx
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/nginx)
