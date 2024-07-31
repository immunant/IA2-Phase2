#!/usr/bin/env python3

import json
from pathlib import Path

def try_convert_arg(arg: str) -> str:
    for prefix in ["", "-I"]:
        if not arg.startswith(prefix):
            continue
        path = Path(arg[len(prefix):])
        if path.exists():
            return prefix + str(path.resolve())
    return arg

def main():
    # `libclangTooling` requires the compilation database to be named exactly `compile_commands.json`.
    # It's up to the caller to set the right cwd.
    cc_db = Path("compile_commands.json")
    cmds = json.loads(cc_db.read_text())
    for cmd in cmds:
        for field in ["file", "output", "directory"]:
            if field in cmd:
                cmd[field] = str(Path(cmd[field]).resolve())
        if "command" in cmd:
            cmd["command"] = " ".join(try_convert_arg(arg) for arg in cmd["command"].split())
        if "arguments" in cmd:
            cmd["arguments"] = [try_convert_arg(arg) for arg in cmd["arguments"]]
    cc_db.write_text(json.dumps(cmds, indent=4))

if __name__ == "__main__":
    main()
