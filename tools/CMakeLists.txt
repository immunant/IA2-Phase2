cmake_minimum_required(VERSION 4.0)
project(tools)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IA2_TRACE_EXIT "Enable IA2 exit-path tracing instrumentation" OFF)

add_subdirectory(rewriter)
add_subdirectory(pad-tls)
