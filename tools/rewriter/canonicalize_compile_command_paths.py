#!/usr/bin/env python3

# /// script
# requires-python = ">=3.4"
# dependencies = [
#     "pathlib",
# ]
# ///

import json
from pathlib import Path

def main():
    cc_db = Path("compile_commands.json")
    cc_text = cc_db.read_text()
    cmds = json.loads(cc_text)
    for cmd in cmds:
        dir = Path(cmd["directory"]).resolve()
        cmd["directory"] = str(dir)
        
        def convert_path(path: str) -> str:
            path: Path = Path(path)
            if not path.is_absolute():
                path = dir / path
            path = path.resolve()
            return str(path)

        cmd["file"] = convert_path(cmd["file"])
        if "output" in cmd:
            cmd["output"] = convert_path(cmd["output"])
        
        def try_convert_arg(arg: str) -> str:
            for prefix in ["", "-I"]:
                if not arg.startswith(prefix):
                    continue
                path = Path(arg[len(prefix):])
                if not path.is_absolute():
                    path = dir / path
                if path.exists():
                    path = path.resolve()
                    return prefix + str(path)
            return arg

        if "command" in cmd:
            cmd["command"] = " ".join(try_convert_arg(arg) for arg in cmd["command"].split())
        if "arguments" in cmd:
            cmd["arguments"] = [try_convert_arg(arg) for arg in cmd["arguments"]]
    cc_text = json.dumps(cmds, indent=4)
    cc_db.write_text(cc_text)

if __name__ == "__main__":
    main()
