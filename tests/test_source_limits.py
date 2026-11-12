# SPDX-License-Identifier: MIT
"""Canonical dispatch retains original sparse and new limited profiles."""
import importlib.util
import os
from pathlib import Path
from unittest import TestCase, mock


class SourceProfiles(TestCase):
    def test_canonical_plan_is_complete_despite_ambient_selectors(self):
        path = Path(__file__).parent / 'source-limits/run.py'
        spec = importlib.util.spec_from_file_location('abs2rel_source_profiles', path)
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        with mock.patch.dict(os.environ, {'MAX_SOURCES': '2', 'LOW_SOURCES': '1',
                                         'ABS2REL_MAX_SOURCES': '2', 'ZMK_TEST_MODE': 'quick'}):
            cases = runner.plan()
        self.assertEqual(len(cases), 13)
        self.assertEqual(cases[0], ('auto-sparse', 0, None, None))
        self.assertIn(('auto-low', 0, 'low', None), cases)
        self.assertIn(('limited', 2, 'low', None), cases)
        self.assertIn(('wide', 256, 'low', None), cases)
        self.assertIn(('fallback', 0, 'no-listeners', None), cases)
        self.assertIn(('reject-sparse', 2, None, runner.REJECTION), cases)
        self.assertEqual(runner.plan('reject-child'),
                         (('reject-child', 2, 'child', runner.REJECTION),))
