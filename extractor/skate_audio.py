"""Prepare Skate 3 player audio locally. Runtime/format helpers are GPL-3.0-only."""
import hashlib,json,shutil,struct,tempfile,urllib.request,zipfile
from pathlib import Path
from tools.owned_game.big import BigArchive
from tools.audio_player import audio_export as audio

def decoder():
    import os
    root=Path(os.environ['LOCALAPPDATA'])/'BullySkate/downloads/vgmstream-r2117'
    root.mkdir(parents=True,exist_ok=True)
    archive=root/'official.zip'
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest()!=audio.VGMSTREAM_SHA:
        print('Downloading the verified Skate 3 audio decoder...',flush=True)
        urllib.request.urlretrieve(audio.VGMSTREAM_URL,archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=audio.VGMSTREAM_SHA:raise ValueError('Audio decoder verification failed')
    # Re-extract the verified software, never execute a previously changed cache.
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            relative=Path(entry.filename)
            if relative.is_absolute() or '..' in relative.parts:raise ValueError('Invalid decoder archive path')
        source.extractall(root/'bin')
    return root/'bin/vgmstream-cli.exe'

def extract(xex,skate_assets,destination):
    xex=xex.resolve(strict=True);game=xex.parent
    if destination.exists():raise ValueError('Audio preparation destination already exists')
    collections=json.loads((skate_assets/'private/stock/skater-collections.json').read_text())['collections']
    destination.parent.mkdir(parents=True,exist_ok=True)
    tool=decoder()
    with tempfile.TemporaryDirectory(prefix='skate-audio-',dir=destination.parent) as temp:
        stage=Path(temp)/'assets';out=stage/'private/audio';out.mkdir(parents=True)
        work=Path(temp)/'decode';work.mkdir()
        manifest={'version':5,'ambience':{},'grains':{},'wheels':{},'banks':{},'patches':{}}
        with (work/'decode.log').open('wb') as log:
            for kind,filename,suffix in [('grains','grains.big','.grain'),('wheels','wheels.big','.snr')]:
                print('Preparing Skate 3 '+kind+'...',flush=True)
                source=BigArchive(game/'data/audio'/filename);folder=work/kind;folder.mkdir();(out/kind).mkdir()
                members={}
                for entry in source.entries:
                    if not entry.path.endswith(suffix):continue
                    name=Path(entry.path).stem;data=source.read(entry);members[name]=data
                    stream=audio.grain(data).stream if suffix=='.grain' else audio.scan_snr(data)[0]
                    (folder/(name+'.snr')).write_bytes(audio.standalone(data,stream))
                audio._decode(tool,folder,[name+'.snr' for name in members],log)
                for name,data in members.items():
                    wav=folder/(name+'.snr.wav')
                    if kind=='grains':manifest[kind][name]=audio.grain_whole(name,data,wav,out/kind)
                    else:
                        shutil.move(wav,out/kind/(name+'.wav'))
                        manifest[kind][name]={'file':kind+'/'+name+'.wav',**audio._wav_info(out/kind/(name+'.wav'))}
            print('Preparing Skate 3 board, trick, landing and grind sounds...',flush=True)
            files=BigArchive(game/'data/audio/audiofiles.big');by_name={Path(e.path).name:e for e in files.entries}
            # Player sounds only: Bully keeps its own pedestrians, traffic, ambience and music.
            banks=tuple(n for n in audio.BANKS if not n.startswith(('water_lapping','fountains_','water_fountain','ocean_')))
            for name in banks:
                data=files.read(by_name[name]);stem=Path(name).stem;streams=audio._streams(name,data)
                folder=work/'banks'/stem;folder.mkdir(parents=True)
                for index,stream in enumerate(streams):(folder/f'{index:04d}.snr').write_bytes(audio.standalone(data,stream))
                audio._decode(tool,folder,[f'{i:04d}.snr' for i in range(len(streams))],log)
                target=out/'banks'/stem;target.mkdir(parents=True)
                samples=[]
                for index in range(len(streams)):
                    wav=target/f'{index:04d}.wav';shutil.move(folder/f'{index:04d}.snr.wav',wav)
                    samples.append({'file':f'banks/{stem}/{index:04d}.wav',**audio._wav_info(wav)})
                manifest['banks'][stem]=samples
                if data[:4]==b'SPLC':manifest['patches'][stem]=audio.splc_patches(data)
            manifest['grain_player']=audio.grain_tuning(collections)
            manifest['player_tuning']=audio.player_tuning(collections)
            manifest['bus_tuning']=audio.bus_tuning(collections)
            manifest['aems']=audio.aems_files(files,out,banks)
            manifest['aems']['mixmap']=audio.mixmap_file(game/'data/audio',out)
            manifest['aems']['splice']=audio.splice_trees(files,out,banks)
            if not manifest['aems']['mixmap']:raise ValueError('Skate 3 MixMap is missing')
            (out/'audio_manifest.json').write_text(json.dumps(manifest),encoding='utf-8')
        entries=[hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(stage).as_posix() for p in sorted(stage.rglob('*')) if p.is_file()]
        (stage/'asset-manifest.sha256').write_text('\n'.join(entries)+'\n',encoding='ascii')
        (stage/'receipt.json').write_text(json.dumps({'schema':1,'files':len(entries),'source':'local Skate 3 audio archives'}))
        stage.rename(destination)
    print('Skate 3 audio prepared from your files; no game audio is distributed.',flush=True)
