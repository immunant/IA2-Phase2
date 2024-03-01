# Repo directory structure

## cmake
CMake files for writing automated tests


## docs
Contains documentation on usage, design and technical details


## examples
Compartmentalized demo applications (not tested as part of the automated test
suite)


## external
Third-party code. These are git subtrees of upstream codebases and each commit
may only modify one subtree or the main repository.

#### chromium
The subset of chromium needed to build PartitionAlloc. See
runtime/partition-alloc for subtree's latest upstream commit and details on
pulling upstream changes.

#### nginx
Nginx web server pulled from github.com/nginx/nginx mirror. Used with the RTMP
module to demo compartmentalizing Nginx plugins.

#### nginx-rtmp-module
Nginx media streaming RTMP module from github.com/arut/nginx-rtmp-module


## runtime 
The compartmentalization runtime, including code for both the target process
itself and for tracing the process.

#### libia2
The main compartmentalization library. Includes code for interposing libc and
the API for compartmentalizing codebases where manual changes are required.

#### partition-alloc
A compartment-aware shim for PartitionAlloc.

#### tracer
A program for restricting syscalls and tracking memory ownership changes as a result of syscalls in compartmentalized programs.


## tests
The automated tests that are run as part of the `check` CMake target.


## tools
Host tools required to build compartmentalized programs.

#### rewriter
Source rewriter and code generator for call gates.

#### pad-tls
Script to patch TLS segments in compartmentalized ELFs.
