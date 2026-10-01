"""Project interoperability hashes into the native startup checker; no game bytes."""
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
profile = ET.parse(root / 'compatibility/pc-1.200.xml').getroot()
lines = ['/* Generated from compatibility/pc-1.200.xml; SHA-256 metadata only. */',
         'typedef struct BullyCodeProbe { DWORD address,length; unsigned char hash[32]; } BullyCodeProbe;',
         'typedef struct BullyDataProbe { DWORD address,length; int writable; } BullyDataProbe;',
         'static const BullyCodeProbe bullyCodeProbes[]={']
for probe in profile.findall('code/probe'):
    digest = bytes.fromhex(probe.get('sha256'))
    assert len(digest) == 32
    assert 0 < int(probe.get('length')) <= 55
    lines.append(' {0x%s,%s,{%s}},' % (probe.get('address'), probe.get('length'), ','.join('0x%02X' % byte for byte in digest)))
lines += ['};', 'static const BullyDataProbe bullyDataProbes[]={']
for region in profile.findall('regions/region'):
    lines.append(' {0x%s,%s,%s},' % (region.get('address'), region.get('length'), '1' if region.get('writable') == 'true' else '0'))
lines += ['};', '']
(root / 'native/game_layout_fingerprints.h').write_bytes('\n'.join(lines).encode('ascii'))
print('Projected native engine fingerprints from the trusted launcher profile.')
