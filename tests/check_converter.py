# SPDX-License-Identifier: MIT
"""Check the converter fixture's active instances and allocated driver state."""
import argparse
import json
from pathlib import Path
import re
import sys


def validate(dts, zephyr, legacy=False, linker_map=None):
    sys.path.insert(0,str(Path(zephyr)/'scripts/dts/python-devicetree/src'))
    from devicetree import dtlib
    tree=dtlib.DT(str(dts))
    nodes=[n for n in tree.node_iter() if 'compatible' in n.props and
           'zmk,input-processor-absolute-to-relative' in n.props['compatible'].to_strings() and
           ('status' not in n.props or n.props['status'].to_string()=='okay')]
    expected={'abs_rel','abs_rel_scroll'} if legacy else {'abs_rel'}
    assert {n.name for n in nodes}==expected, 'unexpected active converter instances'
    result=dict(active=[n.path for n in nodes],legacy_enabled=legacy)
    if linker_map is not None:
        text=Path(linker_map).read_text()
        entries=[dict(section=section,address=int(address,16),bytes=int(size,16))
                 for section,address,size in re.findall(r'^\s*\.bss\.(data_\d+)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)[^\n]*input_processor_absolute_to_relative\.c\.obj',text,re.M)]
        entries=[e for e in entries if e['address'] and e['bytes']]
        assert len(entries)==len(expected) and len({e['section'] for e in entries})==len(expected), 'unexpected allocated converter histories'
        result.update(states=entries,state_bytes=sum(e['bytes'] for e in entries))
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dts',type=Path,required=True);p.add_argument('--zephyr',type=Path,required=True)
    p.add_argument('--map',type=Path);p.add_argument('--legacy',action='store_true')
    p.add_argument('--record',type=Path,required=True);a=p.parse_args()
    if a.record.exists() or a.record.is_symlink(): raise FileExistsError(a.record)
    result=validate(a.dts,a.zephyr,a.legacy,a.map)
    with a.record.open('x') as f: json.dump(result,f,indent=2)
    print('PASS: converter instance selection and allocated state',result)

if __name__=='__main__': main()
