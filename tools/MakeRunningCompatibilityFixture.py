"""Generate metadata for an authored test executable, never a game file."""
from pathlib import Path
import hashlib, sys, xml.etree.ElementTree as ET
import pefile

folder = Path(sys.argv[1])
if len(sys.argv) == 2:
    lengths = [0, 3, 7, 16, 55]
    rows = [','.join('0x%02X' % byte for byte in hashlib.sha256(bytes(range(length))).digest()) for length in lengths]
    text = 'static const unsigned shaLengths[]={'+','.join(map(str, lengths))+'};\nstatic const unsigned char shaExpected[][32]={\n'+'\n'.join('{'+row+'},' for row in rows)+'\n};\n'
    (folder / 'native-sha-vectors.h').write_bytes(text.encode())
else:
    pe = pefile.PE(str(folder / 'Bully.exe'))
    names = {entry.name.decode().lstrip('_'):pe.OPTIONAL_HEADER.ImageBase+entry.address for entry in pe.DIRECTORY_ENTRY_EXPORT.symbols if entry.name}
    layout = ET.Element('bully-layout', {'id':'authored-running-fixture', 'machine':'014C', 'image-base':'00400000'})
    code = ET.SubElement(layout, 'code')
    for name, content in [('compatCodeA', bytes(range(1,17))), ('compatCodeB', bytes(range(0x91,0xA1)))]:
        ET.SubElement(code, 'probe', {'address':'%08X' % names[name], 'length':'16', 'sha256':hashlib.sha256(content).hexdigest()})
    data = ET.SubElement(layout, 'regions')
    ET.SubElement(data, 'region', {'address':'%08X' % names['compatData'], 'length':'4', 'writable':'true'})
    (folder / 'fixture.xml').write_bytes(ET.tostring(layout))
