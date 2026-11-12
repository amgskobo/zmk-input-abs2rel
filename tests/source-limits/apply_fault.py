#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Create one named guard fault in a separate disposable runtime owner copy."""
from pathlib import Path
import sys

owner = Path(__file__).resolve().parents[2]
fault, target = sys.argv[1:]
target = Path(target).resolve()
if target == owner or owner in target.parents or target in owner.parents:
    raise SystemExit('fault target must be a distinct disposable runtime copy')
source = target / 'drivers/input/input_processor_absolute_to_relative.c'
text = source.read_text()
needle = {
    'base': 'ABS2REL_CHECK_ROUTE(DT_INST(source, compat), source)',
    'child': 'DT_FOREACH_CHILD_VARGS(DT_INST(source, compat), ABS2REL_CHECK_ROUTE, source)',
    'device': 'DT_NODE_HAS_STATUS(DT_PHANDLE(DT_INST(source, compat), device), okay)',
}[fault]
if text.count(needle) != 1:
    raise SystemExit('expected exactly one original guard')
source.write_text(text.replace(needle, '1' if fault == 'device' else '/* named missing-guard control */'))
