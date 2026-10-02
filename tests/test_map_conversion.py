import importlib.util, tempfile, unittest, struct
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
def load(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'extractor'/f'{name}.py')
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
geometry=load('bully_geometry');rails=load('bully_rails')

def col(model,kind):
    raw=bytearray(84);raw[:4]=b'COL3'
    struct.pack_into('<HH',raw,8,4,kind);struct.pack_into('<I',raw,32,model)
    struct.pack_into('<3f',raw,52,-1,-1,-1);struct.pack_into('<3f',raw,68,1,1,1)
    raw+=struct.pack('<III',0,0,1)+struct.pack('<3fI3fIB3x',-1,-1,-1,0,1,1,1,0,0)+struct.pack('<II',0,0)
    struct.pack_into('<I',raw,4,len(raw)-8);return raw

def placement(model,area):
    r=bytearray(120);struct.pack_into('<I',r,0,model)
    r[4:9]=b'test\0';struct.pack_into('<f3f3f4f',r,68,area,0,0,0,1,1,1,0,0,0,1)
    return r

class MapConversionTests(unittest.TestCase):
    def test_navigation_channels_do_not_become_solid_walls(self):
        with tempfile.TemporaryDirectory() as tmp:
            game=Path(tmp)/'game';stream=game/'Stream';stream.mkdir(parents=True)
            models=b''.join(col(i,1) for i in [286,*range(290,298)])+col(7000,1)+col(7001,2)+col(7002,3)
            ipb=bytearray(31);ipb[23:27]=b'inst';struct.pack_into('<I',ipb,27,4)
            ipb+=b''.join(placement(i,0) for i in [7000,7001,7002])+placement(7000,-1)
            archive=bytearray();directory=bytearray()
            for name,data in [('test.col',models),('test.ipb',ipb)]:
                sector=len(archive)//2048;size=(len(data)+2047)//2048
                directory+=struct.pack('<II24s',sector,size,name.encode())
                archive+=data+bytes(size*2048-len(data))
            (stream/'World.img').write_bytes(archive);(stream/'World.dir').write_bytes(directory)
            out=Path(tmp)/'out';geometry.export(game,out)
            raw=(out/'world.bmgeo').read_bytes()
            self.assertEqual(struct.unpack_from('<I',raw,8)[0],12)
            self.assertEqual({struct.unpack_from('<I',r,40)[0] for r in [raw[i:i+44] for i in range(12,len(raw),44)]},{7000})
    def test_short_segments_form_a_continuous_grind(self):
        pieces=np.array([[[i*.2,1,0],[(i+1)*.2,1,0]] for i in range(10)])
        result=rails.connect_edges(pieces)
        self.assertEqual(len(result),1);self.assertEqual(len(result[0]),11)
        self.assertAlmostEqual(np.linalg.norm(np.diff(result[0],axis=0),axis=1).sum(),2)
    def test_rounded_ledge_joins_but_sharp_corners_do_not(self):
        points=np.array([[np.cos(t),1,np.sin(t)] for t in np.linspace(0,1.5,12)])
        result=rails.connect_edges(np.stack([points[:-1],points[1:]],axis=1))
        self.assertEqual(len(result),1)
        result=rails.connect_edges(np.array([[[0,1,0],[1,1,0]],[[1,1,0],[1,1,1]]]))
        self.assertEqual(len(result),2)
    def test_no_grind_connects_across_a_gap(self):
        result=rails.connect_edges(np.array([[[0,1,0],[1,1,0]],[[1.1,1,0],[2.1,1,0]]]))
        self.assertEqual(len(result),2)

if __name__=='__main__':unittest.main()
