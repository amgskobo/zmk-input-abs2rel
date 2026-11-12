#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build fixed source profiles/guards against one selected supported core."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

OWNER = Path(__file__).resolve().parents[2]
REJECTION = 'Abs2rel source limit excludes a referenced listener index'
CASES = (
    ('auto-sparse', 0, None, None),
    ('auto-low', 0, 'low', None),
    ('limited', 2, 'low', None),
    ('wide', 256, 'low', None),
    ('fallback', 0, 'no-listeners', None),
    ('disabled-device', 2, 'disabled-device', None),
    ('disabled-listener', 2, 'disabled-listener', None),
    ('reject-base', 2, 'base', REJECTION),
    ('reject-child', 2, 'child', REJECTION),
    ('reject-sparse', 2, None, REJECTION),
    ('reject-negative', -1, 'low', 'outside the active range'),
    ('reject-over', 257, 'low', 'outside the active range'),
    ('strict-auto', 0, None, None),
)


def plan(control=None):
    # Canonical dispatch never inherits profile/selector variables. Controls
    # are explicit and do not claim completion of the full matrix.
    return CASES if control is None else tuple(case for case in CASES if case[0] == control)


def run_command(command, output, env, cwd):
    start = time.time_ns()
    with output.open('xb') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, cwd=cwd)
    record = dict(command=command, exit=result.returncode, start_ns=start, end_ns=time.time_ns())
    output.with_suffix(output.suffix + '.json').write_text(json.dumps(record, indent=2) + '\n')
    return result.returncode, output.read_text(errors='replace')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--control', choices=[case[0] for case in CASES])
    args = parser.parse_args()
    workspace, output = args.workspace.resolve(strict=True), args.output
    if not output.is_absolute() or output.exists() or output.is_symlink():
        parser.error('fresh absolute output required')
    if any(path == output.resolve() or path in output.resolve().parents for path in (OWNER, workspace)):
        parser.error('output must be outside selected sources')
    output.mkdir(parents=True, exist_ok=False)
    results = []
    env = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL='1', ASAN_OPTIONS='detect_leaks=0',
               UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1')
    for name, bound, overlay, expected in plan(args.control):
        conf = output / (name + '.conf')
        conf.write_text(f'CONFIG_ZMK_INPUT_ABS2REL_MAX_SOURCES={bound}\n' +
                        ('CONFIG_ASAN=y\nCONFIG_UBSAN=y\n' if name == 'strict-auto' else ''))
        build = output / name
        command = ['bash', '-c',
                   'source "$1"; zmk_test_select_workspace "$2" || exit; shift 2; exec west "$@"',
                   'source-limits', str(OWNER / 'tests/integration/workspace-env.sh'), str(workspace),
                   'build', '-o=-j1', '-s', str(workspace / 'zmk/app'), '-d', str(build),
                   '-b', 'native_sim//zmk_test_mock', '--',
                   '-DZMK_CONFIG=' + str(OWNER / 'tests/source-limits/config'),
                   '-DZMK_EXTRA_MODULES=' + str(OWNER) + ';' + str(OWNER / 'tests/source-limits/module'),
                   '-DEXTRA_CONF_FILE=' + str(conf), '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
        if overlay:
            command += ['-DEXTRA_DTC_OVERLAY_FILE=' + str(OWNER / 'tests/source-limits' / (overlay + '.overlay'))]
        code, text = run_command(command, output / (name + '.build.log'), env, workspace)
        item = dict(case=name, bound=bound, overlay=overlay, build_exit=code)
        if expected:
            item.update(status='PASS' if code != 0 and expected in text else 'FAIL',
                        kind='expected-rejection', required_diagnostic=expected)
        elif code:
            item.update(status='FAIL', kind='setup-or-build-failure')
        else:
            config = (build / 'zephyr/.config').read_text()
            if f'CONFIG_ZMK_INPUT_ABS2REL_MAX_SOURCES={bound}\n' not in config:
                item.update(status='FAIL', kind='wrong-effective-bound')
            else:
                binary = build / 'zephyr/zmk.exe'
                item['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
                item['runs'] = []
                for repeat in range(1, 4):
                    code, text = run_command(['timeout', '30', str(binary)],
                                             output / f'{name}.run-{repeat}.log', env, workspace)
                    upstream = re.search(r'kscan_mock\.c:103[^\n]*runtime error: left shift', text)
                    if name == 'strict-auto' and code and upstream:
                        status = 'BLOCKED'
                    elif code == 0 and 'abs2rel source limits: PASS' in text and not re.search(
                            r'runtime error:|ERROR: AddressSanitizer|ASSERTION FAIL', text):
                        status = 'PASS'
                    else:
                        status = 'FAIL'
                    item['runs'].append(dict(repeat=repeat, exit=code, status=status))
                states = {run['status'] for run in item['runs']}
                item.update(status='FAIL' if 'FAIL' in states else 'BLOCKED' if 'BLOCKED' in states else 'PASS',
                            kind='upstream-sanitizer' if 'BLOCKED' in states else 'product')
        results.append(item)
        print(name + ': ' + item['status'], flush=True)
    if not results:
        raise RuntimeError('required nonempty source matrix did not run')
    states = {item['status'] for item in results}
    status = 'FAIL' if 'FAIL' in states else 'BLOCKED' if 'BLOCKED' in states else 'PASS'
    report = dict(status=status, complete_matrix=args.control is None, expected_cases=len(plan(args.control)),
                  completed_cases=len(results), results=results)
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(('abs2rel source control' if args.control else 'abs2rel complete source matrix') + ': ' + status)
    return 1 if status == 'FAIL' else 2 if status == 'BLOCKED' else 0


if __name__ == '__main__':
    raise SystemExit(main())
