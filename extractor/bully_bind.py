"""Read Jimmy's original bind rig from the user's installation."""
import io,json,struct,time,logging,math
from pathlib import Path
time.clock=time.perf_counter
from pyffi.formats.nif import NifFormat
# IMG directory sizes include zero sector padding after the valid NIF footer.
# PyFFI logs that padding as an error; structural read failures still raise.
logging.getLogger('pyffi.nif').setLevel(logging.CRITICAL)
NAMES = ['Dummy', 'Root', 'Root Pelvis', 'Root L Thigh', 'Root L Calf', 'Root L Foot', 'Root R Thigh', 'Root R Calf', 'Root R Foot', 'Root01', 'Root Spine', 'Root Spine1', 'Root Spine2', 'Root Neck', 'Root Head', 'Root Ponytail1', 'Root EyeLids', 'Root Brow', 'Root Eyes', 'Root L Clavicle', 'Root L UpperArm', 'Root L Forearm', 'Root L Hand', 'Root L Finger0', 'Root L Finger1', 'Root L Finger11', 'Left_Shoulder', 'Root R Clavicle', 'Root R UpperArm', 'Root R Forearm', 'Root R Hand', 'Root R Finger0', 'Root R Finger1', 'Root R Finger11', 'Right_Shoulder', 'ARROW']
def export(game,output):
    directory=(game/'Stream/World.dir').read_bytes();payload=None
    with (game/'Stream/World.img').open('rb') as archive:
        for i in range(0,len(directory),32):
            offset,size,name=struct.unpack_from('<II24s',directory,i)
            if name.split(b'\0')[0].lower()==b'brownjacket.nif':
                archive.seek(offset*2048);payload=archive.read(size*2048);break
    if payload is None:raise ValueError('Jimmy model is missing from World.img')
    data=NifFormat.Data();data.read(io.BytesIO(payload));root=data.roots[0]
    nodes={b.name.decode():b for b in data.blocks if isinstance(b,NifFormat.NiNode)}
    parents={child.name.decode():parent.name.decode() for parent in nodes.values()
             for child in parent.children if isinstance(child,NifFormat.NiNode)}
    result=[]
    for i,name in enumerate(NAMES):
        if name not in nodes:raise ValueError('Missing native bind bone: '+name)
        parent=parents.get(name)
        while parent is not None and parent not in NAMES:parent=parents.get(parent)
        index=NAMES.index(parent) if parent is not None else -1
        if index>=i:raise ValueError('Unexpected native rig hierarchy')
        matrix=nodes[name].get_transform(root)
        values=[[getattr(matrix,f'm_{r}{c}') for c in range(1,4)] for r in range(1,5)]
        if not all(math.isfinite(v) for row in values for v in row):raise ValueError('Invalid native bind transform')
        result.append(dict(name=name,parent=index,matrix=values))
    output.write_text(json.dumps(result,indent=2),encoding='utf-8')
