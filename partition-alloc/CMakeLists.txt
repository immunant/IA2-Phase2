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
	partition_alloc_base/logging.cc
	partition_alloc_base/memory/ref_counted.cc
	partition_alloc_base/rand_util.cc
	partition_alloc_base/strings/stringprintf.cc
	partition_alloc_base/threading/platform_thread.cc
	partition_alloc_base/time/time.cc
	partition_alloc_base/time/time_override.cc
	partition_alloc_base/time/time_now_posix.cc
	partition_alloc_hooks.cc
	partition_bucket.cc
	partition_oom.cc
	partition_page.cc
	partition_root.cc
	partition_stats.cc
	pkey.cc
	random.cc
	reservation_offset_table.cc
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

add_library(partition-alloc SHARED
    src/allocator_shim.cc
    ${PA_SRCS})

if(LIBIA2_INSECURE)
    target_compile_definitions(partition-alloc PUBLIC LIBIA2_INSECURE=1)
endif()

target_include_directories(partition-alloc
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EXTERNAL_DIR}/chromium/src)

target_compile_options(partition-alloc
	PRIVATE
		"-fPIC"
		"-std=c++17"
		"-Wno-invalid-offsetof"
		"-Wno-return-type"
)
target_link_options(partition-alloc
    INTERFACE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
        "-Wl,--wrap=free"
    PRIVATE
        "-Wl,-z,now"
)
