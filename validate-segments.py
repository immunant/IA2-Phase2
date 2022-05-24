#!/usr/bin/env python3
import os
import sys
import subprocess

args = sys.argv[1:]

readelf_process = subprocess.run(["readelf", "-W", "--segments"] + args,
	capture_output=True,
	check=True,
	encoding="UTF-8",
	env=dict(os.environ, **{"LANG": "C"})
)
readelf_lines = readelf_process.stdout.split("\n")[1:]

segment_begin = readelf_lines.index("Program Headers:") + 2
segment_count = readelf_lines[segment_begin:].index("")
segment_lines = readelf_lines[segment_begin:segment_begin + segment_count]

# filter "interpreter" line
segment_lines = filter(lambda line: not line.startswith("      "), segment_lines)

segments = list(map(str.split, segment_lines))


seg_sections_begin = readelf_lines.index(" Section to Segment mapping:") + 2
seg_sections_count = readelf_lines[seg_sections_begin:].index("")
seg_sections_lines = readelf_lines[seg_sections_begin:seg_sections_begin + seg_sections_count]

seg_sections = list(map(lambda x: x.split()[1:], seg_sections_lines))

def offset_round(offset: str) -> bool:
	return offset.endswith("000")

for i, segment in enumerate(segments):
	invalid = None
	if not offset_round(segment[1]):
		invalid = segment[1]
	if not offset_round(segment[2]):
		invalid = segment[2]
	if not offset_round(segment[3]):
		invalid = segment[3]
	if (segment[0] == "LOAD" or "RELRO" in segment[0]) and invalid:
		print(f"invalid segment {i} ({invalid} not aligned): {segment}")
		print(f"segment sections: {' '.join(seg_sections[i])}")
		objdump_process = subprocess.run(["objdump", "-w", "--section-headers"] + args,
			capture_output=True,
			check=True,
			encoding="UTF-8",
			env=dict(os.environ, **{"LANG": "C"})
		)
		print(objdump_process.stdout)
		sys.exit(1)
