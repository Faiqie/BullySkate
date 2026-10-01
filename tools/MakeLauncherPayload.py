"""Embed the redistributable runtime, not the user's extracted Skate 3 data."""
from pathlib import Path
import sys
import zipfile
package, destination = map(Path, sys.argv[1:])
files = [package / 'deployment.sha256']
for line in files[0].read_text().splitlines():
    _, relative = line.split('  ', 1)
    if any(part in ('private','skate-assets','assets') for part in Path(relative).parts):
        raise SystemExit('Do not embed locally prepared game assets')
    if Path(relative).suffix.lower() in ('.xex','.img','.dir','.nif','.nft','.dds','.abin','.bmgeo','.bmrails','.vlt'):
        raise SystemExit('Game data cannot enter a public launcher')
    files.append(package / relative)
destination.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for file in files:
        archive.write(file, file.relative_to(package).as_posix())
print(f'Embedded payload: {len(files)} files, {destination.stat().st_size:,} bytes')
