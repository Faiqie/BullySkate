"""Reject game data and local state in tracked source and the embedded payload."""
from pathlib import Path
import json, subprocess, sys, zipfile, hashlib, re

ROOT=Path(__file__).resolve().parents[1]
FORBIDDEN={'.xex','.img','.dir','.nif','.nft','.dds','.abin','.bmgeo','.bmrails','.rwcmset','.vlt'}
PRIVATE={'private','skate-assets','bully-assets','assets'}
NAMES={'jimmy-bind.json','vehicle-bounds.txt','console-setup.xml','extraction-receipt.json','source-path.txt','viewmodel_data.h','stock.rs','stock_fields.rs'}

def check_name(name):
    path=Path(name)
    if path.suffix.lower() in FORBIDDEN or set(path.parts)&PRIVATE or path.name in NAMES:
        raise ValueError('Game data/local state is forbidden: '+name)
    if path.name.startswith(('BullyFile','FileTableBully')):
        raise ValueError('Save file is forbidden: '+name)
    if path.is_absolute() or '..' in path.parts:
        raise ValueError('Unsafe package path: '+name)

def source_files():
    if (ROOT/'.git').exists():
        result=subprocess.check_output(['git','-C',str(ROOT),'ls-files','--cached','--others','--exclude-standard','-z'])
        return [n.decode() for n in result.split(b'\0') if n]
    files=[]
    for file in ROOT.rglob('*'):
        if not file.is_file():continue
        relative=file.relative_to(ROOT)
        if relative.parts[0] in ('work','prepared','release','target') or '__pycache__' in relative.parts:continue
        if file.suffix.lower() in ('.pyc','.obj','.pdb','.map','.log'):continue
        if file.name=='BullySkateLauncher.exe' or file.name=='mopper.exe':continue
        if relative.parts[0]=='runtime' and file.suffix.lower() in ('.exe','.asi'):continue
        files.append(relative.as_posix())
    return files

def audit(payload=None):
    names=source_files()
    for name in names:
        check_name(name)
        file=ROOT/name
        if file.suffix.lower() in ('.cs','.rs','.ps1','.py','.toml','.lua','.md','.txt'):
            if re.search(rb'[A-Za-z]:[/\\]Users[/\\][^/\\\s]+',file.read_bytes()):
                raise ValueError('Personal build path found: '+name)
    count=0
    if payload is not None:
        with zipfile.ZipFile(payload) as archive:
            listed=archive.namelist()
            if len(listed)!=len(set(listed)):raise ValueError('Duplicate embedded payload entry')
            for name in listed:check_name(name)
            lines=archive.read('deployment.sha256').decode().splitlines()
            expected={}
            for line in lines:
                sha,name=line.split('  ',1);check_name(name)
                if name in expected:raise ValueError('Duplicate deployment entry')
                if hashlib.sha256(archive.read(name)).hexdigest()!=sha:raise ValueError('Bad embedded checksum: '+name)
                expected[name]=sha
            if set(listed)!=set(expected)|{'deployment.sha256'}:raise ValueError('Unlisted payload entry')
            count=len(listed)
    report={'source_files':len(names),'payload_files':count,'game_data_included':False,'local_paths_included':False}
    print(json.dumps(report,indent=2));return report

if __name__=='__main__':
    payload=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'work/launcher-payload.zip'
    try:audit(payload if payload.exists() else None)
    except (ValueError,OSError,KeyError,zipfile.BadZipFile) as error:raise SystemExit(str(error))
