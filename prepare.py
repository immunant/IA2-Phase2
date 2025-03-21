#/usr/bin/python

import json
import subprocess
import os
import tempfile

from typing import List, Any, Set

# TODO: Make into CLI args.
repo_path = "/home/legare/mozilla-unified"
cc_path = "/home/legare/mozilla-unified/obj-x86_64-pc-linux-gnu/compile_commands.json"
ia2_path = "/home/legare/IA2-Phase2"

# Path within an IA2 checkout where the built rewriter can be found.
DEFAULT_REWRITER_PATH = "build/tools/rewriter/ia2-rewriter"

def sanitize_cc_cmds(cmds: List[Any]):
	for entry in cmds:
		entry['command'] = entry['command'].replace('-fstrict-flex-arrays=1', '')
		# force C++17 support
		if "++" in entry['command']:
			entry['command'] = entry['command'] + ' -std=gnu++17'

def load_cc_cmds() -> List[Any]:
	return json.load(open(cc_path, "r"))

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

def rewriter(cc_cmds: List[Any], compartment_files: List[str], files_to_rewrite: List[str]):
	compile_commands_file = mktemp(json.dumps(cc_cmds))
	compartment_files_file = mktemp('\n'.join(compartment_files))
	files_to_rewrite_file = mktemp('\n'.join(files_to_rewrite))

	out_dir = "ff-rewritten"
	wrapper_prefix = out_dir + "/wrapper"
	rewriter_path = f"{ia2_path}/{DEFAULT_REWRITER_PATH}"

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
	return [], [wrapper_prefix + ".c", wrapper_prefix + ".h"]

modified_files, wrapper = rewriter(filtered_cc_cmds, compartment_files, files_to_rewrite)

# stop here for now
os._exit(0)

def load_link_cmds() -> List[Any]:
	return json.load(open("link_commands.json", "r"))



link_cmds = load_link_cmds()
#add ia2 lib and flags to link commands
link_cmds = add_ia2(link_cmds)

compile(original_files + modified_files + wrapper, ia2_lib)

def run(cmds):
	for entry in cmds:
		os.chdir(entry['directory'])
		system(entry['command'])

run(cc_cmds)
run(link_cmds)
