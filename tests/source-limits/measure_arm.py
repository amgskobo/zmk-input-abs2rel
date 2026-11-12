#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Recompile matched real Cortex-M objects and retain state/text/frame evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
from elftools.elf.elffile import ELFFile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('default_build', type=Path)
    parser.add_argument('limited_build', type=Path)
    parser.add_argument('baseline_owner', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if not args.output.is_absolute() or args.output.exists() or args.output.is_symlink():
        parser.error('fresh absolute output required')
    args.output.mkdir(parents=True, exist_ok=False)
    result = {}
    for name, build, baseline in (('baseline', args.default_build, True),
                                   ('default', args.default_build, False),
                                   ('limited', args.limited_build, False)):
        rows = json.loads((build / 'compile_commands.json').read_text())
        rows = [row for row in rows if row['file'].endswith('/input_processor_absolute_to_relative.c')]
        if len(rows) != 1:
            raise RuntimeError('expected exactly one production converter compile command')
        row = rows[0]
        command = row.get('arguments') or shlex.split(row['command'])
        command[command.index('-o') + 1] = str(args.output / (name + '.o'))
        if baseline:
            source = args.baseline_owner / 'drivers/input/input_processor_absolute_to_relative.c'
            command[command.index(row['file'])] = str(source)
        command += ['-fstack-usage']
        with (args.output / (name + '.compile.log')).open('xb') as log:
            process = subprocess.run(command, cwd=row['directory'], stdout=log, stderr=subprocess.STDOUT)
        (args.output / (name + '.command.json')).write_text(json.dumps(
            dict(command=command, cwd=row['directory'], exit=process.returncode), indent=2) + '\n')
        if process.returncode:
            raise RuntimeError(name + ' compile failed')
        prefix = str(Path(command[0]).parent / 'arm-zephyr-eabi-')
        obj = args.output / (name + '.o')
        symbols = subprocess.check_output([prefix + 'nm', '-S', '--size-sort', str(obj)], text=True)
        (args.output / (name + '.symbols.txt')).write_text(symbols)
        disassembly = subprocess.check_output([prefix + 'objdump', '-dr', str(obj)], text=True)
        (args.output / (name + '.disassembly.txt')).write_text(disassembly)
        text, state = {}, {}
        for address, size, kind, symbol in re.findall(r'^(\w+) (\w+) ([a-zA-Z]) (.+)$', symbols, re.M):
            if kind.lower() == 't':
                text[symbol] = int(size, 16)
            elif kind.lower() in ('b', 'd'):
                state[symbol] = int(size, 16)
        frames = (args.output / (name + '.su')).read_text()
        with obj.open('rb') as stream:
            elf = ELFFile(stream)
            code_sections = {section.name: hashlib.sha256(section.data()).hexdigest()
                             for section in elf.iter_sections() if section.name.startswith('.text')}
        result[name] = dict(object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
                            functions=text, function_text_bytes=sum(text.values()),
                            code_sections=code_sections, data=state, data_bytes=sum(state.values()), frames=frames)
    assert result['baseline']['functions'] == result['default']['functions'], 'default function text changed'
    assert result['baseline']['data'] == result['default']['data'], 'default state changed'
    assert result['baseline']['code_sections'] == result['default']['code_sections'], 'default code bytes changed'
    assert result['baseline']['data_bytes'] - result['limited']['data_bytes'] == 84, 'expected 3 unused 28-byte source slots'
    # Compare frame bytes by function, ignoring source paths and line numbers.
    def frame_sizes(text):
        return {line.split('\t')[0].rsplit(':', 1)[1]: line.split('\t')[1:]
                for line in text.splitlines()}
    assert frame_sizes(result['baseline']['frames']) == frame_sizes(result['default']['frames']), 'default frames changed'
    assert frame_sizes(result['default']['frames']) == frame_sizes(result['limited']['frames']), 'limited frames changed'
    (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Cortex-M matched baseline/default/limited state/text/frames: PASS')


if __name__ == '__main__':
    main()
