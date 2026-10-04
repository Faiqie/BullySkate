"""Derive connected, convex grind ledges from the player's own Bully map."""
from collections import defaultdict
import numpy as np,struct

def connect_segments(segments):
    points={};neighbors=defaultdict(set)
    for first,last in segments:
        a=tuple(np.rint(first*1000).astype(np.int64));b=tuple(np.rint(last*1000).astype(np.int64))
        if a==b:continue
        points[a]=first;points[b]=last;neighbors[a].add(b);neighbors[b].add(a)
    def through(key):
        ends=list(neighbors[key])
        if len(ends)!=2:return False
        a=points[ends[0]]-points[key];b=points[ends[1]]-points[key]
        return np.dot(a,b)/np.linalg.norm(a)/np.linalg.norm(b)<-0.57
    unused={tuple(sorted((a,b))) for a,ends in neighbors.items() for b in ends}
    result=[]
    starts=sorted(neighbors,key=lambda k:(through(k),k))
    for start in starts:
        for second in sorted(neighbors[start]):
            edge=tuple(sorted((start,second)))
            if edge not in unused:continue
            unused.remove(edge);chain=[start,second];previous,current=start,second
            while current!=start and through(current):
                nxt=next(k for k in neighbors[current] if k!=previous)
                edge=tuple(sorted((current,nxt)))
                if edge not in unused:break
                unused.remove(edge);chain.append(nxt);previous,current=current,nxt
            p=np.array([points[k] for k in chain])
            if np.linalg.norm(np.diff(p,axis=0),axis=1).sum()>=0.6:result.append(p)
    return result

