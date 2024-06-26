# Configuration file for the LLVM lit test runner.
# Loosely based on the lit.cfg.py from LLVM

import lit
from lit.llvm import llvm_config
from lit.llvm.subst import ToolSubst
import os

# name: The name of this test suite.
config.name = 'IA2'

# testFormat: The test format to use to interpret tests.
config.test_format = lit.formats.ShTest(False)

# suffixes: A list of file extensions to treat as test files. This is overriden
# by individual lit.local.cfg files in the test subdirectories.
config.suffixes = ['.c']

# excludes: A list of directories to exclude from the testsuite. The 'Inputs'
# subdirectories contain auxiliary inputs for various tests in their parent
# directories.
config.excludes = [entry.name for entry in os.scandir(os.path.dirname(os.path.abspath(__file__))) if entry.name not in [
    'global_fn_ptr',
    'header_includes',
    'heap_two_keys',
    'macro_attr',
    'minimal',
    'mmap_loop',
    'omit_wrappers',
    'protected_threads',
    'read_config',
    'recursion',
    'rewrite_fn_ptr_eq',
    'rewrite_macros',
    'ro_sharing',
    'shared_data',
    'should_segfault',
    'sighandler',
    'simple1',
    'structs',
    'threads',
    'trusted_direct',
    'trusted_indirect',
    'two_keys_minimal',
    'two_shared_ranges',
    'untrusted_indirect',
]]

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

llvm_config.add_tool_substitutions([ToolSubst('ia2-rewriter')], search_dirs=os.path.join(config.ia2_obj_root, "tools", "rewriter"))

llvm_config.use_default_substitutions()
llvm_config.use_clang()

config.substitutions.extend([
    ('%ia2_generate_checks', '%s/tests/generate-checks.sh' % config.ia2_src_root),
    ('%ia2_include', '%s/runtime/libia2/include' % config.ia2_src_root),
    ('%binary_dir', config.ia2_obj_root),
    ('%source_dir', config.ia2_src_root)
])

config.environment['LD_BIND_NOW'] = '1'
