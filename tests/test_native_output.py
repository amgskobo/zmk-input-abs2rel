# SPDX-License-Identifier: MIT
"""Native entry rejects occupied evidence paths before invoking west."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

class NativeOutput(unittest.TestCase):
    def test_existing_and_linked_outputs_preserve_evidence(self):
        entry=Path(__file__).resolve().parent/'run-native.sh'
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder); workspace=root/'workspace'; workspace.mkdir()
            tools=root/'tools'; tools.mkdir(); west=tools/'west'
            west.write_text('#!/bin/sh\ntouch "$CALLS"\nexit 0\n'); west.chmod(0o755)
            occupied=root/'occupied'; occupied.mkdir(); sentinel=occupied/'sentinel'; sentinel.write_text('original')
            linked=root/'linked'; linked.symlink_to(occupied,target_is_directory=True)
            dangling=root/'dangling'; dangling.symlink_to(root/'absent',target_is_directory=True)
            logged=root/'logged'; Path(str(logged)+'.build.log').write_text('original log')
            for target in [occupied,linked,dangling,logged,Path('relative-output')]:
                env=dict(os.environ,ZMK_TEST_OUTPUT=str(target),PATH=str(tools)+os.pathsep+os.environ['PATH'],CALLS=str(root/'calls'))
                result=subprocess.run(['bash',str(entry),str(workspace)],env=env,capture_output=True,text=True)
                with self.subTest(output=str(target)):
                    self.assertNotEqual(result.returncode,0)
                    self.assertIn('fresh absolute output',result.stderr)
                    self.assertFalse((root/'calls').exists())
                    self.assertEqual(sentinel.read_text(),'original')
                    self.assertEqual(Path(str(logged)+'.build.log').read_text(),'original log')

if __name__=='__main__': unittest.main()
