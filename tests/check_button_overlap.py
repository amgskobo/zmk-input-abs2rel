#!/usr/bin/env python3
# Copyright (c) 2026 amgskobo
# SPDX-License-Identifier: MIT
"""Require independent overlapping button/listener releases and reject shared button history."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

SOURCE_FILES = [ROOT / "drivers/input/input_processor_absolute_to_relative.c",
                ROOT / "tests/runtime/driver_harness.c", ROOT / "tests/runtime/run.py",
                pathlib.Path(__file__).resolve()]
EMIT = "--emit-driver"
BASE_DEFINES = ["-DABS2REL_TEST_OVERLAP_ONLY=1"]
FAULT_DEFINE = "-DABS2REL_TEST_ALIAS_BUTTON_HISTORY=1"
RUN_ARGS = []
PASS_MARKER = "overlapping button/listener release ownership: 24 cases PASS"
FAILURE_MARKER = "overlap button release ownership"
SUMMARY = "button overlap: normal24 cases PASS; shared button history rejected at release ownership"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    output = parser.parse_args().output.absolute()
    if output.exists() or output.is_symlink():
        raise FileExistsError("fresh evidence directory required: " + str(output))
    output.mkdir(parents=True, exist_ok=False)
    sources = list((ROOT / "include").rglob("*.h")) + SOURCE_FILES
    (output / "source-sha256.json").write_text(json.dumps({
        str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sources}, indent=2) + "\n", encoding="utf-8")
    commands = []
    def run(command):
        command = [str(part) for part in command]
        try:
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, timeout=30)
        except subprocess.TimeoutExpired as failure:
            commands.append({"command": command, "exit": "TIMEOUT", "output":
                             str(failure.stdout or "")})
            (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n", encoding="utf-8")
            raise
        commands.append({"command": command, "exit": result.returncode, "output": result.stdout})
        (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n", encoding="utf-8")
        return result
    run(["cc", "--version"]).check_returncode()
    unit = output / "fixture.c"
    run([sys.executable, ROOT / "tests/runtime/run.py", EMIT, unit]).check_returncode()
    original = unit.read_bytes()
    refused = run([sys.executable, ROOT / "tests/runtime/run.py", EMIT, unit])
    assert refused.returncode != 0 and unit.read_bytes() == original, "emission must preserve existing source"
    alias = output / "existing-alias.c"
    alias.symlink_to(unit.name)
    refused = run([sys.executable, ROOT / "tests/runtime/run.py", EMIT, alias])
    assert refused.returncode != 0 and unit.read_bytes() == original, "emission must preserve alias target"
    unknown = output / "unknown.c"
    refused = run([sys.executable, ROOT / "tests/runtime/run.py", "--emit-unknown", unknown])
    assert refused.returncode != 0 and not unknown.exists(), "unknown emission mode must fail before creation"
    summary = {}
    for label, fault in (("normal", []), ("fault", [FAULT_DEFINE])):
        binary = output / label
        run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", *BASE_DEFINES, *fault,
             "-I" + str(ROOT / "include"), "-I" + str(ROOT / "drivers/input"),
             "-I" + str(ROOT / "tests/runtime"), str(unit), "-o", str(binary)]).check_returncode()
        result = run([binary, *RUN_ARGS])
        if label == "normal":
            assert result.returncode == 0 and PASS_MARKER in result.stdout, "normal boundary must pass"
        else:
            assert result.returncode != 0 and FAILURE_MARKER in result.stdout, "fault must fail the exact boundary assertion"
        summary[label] = {"exit": result.returncode, "expected": True}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(SUMMARY)

if __name__ == "__main__":
    main()
