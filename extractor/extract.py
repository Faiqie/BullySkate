"""Extract only the owned-disc data consumed by the Bully skating host."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'vendor/python'))
from tools.owned_game.big import BigArchive
from tools.asset_pipeline.vlt import convert as convert_vlt
from tools.asset_pipeline.physics_skeleton import convert as convert_skeleton

GAME = {"version": 1, "character_scene": "private/skater.glb",
        "initial_animation": "R_IDLE_HCOM_000",
        "action_graph": "private/stock/data/state/ActionGraph_OnBoard.stategraph",
        "motion_graph": "private/stock/data/state/MotionGraph_OnBoard.stategraph"}

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def expected_files(path):
    result = {}
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        sha, relative = line.split('  ', 1)
        BigArchive.safe_relative(relative)
        if len(sha) != 64 or any(c not in '0123456789abcdef' for c in sha.lower()):
            raise ValueError('Invalid expected asset checksum')
        if relative in result:
            raise ValueError('Duplicate expected asset path')
        result[relative] = sha.lower()
    if len(result) != 18:
        raise ValueError('Incomplete required asset list')
    return result

def extract_selected(archive, output, wanted):
    big = BigArchive(archive)
    wanted = set(wanted)
    entries = [e for e in big.entries if e.path.lower() in wanted]
    if len({e.path.lower() for e in entries}) != len(entries):
        raise ValueError('Duplicate required entry in ' + str(archive))
    big.extract_entries(entries, output)
    return {e.path.lower() for e in entries}

def extract(xex, destination, manifest):
    xex = xex.resolve(strict=True)
    if xex.name.lower() != 'default.xex' or xex.open('rb').read(4) != b'XEX2':
        raise ValueError('Choose Skate 3 default.xex from an extracted Xbox 360 game folder.')
    source = xex.parent
    for name in ('miscload.big', 'miscboot.big', 'db.big'):
        if not (source / 'data/big' / name).is_file():
            raise ValueError('The XEX alone is not enough. Missing data/big/' + name +
                             '. Keep the complete extracted Skate 3 folder together.')
    expected = expected_files(manifest)
    destination = destination.resolve()
    if destination.exists():
        raise ValueError('Extraction destination already exists; use a new staging directory.')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='skate-extract-', dir=destination.parent) as temp:
        stage = Path(temp) / 'assets'
        stock = stage / 'private/stock'
        stock.mkdir(parents=True)
        wanted = {p.removeprefix('private/stock/').lower()
                  for p in expected if p.startswith('private/stock/data/')}
        print('Extracting animation banks, state graphs, camera and controller data...', flush=True)
        found = extract_selected(source / 'data/big/miscload.big', stock, wanted)
        found |= extract_selected(source / 'data/big/miscboot.big', stock, wanted - found)
        for relative in sorted(wanted - found):
            loose = source / relative
            if not loose.is_file():
                raise ValueError('Required Skate 3 file not found: ' + relative)
            target = stock / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(loose, target)
        # The source pipeline prefers loose animation banks when present.
        for name in ('OnBoard.abin', 'OffBoard.abin'):
            loose = source / 'data/anim' / name
            if loose.is_file():
                shutil.copyfile(loose, stock / 'data/anim' / name)
        print('Converting the source physics database and skeleton...', flush=True)
        database = Path(temp) / 'database'
        needed = {'data/db/' + stem + extension
                  for stem in ('skaterschema', 'skatercollections')
                  for extension in ('.bin', '.vlt')}
        found = extract_selected(source / 'data/big/db.big', database, needed)
        if found != needed:
            raise ValueError('Skate 3 physics database is incomplete.')
        names = (Path(__file__).parent / 'tools/asset_pipeline/names.txt').read_text().splitlines()
        collections = convert_vlt(database / 'data/db/skaterschema',
                                  database / 'data/db/skatercollections', names)
        (stock / 'skater-collections.json').write_text(json.dumps(collections), encoding='utf-8')
        skeleton = convert_skeleton(stock / 'data/anim/OnBoard.abin')
        (stock / 'physics-skeletons.json').write_text(json.dumps(skeleton), encoding='utf-8')
        (stage / 'private/game.json').write_text(json.dumps(GAME), encoding='utf-8')
        print('Verifying every extracted file against the supported source data...', flush=True)
        for relative, sha in expected.items():
            if digest(stage / relative) != sha:
                raise ValueError('Unsupported or altered Skate 3 data: ' + relative +
                                 '. This build supports the verified Xbox 360 source edition.')
        shutil.copyfile(manifest, stage / 'asset-manifest.sha256')
        receipt = {'version': 1, 'xex': str(xex), 'xex_sha256': digest(xex),
                   'files': expected, 'source': 'local Skate 3 Xbox 360 archives'}
        (stage / 'extraction-receipt.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
        stage.rename(destination)
    print('Extraction verified: 18 required files; no Skate 3 character mesh needed.', flush=True)

def extract_bully(game, destination):
    import bully_geometry, bully_rails, bully_bind
    game=game.resolve(strict=True)
    if game.is_file():game=game.parent
    for name in ('Stream/World.dir','Stream/World.img'):
        if not (game/name).is_file():raise ValueError('Select the complete Bully folder. Missing '+name)
    directory=game/'Stream/World.dir';image=game/'Stream/World.img'
    raw=directory.read_bytes()
    if len(raw)%32:raise ValueError('Invalid Bully world directory')
    import struct
    for offset in range(0,len(raw),32):
        sector,size,_=struct.unpack_from('<II24s',raw,offset)
        if (sector+size)*2048>image.stat().st_size:raise ValueError('Truncated Bully world archive')
    destination=destination.resolve()
    if destination.exists():raise ValueError('Bully preparation destination already exists')
    destination.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='bully-prepare-',dir=destination.parent) as temp:
        stage=Path(temp)/'assets';stage.mkdir()
        print('Preparing collision and car bounds from your Bully World.img...',flush=True)
        world=bully_geometry.export(game,stage)
        print('Preparing area-specific grind rails...',flush=True)
        rails=bully_rails.export(stage)
        print('Preparing Jimmy\'s native bind rig...',flush=True)
        bully_bind.export(game,stage/'jimmy-bind.json')
        names=['jimmy-bind.json','vehicle-bounds.txt','world.bmgeo','world.bmrails']
        lines=[digest(stage/name)+'  '+name for name in names]
        (stage/'asset-manifest.sha256').write_text('\n'.join(lines)+'\n',encoding='ascii')
        (stage/'receipt.json').write_text(json.dumps({'schema':2,'game':str(game),'directory_sha256':digest(directory),
            'triangles':world['triangles'],'rails':rails['convex_top_grind_edges']},indent=2))
        stage.rename(destination)
    print('Bully preparation verified: collision, rails, rig and vehicle bounds; original archives preserved.',flush=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    choice=parser.add_mutually_exclusive_group(required=True)
    choice.add_argument('--xex',type=Path)
    choice.add_argument('--bully',type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--manifest', type=Path)
    args = parser.parse_args()
    try:
        if args.bully:extract_bully(args.bully,args.out)
        elif args.manifest:extract(args.xex,args.out,args.manifest)
        else:raise ValueError('--manifest is required with --xex')
    except (ValueError, OSError, RuntimeError) as error:
        print('Extraction failed: ' + str(error), file=sys.stderr, flush=True)
        sys.exit(1)
