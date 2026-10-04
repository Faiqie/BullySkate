"""Copy license notices for the separate, locked GPL audio-worker dependency graph."""
import json,subprocess,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
metadata=json.loads(subprocess.check_output(['cargo','metadata','--locked','--format-version','1','--manifest-path',str(root/'audio-worker/Cargo.toml')]))
rows=[]
for package in metadata['packages']:
    if package['source'] is None:continue
    notices=[];folder=Path(package['manifest_path']).parent
    for file in folder.iterdir():
        if file.is_file() and file.name.upper().startswith(('LICENSE','COPYING','NOTICE','LICENCE','AUTHORS')):
            target=root/'licenses/audio-rust'/f"{package['name']}-{package['version']}"/file.name
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(file,target)
            notices.append(target.relative_to(root).as_posix())
    rows.append({key:package[key] for key in ('name','version','license','repository')}|{'notices':notices})
(root/'licenses/audio-rust-dependencies.json').write_text(json.dumps(rows,indent=2)+'\n')
print(f'Collected {len(rows)} locked audio dependency notices.')
