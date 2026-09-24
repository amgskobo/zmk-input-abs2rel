"""Exercise actual event helpers from the Zephyr driver with host stubs."""

import pathlib
import re
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
source_path = root / "drivers/input/input_processor_absolute_to_relative.c"
source = source_path.read_text(encoding="utf-8")
names = (
    "encode_suppression",
    "drop_reference",
    "apply_generation",
    "stream_for_event",
    "process_axis",
    "handle_touch_button",
    "handle_button_suppress",
    "absolute_to_relative_handle_event",
    "absolute_to_relative_init",
    "absolute_to_relative_device_valid",
    "absolute_to_relative_get_suppression",
    "absolute_to_relative_set_suppression",
    "absolute_to_relative_layer_listener",
)
functions = []
for name in names:
    found = None
    for match in re.finditer(r"\b" + name + r"\s*\(", source):
        start = max(source.rfind("\nstatic", 0, match.start()),
                    source.rfind("\nint ", 0, match.start())) + 1
        preceding = source[start:match.start()]
        if start and ";" not in preceding and "}" not in preceding:
            found = start
            break
    if found is None:
        raise RuntimeError(f"missing driver definition: {name}")
    end = source.index("\n}", match.end()) + 2
    line = source.count("\n", 0, found) + 1
    functions.append(f'#line {line} "{source_path}"\n' + source[found:end])

harness = (root / "tests/runtime/harness.c").read_text(encoding="utf-8")
resync_start = source.index("#define ABSOLUTE_TO_RELATIVE_RESYNC")
resync_end = source.index("\n\nstatic int absolute_to_relative_layer_listener", resync_start)
resync_line = source.count("\n", 0, resync_start) + 1
functions.insert(-1, f'#line {resync_line} "{source_path}"\n' +
                 source[resync_start:resync_end])
with tempfile.TemporaryDirectory(prefix="abs2rel-runtime-") as folder:
    work = pathlib.Path(folder)
    unit = work / "test.c"
    unit.write_text(harness.replace("/* DRIVER_FUNCTIONS */", "\n".join(functions) +
                                    f'\n#line 1 "{unit}"\n'), encoding="utf-8")
    for variant, flags in (
        ("optimized", ["-O2"]),
        ("sanitized", ["-O1", "-g", "-fno-omit-frame-pointer",
                       "-fsanitize=address,undefined", "-fno-sanitize-recover=all"]),
        ("coverage", ["-O0", "--coverage"]),
    ):
        binary = work / variant
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
                        "-I" + str(root / "include"), str(unit), "-o", str(binary)],
                       check=True)
        subprocess.run([str(binary)], check=True)
    report = subprocess.run(["gcov", "-b", "-c", "-o", str(work / "coverage-test.gcno"),
                             str(unit)], cwd=work, check=True, capture_output=True, text=True)
    print(report.stdout, end="")
    driver = report.stdout.split(f"File '{source_path}'", 1)[1].split("Creating ", 1)[0]
    if "Lines executed:100.00%" not in driver or "Taken at least once:100.00%" not in driver:
        raise RuntimeError("extracted event helpers fell below 100% line/branch coverage")
