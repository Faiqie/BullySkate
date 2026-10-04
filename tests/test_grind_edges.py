import importlib.util, unittest
from pathlib import Path
import numpy as np
spec=importlib.util.spec_from_file_location('rails',Path(__file__).resolve().parents[1]/'extractor/bully_rails.py')
rails=importlib.util.module_from_spec(spec);spec.loader.exec_module(rails)

class GrindGeometryTests(unittest.TestCase):
    def test_duplicate_faces_do_not_remove_a_real_ledge(self):
        top=[[0,1,0],[2,1,0],[0,1,-2]];side=[[2,1,0],[0,1,0],[0,-1,0]]
        self.assertEqual(len(rails.sharp_edges(np.array([top,side,top,side],dtype=float))),1)

    def test_open_top_ledge_requires_a_real_drop_not_a_ground_seam(self):
        top=np.array([[[0,1,0],[2,1,0],[0,1,-2]],[[2,1,0],[2,1,-2],[0,1,-2]]],dtype=float)
        floor=np.array([[[-3,0,3],[5,0,3],[-3,0,-5]],[[5,0,3],[5,0,-5],[-3,0,-5]]],dtype=float)
        edges=rails.sharp_edges(np.concatenate([top,floor]),open_ledges=True)
        self.assertGreaterEqual(len(edges),4)
        flat=top.copy();flat[:, :, 1]=0
        self.assertEqual(len(rails.sharp_edges(np.concatenate([flat,floor]),open_ledges=True)),0)

    def test_convex_ledge_is_detected_but_concave_and_flat_seams_are_not(self):
        top=[[0,1,0],[2,1,0],[0,1,-2]]
        self.assertEqual(len(rails.sharp_edges(np.array([top,[[2,1,0],[0,1,0],[0,-1,0]]],dtype=float))),1)
        self.assertEqual(len(rails.sharp_edges(np.array([top,[[2,1,0],[0,1,0],[0,3,0]]],dtype=float))),0)
        self.assertEqual(len(rails.sharp_edges(np.array([top,[[2,1,0],[0,1,0],[2,1,2]]],dtype=float))),0)

    def test_short_connected_segments_form_a_continuous_grind(self):
        points=np.array([[i*.2,1,0] for i in range(8)])
        result=rails.connect_segments(np.stack([points[:-1],points[1:]],axis=1))
        self.assertEqual(len(result),1);self.assertEqual(len(result[0]),8)

    def test_gaps_and_junctions_are_not_bridged(self):
        segments=np.array([[[0,1,0],[1,1,0]],[[1.01,1,0],[2.01,1,0]]])
        self.assertEqual(len(rails.connect_segments(segments)),2)
        junction=np.array([[[0,1,0],[1,1,0]],[[1,1,0],[2,1,0]],[[1,1,0],[1,1,1]]])
        self.assertEqual(len(rails.connect_segments(junction)),3)

    def test_smooth_closed_ledge_retains_a_closed_chain(self):
        angles=np.arange(17)*2*np.pi/16
        p=np.array([np.cos(angles),np.ones(17),np.sin(angles)]).T
        chains=rails.connect_segments(np.stack([p[:-1],p[1:]],axis=1))
        self.assertEqual(len(chains),1);self.assertLess(np.linalg.norm(chains[0][0]-chains[0][-1]),.001)

if __name__=='__main__':unittest.main()
