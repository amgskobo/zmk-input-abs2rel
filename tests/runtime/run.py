# SPDX-License-Identifier: MIT
"""Compile the complete unchanged production driver, never lifted definitions."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[2]
source=root/'drivers/input/input_processor_absolute_to_relative.c'
core=root/'include/zmk-input-abs2rel/absolute_to_relative_core.h'

def check_report(text, production):
    expected=set(re.findall(r'^static[^;{}]+?\b(\w+)\([^;{}]*\)\s*\{', production, re.M))
    functions={name:int(count) for name,count in re.findall(r'^function (\w+) called (\d+)',text,re.M)}
    executable=re.findall(r'^[ \t]*([^:\r\n]+):[ \t]*([1-9]\d*):',text,re.M)
    measured=[count.strip() for count,_ in executable if count.strip()!='-']
    if not measured:
        raise RuntimeError('coverage report has no executable lines')
    if not expected or not expected.issubset(functions) or any(functions[n]==0 for n in expected):
        raise RuntimeError('coverage report has no executable lines for a gated function')
    branches=re.findall(r'^branch.*',text,re.M)
    if any(c.startswith(('#####','=====')) for c in measured) or not branches or any(
            re.search(r'taken 0(?: |$)|never executed',b) for b in branches):
        raise RuntimeError('production line/branch was not executed')
    return len(expected),len(measured),len(branches)

def main():
    if len(sys.argv)!=1: raise SystemExit('usage: run.py (no selectors; complete driver only)')
    mode=os.environ.get('ZMK_TEST_MODE','quick')
    if mode not in ('quick','full'): raise SystemExit('ZMK_TEST_MODE must be quick or full')
    variants=[('sanitized',['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-sanitize-recover=all'])]
    if mode=='full': variants += [('optimized',['-O2']),('coverage',['-O0','--coverage'])]
    with tempfile.TemporaryDirectory(prefix='abs2rel-driver-') as folder:
        work=Path(folder)
        for label,flags in variants:
            binary=work/label
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',*flags,
                '-I'+str(root/'tests/support'),'-I'+str(root/'include'),
                str(root/'tests/runtime/driver_harness.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
        if mode=='full':
            subprocess.run(['gcov','-b','-c',str(work/'coverage-driver_harness.gcno')],cwd=work,check=True,capture_output=True)
            for path in (source,):
                report=(work/(path.name+'.gcov')).read_text(encoding='utf-8')
                counts=check_report(report,path.read_text(encoding='utf-8'))
                print(path.name, 'functions/lines/branches',counts,'complete PASS')

if __name__=='__main__': main()
