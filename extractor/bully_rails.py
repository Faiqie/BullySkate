"""Area-specific grind candidates, prepared only on the user's computer."""
from pathlib import Path
import numpy as np,struct,json

def connect_edges(points):
    """Join short authored ledge pieces into continuous, gently turning rails."""
    if len(points)==0:return []
    flat=points.reshape(-1,3)
    _,first,ids=np.unique(np.rint(flat*1000).astype(np.int64),axis=0,return_index=True,return_inverse=True)
    vertices=flat[first];edges=ids.reshape(-1,2);adj={}
    for i,(a,b) in enumerate(edges):
        adj.setdefault(int(a),[]).append(i);adj.setdefault(int(b),[]).append(i)
    used=set();chains=[]
    order=sorted(range(len(edges)),key=lambda i: min(len(adj[int(v)]) for v in edges[i])==2)
    for seed in order:
        if seed in used:continue
        a,b=map(int,edges[seed])
        if len(adj[a])==2 and len(adj[b])!=2:a,b=b,a
        chain=[a,b];used.add(seed)
        while len(chain)<4096:
            previous,current=chain[-2:]
            candidates=[i for i in adj[current] if i not in used]
            if len(adj[current])!=2 or len(candidates)!=1:break
            i=candidates[0];other=int(edges[i,0] if edges[i,1]==current else edges[i,1])
            incoming=vertices[current]-vertices[previous];outgoing=vertices[other]-vertices[current]
            cosine=np.dot(incoming,outgoing)/(np.linalg.norm(incoming)*np.linalg.norm(outgoing))
            if cosine<0.64:break
            used.add(i);chain.append(other)
            if other==chain[0]:break
        rail=vertices[chain]
        if np.linalg.norm(np.diff(rail,axis=0),axis=1).sum()>=0.7:chains.append(rail)
    return chains
def export(root):
    raw=(root/'world.bmgeo').read_bytes()
    assert raw[:8]==b'BMGEO2\0\0'
    records=np.frombuffer(raw[12:],dtype=np.dtype([('p','<f4',(3,3)),('material','<u2'),('area','<u2'),('model','<u4')]))
    output=[];reports={}
    for area in np.unique(records['area']):
        p=records['p'][records['area']==area]
        n=np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]);n/=np.linalg.norm(n,axis=1)[:,None]
        _,ids=np.unique(np.rint(p.reshape(-1,3)*1000).astype(np.int64),axis=0,return_inverse=True)
        ids=ids.reshape(-1,3);edges=np.stack([ids,np.roll(ids,-1,axis=1)],axis=2).reshape(-1,2)
        se=np.sort(edges,axis=1);order=np.lexsort((se[:,1],se[:,0]));s=se[order]
        starts=np.r_[0,np.flatnonzero(np.any(s[1:]!=s[:-1],axis=1))+1];ends=np.r_[starts[1:],len(order)]
        pairs=starts[ends-starts==2];a=order[pairs];b=order[pairs+1];ta=a//3;tb=b//3
        na=n[ta];nb=n[tb];delta=p[ta,(a%3+1)%3]-p[ta,a%3]
        length=np.linalg.norm(delta,axis=1);horizontal=np.linalg.norm(delta[:,[0,2]],axis=1)
        dot=np.einsum('ij,ij->i',na,nb);convex=np.einsum('ij,ij->i',delta,np.cross(na,nb))>=-1e-6
        top_side=((na[:,1]>0.65)&(nb[:,1]<0.45))|((nb[:,1]>0.65)&(na[:,1]<0.45))
        mask=top_side&convex&(dot<0.85)&(length>0.12)&(length<100)&(np.abs(delta[:,1])<horizontal*0.85)
        selected=a[mask];points=np.stack([p[selected//3,selected%3],p[selected//3,(selected%3+1)%3]],axis=1)
        chains=connect_edges(points)
        for chain in chains:
            output.append(struct.pack('<HH',int(area),len(chain))+np.asarray(chain,dtype='<f4').tobytes())
        reports[int(area)]={'triangles':len(p),'rails':len(chains),'candidate_segments':len(points),'shared_edges':len(pairs)}
    if len(output)>65535:raise ValueError('Too many grind rails')
    (root/'world.bmrails').write_bytes(b'BMRL3\0\0\0'+struct.pack('<I',len(output))+b''.join(output))
    report={'areas':reports,'convex_top_grind_edges':len(output)}
    return report