def exposed_ledges(p,n,edges,order,starts,ends):
    """Open top edges qualify only where the outside really drops away."""
    isolated=order[starts[ends-starts==1]]
    a=p[isolated//3,isolated%3];b=p[isolated//3,(isolated%3+1)%3]
    delta=b-a;length=np.linalg.norm(delta,axis=1);normal=n[isolated//3]
    select=(normal[:,1]>.65)&(length>=.1)&(length<100)&(np.abs(delta[:,1])<np.linalg.norm(delta[:,[0,2]],axis=1)*.6)
    a,b,delta,normal=a[select],b[select],delta[select],normal[select]
    if len(a)==0:return np.empty((0,2,3))
    lo=p.min(1);hi=p.max(1);grid=defaultdict(list)
    for i,(first,last) in enumerate(zip(np.floor(lo[:,[0,2]]/10).astype(int),np.floor(hi[:,[0,2]]/10).astype(int))):
        if np.prod(last-first+1)>4096:continue
        for x in range(first[0],last[0]+1):
            for z in range(first[1],last[1]+1):grid[x,z].append(i)
    grid={key:np.array(value) for key,value in grid.items()}
    def ground(point,upper):
        ids=grid.get(tuple(np.floor(point[[0,2]]/10).astype(int)))
        if ids is None:return None
        q=p[ids];v=q[:,1]-q[:,0];w=q[:,2]-q[:,0];offset=point-q[:,0]
        det=v[:,0]*w[:,2]-v[:,2]*w[:,0];valid=np.abs(det)>1e-7
        safe=np.where(valid,det,1);s=(offset[:,0]*w[:,2]-offset[:,2]*w[:,0])/safe;t=(v[:,0]*offset[:,2]-v[:,2]*offset[:,0])/safe
        height=q[:,0,1]+s*v[:,1]+t*w[:,1]
        ok=valid&(s>=-1e-4)&(t>=-1e-4)&(s+t<=1.0001)&(height<=upper+.04)&(height>upper-30)
        return height[ok].max() if np.any(ok) else None
    result=[]
    for first,last,edge,top in zip(a,b,delta,normal):
        outside=np.cross(edge,top);outside[1]=0;outside/=np.linalg.norm(outside)
        valid=True
        for fraction in (.25,.5,.75):
            point=first+edge*fraction;inside=ground(point-outside*.05,point[1]);other=ground(point+outside*.2,point[1])
            if inside is None or other is None or abs(inside-point[1])>.08 or point[1]-other<.12:valid=False;break
        if valid:result.append([first,last])
    return np.array(result).reshape(-1,2,3)

def sharp_edges(p,open_ledges=False):
    if len(p)==0:return np.empty((0,2,3))
    n=np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]);norm=np.linalg.norm(n,axis=1)
    p=p[norm>1e-7];n=n[norm>1e-7]/norm[norm>1e-7,None]
    if len(p)==0:return np.empty((0,2,3))
    _,ids=np.unique(np.rint(p.reshape(-1,3)*1000).astype(np.int64),axis=0,return_inverse=True)
    ids=ids.reshape(-1,3)
    # Duplicate collision faces otherwise turn a two-face edge into a rejected
    # four-face edge. Keep opposite windings, but collapse identical faces.
    orientation=n[np.arange(len(n)),np.argmax(np.abs(n),axis=1)]>0
    _,unique=np.unique(np.c_[np.sort(ids,axis=1),orientation],axis=0,return_index=True)
    p,n,ids=p[unique],n[unique],ids[unique]
    edges=np.stack([ids,np.roll(ids,-1,axis=1)],axis=2).reshape(-1,2)
    edges=np.sort(edges,axis=1);order=np.lexsort((edges[:,1],edges[:,0]));s=edges[order]
    starts=np.r_[0,np.flatnonzero(np.any(s[1:]!=s[:-1],axis=1))+1];ends=np.r_[starts[1:],len(order)]
    shared=starts[ends-starts==2];a=order[shared];b=order[shared+1];ta=a//3;tb=b//3
    na=n[ta];nb=n[tb];delta=p[ta,(a%3+1)%3]-p[ta,a%3]
    length=np.linalg.norm(delta,axis=1);horizontal=np.linalg.norm(delta[:,[0,2]],axis=1)
    dot=np.einsum('ij,ij->i',na,nb);convex=np.einsum('ij,ij->i',delta,np.cross(na,nb))>=-1e-6
    top_side=((na[:,1]>0.45)&(nb[:,1]<0.45))|((nb[:,1]>0.45)&(na[:,1]<0.45))
    mask=top_side&convex&(dot<0.94)&(length>=0.03)&(length<100)&(np.abs(delta[:,1])<horizontal*0.85)
    selected=a[mask]
    result=np.stack([p[selected//3,selected%3],p[selected//3,(selected%3+1)%3]],axis=1)
    if open_ledges:result=np.concatenate([result,exposed_ledges(p,n,edges,order,starts,ends)])
    return result

def export(root,grind_models=None):
    raw=(root/'world.bmgeo').read_bytes()
    if raw[:8]!=b'BMGEO2\0\0':raise ValueError('Invalid collision header')
    records=np.frombuffer(raw[12:],dtype=np.dtype([('p','<f4',(3,3)),('material','<u2'),('area','<u2'),('model','<u4')]))
    if grind_models is not None:records=records[np.isin(records['model'],grind_models)]
    # Foliage and water surfaces are not usable ledges; the map itself is unchanged.
    records=records[~np.isin(records['material'],[30,31,43])]
    output=[];reports={};primitive_count=0
    for area in np.unique(records['area']):
        p=records['p'][records['area']==area];edges=sharp_edges(p,open_ledges=True);chains=connect_segments(edges)
        for chain in chains:
            closed=np.linalg.norm(chain[0]-chain[-1])<0.001
            if closed:chain=chain[:-1]
            if len(chain)>65535:raise ValueError('Grind chain is too long')
            output.append(struct.pack('<HHI',int(area),int(closed),len(chain))+np.asarray(chain,dtype='<f4').tobytes())
            primitive_count+=len(chain) if closed else len(chain)-1
        reports[int(area)]={'triangles':len(p),'sharp_edges':len(edges),'rails':len(chains)}
    if len(output)>65535:raise ValueError('Too many grind rails')
    (root/'world.bmrails').write_bytes(b'BMRL3\0\0\0'+struct.pack('<I',len(output))+b''.join(output))
    return {'areas':reports,'convex_top_grind_edges':primitive_count,'connected_rails':len(output)}
