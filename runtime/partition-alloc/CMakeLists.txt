cmake_minimum_required(VERSION 3.12)
project(partition-alloc)

set(PA_SRCS
	address_pool_manager.cc
	address_pool_manager_bitmap.cc
	address_space_randomization.cc
	allocation_guard.cc
	dangling_raw_ptr_checks.cc
	memory_reclaimer.cc
	oom.cc
	oom_callback.cc
	page_allocator.cc
	partition_address_space.cc
	partition_alloc.cc
	partition_alloc_base/check.cc
	partition_alloc_base/cpu.cc
	partition_alloc_base/debug/alias.cc
	partition_alloc_base/debug/stack_trace_linux.cc
	partition_alloc_base/debug/stack_trace_posix.cc
	partition_alloc_base/logging.cc
	partition_alloc_base/log_message.cc
	partition_alloc_base/memory/page_size_posix.cc
	partition_alloc_base/memory/ref_counted.cc
	partition_alloc_base/rand_util.cc
	partition_alloc_base/strings/cstring_builder.cc
	partition_alloc_base/strings/safe_sprintf.cc
	partition_alloc_base/strings/stringprintf.cc
	partition_alloc_base/strings/string_util.cc
	partition_alloc_base/threading/platform_thread.cc
	partition_alloc_base/time/time.cc
	partition_alloc_base/time/time_override.cc
	partition_alloc_base/time/time_now_posix.cc
	partition_alloc_hooks.cc
	partition_bucket.cc
	partition_dcheck_helper.cc
	partition_freelist_entry.cc
	partition_oom.cc
	partition_page.cc
	partition_root.cc
	partition_stats.cc
	thread_isolation/mte.cc
	thread_isolation/pkey.cc
	thread_isolation/thread_isolation.cc
	random.cc
	reservation_offset_table.cc
	shim/allocator_shim.cc
	shim/nonscannable_allocator.cc
	spinning_mutex.cc
	starscan/metadata_allocator.cc
	starscan/pcscan.cc
	starscan/pcscan_internal.cc
	starscan/pcscan_scheduling.cc
	starscan/snapshot.cc
	starscan/stack/stack.cc
	starscan/stats_collector.cc
	starscan/write_protector.cc
	tagging.cc
	thread_cache.cc
	page_allocator_internals_posix.cc
	partition_alloc_base/files/file_util_posix.cc
	partition_alloc_base/posix/safe_strerror.cc
	partition_alloc_base/rand_util_posix.cc
	partition_alloc_base/threading/platform_thread_posix.cc
	partition_alloc_base/time/time_conversion_posix.cc)

list(TRANSFORM PA_SRCS PREPEND ${EXTERNAL_DIR}/chromium/src/base/allocator/partition_allocator/)

add_library(partition-alloc_unpadded SHARED
	src/allocator_shim_default_dispatch_to_partition_alloc.cc
	src/get_pkey.cc
	src/shared_allocator.cc
    ${PA_SRCS})

pad_tls_library(partition-alloc_unpadded partition-alloc)

target_link_libraries(partition-alloc_unpadded PRIVATE
    stdc++
)

target_include_directories(partition-alloc BEFORE
	INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(partition-alloc_unpadded BEFORE
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
    ${EXTERNAL_DIR}/chromium/src)

# Propagage ARM64 define
if(LIBIA2_AARCH64)
    target_compile_options(partition-alloc_unpadded
    PRIVATE
        "-DARCH_CPU_ARM64"
    )
endif()

target_compile_options(partition-alloc_unpadded
	PRIVATE
		"-fPIC"
		"-std=c++17"
		"-Wno-invalid-offsetof"
		"-Wno-return-type"
)
target_compile_options(partition-alloc_unpadded
	PUBLIC
		"-fno-exceptions"
)
target_link_options(partition-alloc_unpadded
    PRIVATE
        "-Wl,-z,now"
)

# Wrapping options from upstream BUILD.gn
target_link_options(partition-alloc_unpadded
	PUBLIC
		"-Wl,-wrap,calloc"
		"-Wl,-wrap,free"
		"-Wl,-wrap,malloc"
		"-Wl,-wrap,memalign"
		"-Wl,-wrap,posix_memalign"
		"-Wl,-wrap,pvalloc"
		"-Wl,-wrap,realloc"
		"-Wl,-wrap,valloc"

		# Not allocating memory, but part of the API
		"-Wl,-wrap,malloc_usable_size"

		# <stdlib.h> functions
		"-Wl,-wrap,realpath"

		# <string.h> functions
		"-Wl,-wrap,strdup"
		"-Wl,-wrap,strndup"

		# <unistd.h> functions
		"-Wl,-wrap,getcwd"

		# <stdio.h> functions
		"-Wl,-wrap,asprintf"
		"-Wl,-wrap,vasprintf"
)
