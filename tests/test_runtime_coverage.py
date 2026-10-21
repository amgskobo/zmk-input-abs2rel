# Copyright (c) 2026 amgskobo
# SPDX-License-Identifier: MIT
"""Synthetic compiler/gcov process controls for the public runtime entry."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest


class RuntimeCoverageAdmission(unittest.TestCase):
    def test_empty_or_unrelated_report_cannot_claim_coverage(self):
        root = Path(__file__).resolve().parents[1]
        for kind in ('empty', 'outside', 'missing-range'):
            with self.subTest(report=kind), tempfile.TemporaryDirectory() as folder:
                work = Path(folder)
                tools = work / 'bin'
                tools.mkdir()
                compiler = tools / 'cc'
                compiler.write_text(textwrap.dedent('''\
                    #!/usr/bin/env python3
                    import json, os, pathlib, sys
                    args = sys.argv[1:]
                    with open(os.environ["RECORDING_COMPILER_LOG"], "a") as log:
                        log.write(json.dumps(args) + "\\n")
                    output = pathlib.Path(args[args.index("-o") + 1])
                    output.write_text("#!/bin/sh\\nexit 0\\n")
                    output.chmod(0o755)
                    '''), encoding='utf-8')
                compiler.chmod(0o755)
                gcov = tools / 'gcov'
                gcov.write_text(textwrap.dedent('''\
                    #!/usr/bin/env python3
                    import os, pathlib
                    kind = os.environ["COVERAGE_REPORT_KIND"]
                    text = "" if kind == "empty" else "1:0:unrelated metadata\\n"
                    source_root = pathlib.Path(os.environ["CONTROLLED_OWNER_ROOT"], "drivers/input")
                    if kind == "missing-range":
                        source = (source_root / "input_processor_absolute_to_relative.c").read_text()
                        line = source[:source.index("static atomic_val_t encode_suppression")].count("\\n") + 2
                        text = "1:" + str(line) + ":one gated function only\\n"
                    for source in source_root.glob("*.c"):
                        pathlib.Path(source.name + ".gcov").write_text(text)
                    '''), encoding='utf-8')
                gcov.chmod(0o755)
                env = dict(os.environ, ZMK_TEST_MODE='full',
                           PYTHONDONTWRITEBYTECODE='1',
                           RECORDING_COMPILER_LOG=str(work / 'compiler.jsonl'),
                           COVERAGE_REPORT_KIND=kind, CONTROLLED_OWNER_ROOT=str(root))
                env['PATH'] = str(tools) + os.pathsep + env['PATH']
                result = subprocess.run([sys.executable, str(root / 'tests/runtime/run.py')],
                                        env=env, capture_output=True, text=True, timeout=30)
                self.assertNotEqual(result.returncode, 0,
                                    'synthetic noncoverage report falsely passed: ' + result.stdout)
                self.assertIn('coverage report has no executable lines', result.stderr)
                if kind == 'missing-range':
                    self.assertIn('a gated function range', result.stderr)
                calls = (work / 'compiler.jsonl').read_text(encoding='utf-8').splitlines()
                self.assertGreater(len(calls), 0)
                self.assertTrue(all(isinstance(json.loads(row), list) for row in calls))


    def test_each_gated_range_requires_measured_lines(self):
        import importlib.util
        runner = Path(__file__).resolve().parent / 'runtime/run.py'
        spec = importlib.util.spec_from_file_location('abs2rel_runtime_coverage', runner)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        complete = '1:10:first\nbranch 0 taken 1\n2:20:second\n'
        self.assertEqual(module.misses(complete, [(10, 10), (20, 20)]),
                         (2, 2, 1, 1, []))
        with self.assertRaisesRegex(RuntimeError, 'a gated function range'):
            module.misses('1:10:first\n', [(10, 10), (20, 20)])
        with self.assertRaisesRegex(RuntimeError, 'no executable lines'):
            module.misses(complete, [])
        missed = module.misses('#####:10:first\nbranch 0 never executed\n', [(10, 10)])
        self.assertEqual(missed[:4], (1, 0, 1, 0))
        self.assertEqual(len(missed[4]), 2)


if __name__ == '__main__':
    unittest.main()
