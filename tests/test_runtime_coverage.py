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
                        text = "function drop_reference called 1\\n1:24:one gated function only\\n"
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
                    self.assertIn('a gated function', result.stderr)
                calls = (work / 'compiler.jsonl').read_text(encoding='utf-8').splitlines()
                self.assertGreater(len(calls), 0)
                self.assertTrue(all(isinstance(json.loads(row), list) for row in calls))


    def test_each_production_function_and_branch_is_required(self):
        import importlib.util
        runner=Path(__file__).resolve().parent/'runtime/run.py'
        spec=importlib.util.spec_from_file_location('coverage',runner)
        module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        source='static int first(void) { return 1; }\nstatic int second(void) { return 2; }'
        complete='function first called 1\n1:10:first\nbranch 0 taken 1\nfunction second called 1\n2:20:second\n'
        self.assertEqual(module.check_report(complete,source),(2,2,1))
        with self.assertRaisesRegex(RuntimeError,'a gated function'):
            module.check_report('function first called 1\n1:10:first\n',source)
        with self.assertRaisesRegex(RuntimeError,'no executable lines'):
            module.check_report('function first called 1\n',source)
        for text in [complete.replace('1:10','#####:10'),complete.replace('taken 1','taken 0'),complete.replace('taken 1','never executed'),complete.replace('second called 1','second called 0')]:
            with self.subTest(report=text),self.assertRaises(RuntimeError): module.check_report(text,source)

    def test_invalid_mode_and_removed_selectors_fail(self):
        runner=Path(__file__).resolve().parent/'runtime/run.py'
        result=subprocess.run([sys.executable,str(runner),'--emit-driver','unused'],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        result=subprocess.run([sys.executable,str(runner)],env=dict(os.environ,ZMK_TEST_MODE='invalid'),capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)


if __name__ == '__main__':
    unittest.main()
