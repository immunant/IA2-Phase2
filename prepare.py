#/usr/bin/python

import json
import subprocess
import os
import tempfile

from typing import List, Any, Set

repo_path = "/home/fwings/firefox-tree/mozilla-unified"
ia2_path = "/home/fwings/IA2-Phase2"
ia2_build_path = ia2_path + "/build-repro"

def sanitize_cc_cmds(cmds: List[Any]):
	for entry in cmds:
		entry['command'] = entry['command'].replace('-fstrict-flex-arrays=1', '')
		# force C++17 support
		if "++" in entry['command']:
			entry['command'] = entry['command'] + ' -std=gnu++17'

def load_cc_cmds() -> List[Any]:
	return json.load(open("compile_commands.json", "r"))

cc_cmds = load_cc_cmds()
sanitize_cc_cmds(cc_cmds)

compartment_cc_cmds = filter(lambda entry: "media/libjpeg/" in entry['file'], cc_cmds)
compartment_files = list(map(lambda entry: entry['file'], compartment_cc_cmds))
print("compartment_files: " + str(compartment_files) + "\n")

def select_files(exclude_files: List[str]) -> Set[str]:
	p = subprocess.Popen([
		'rg', '-l',
		'\\bjpeg_|\\bjinit_',
		'-g', '!libjpeg',
		# ignore jxl for now, would be nice to cover later
		'-g' '!jpeg-xl',
		# ignore skia, causes errors
		'-g' '!skia',
		# ignore third-party copies not built in our config
		'-g' '!**/aom/third_party/**',
		'-g' '!**/libvpx/third_party/**',
		'-g', '*.c',
		'-g', '*.cc',
		'-g', '*.cpp',
		# not part of build
		'-g' '!jpeg_frame_writer.cc',
		repo_path
	], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	stdout, stderr = p.communicate()
	found_files = stdout.decode().strip('\n').split('\n')
	return set(found_files) - set(exclude_files)

files_to_rewrite = list(select_files(compartment_files))

print("files_to_rewrite: " + str(files_to_rewrite) + "\n")
	#mozilla_unified/) - compartment_files

# only contains cc_cmds for files to rewrite or that define rewritten functions
filtered_cc_cmds = list(filter(lambda entry: entry['file'] in files_to_rewrite or entry['file'] in compartment_files, cc_cmds))

def mktemp(contents: str) -> tempfile._TemporaryFileWrapper:
	file = tempfile.NamedTemporaryFile()
	file.write(contents.encode())
	file.flush()
	return file
	
CALL_GATES_FILENAME = "call_gates"

out_dir = "ff-rewritten"

def rewriter(cc_cmds: List[Any], compartment_files: List[str], files_to_rewrite: List[str], out_dir: str):
	rewriter_path = ia2_build_path + '/tools/rewriter/ia2-rewriter'
	compile_commands_file = mktemp(json.dumps(cc_cmds))
	compartment_files_file = mktemp('\n'.join(compartment_files))
	files_to_rewrite_file = mktemp('\n'.join(files_to_rewrite))

	wrapper_prefix = out_dir + "/" + CALL_GATES_FILENAME

	p = subprocess.Popen([
		'gdb', '-ex', 'run', '--args',
		rewriter_path,
		'--library-only-mode',
		'--cc-db',
		compile_commands_file.name,
		'--library-files',
		compartment_files_file.name,
		'--rewrite-files',
		files_to_rewrite_file.name,
		'--root-directory',
		repo_path,
		'--output-directory',
		out_dir,
		'--output-prefix',
		wrapper_prefix,
	])
	p.wait()
	print("exit: {}", p.returncode)
	assert(p.returncode == 0)

	# todo: get modified files from rewriter
	modified_files = files_to_rewrite

	ld_args_file = wrapper_prefix + "_1.ld"
	wrapper_c = wrapper_prefix + ".c"
	wrapper_h = wrapper_prefix + ".h"
	return modified_files, wrapper_c, wrapper_h, ld_args_file

modified_files, wrapper_c, wrapper_h, ld_args_file = rewriter(filtered_cc_cmds, compartment_files, files_to_rewrite, out_dir)

# stop here for now
os._exit(0)

def build_wrapper_lib(wrapper_c: str) -> str:
    # build wrapper lib
    wrapper_lib_filename = f"lib{CALL_GATES_FILENAME}.so"
    
    # run compiler <flags> -fPIC -shared wrapper.c -o libwrapper.so -L/path/to/orig -lorig
    wrapper_cmd = f"cc -fPIC -shared -Wl,-z,now {wrapper_c} -I {ia2_path}/runtime/libia2/include -o {wrapper_lib_filename}" #-L/path/to/orig -lorig"
    os.system(wrapper_cmd)
    return wrapper_lib_filename

build_wrapper_lib(wrapper_c)

def add_cflags(cc_cmd: str, cflags: List[str]) -> str:
    before, after = cc_cmd.split(' ', 1)
    return ' '.join([before] + cflags + [after])

def add_ia2_includes(wrapper_h, modified_files: List[str], cc_cmds: List[Any]) -> List[Any]:
    new_cmds = cc_cmds.copy()
    ia2_include = ["-I{ia2_path}/libia2/include"]
    wrapper_include = ["-include", wrapper_h]
    for entry in new_cmds:
        if entry["filename"] in modified_files:
            entry["command"] = add_cflags(entry["command"], ia2_include)
    return cc_cmds

cc_cmds_with_ia2_includes = add_ia2_includes(filtered_cc_cmds)

def load_link_cmds() -> List[Any]:
	return json.load(open("link_commands.json", "r"))

def add_ia2_libs(link_cmds):
	for entry in link_cmds:
	    # add --wrap=... flags via ld_args_file
	    # -L ia2_build_dir/runtime/libia2/
		# -llibia2
		entry['command'] = entry['command']

link_cmds = load_link_cmds()
#add ia2 lib and flags to link commands
link_cmds = add_ia2(link_cmds)

# def compile(files, cmds):
#     entry_for_file = {}
#     for entry in cmds:
#         entry_for_file[entry['filename']] = entry
#     for filename in files:
#         command = entry_for_file[filename]['command']
#         #os.system(command)
#         print(f"should run {command}")
        
#ia2_lib
#compile(original_files + modified_files + wrapper, )

def run(cmds, files=[]):
	for entry in cmds:
	    # if entry['filename'] not in files:
		#    continue
		os.chdir(entry['directory'])
		system(entry['command'])

run(cc_cmds)
run(link_cmds)
