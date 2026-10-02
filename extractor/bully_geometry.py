"""Convert the selected user's Bully world archive; never ship the output."""
from pathlib import Path
import struct,numpy as np,collections,json
from types import SimpleNamespace
class BullyColFile:
    @staticmethod
    def from_bytes(data):
        models=[];offset=0
        while offset+8<=len(data) and data[offset:offset+4]==b'COL3':
            size=struct.unpack_from('<I',data,offset+4)[0]+8
            if size<100 or offset+size>len(data):raise ValueError('Invalid Bully COL record')
            raw=data[offset:offset+size]
            models.append(SimpleNamespace(raw=raw,object_id=struct.unpack_from('<I',raw,32)[0],
                major_type=struct.unpack_from('<H',raw,8)[0],minor_type=struct.unpack_from('<H',raw,10)[0],
                bounds=SimpleNamespace(min=struct.unpack_from('<3f',raw,52),max=struct.unpack_from('<3f',raw,68))))
            offset+=size
        return SimpleNamespace(models=models)

def physical_collision(model):
    # The other world COL channels contain NOGO/WALKABLE navigation volumes.
    # They overlap real scenery but are not solid geometry for the player.
    return model.minor_type == 1
def export_vehicle_bounds(models,path):
    lines=['BMVB1']
    for model in [286,*range(290,298)]:
        if model not in models:raise ValueError('Missing street car collision model')
        m=models[model];values=(*m.bounds.min,*m.bounds.max)
        if not all(np.isfinite(values)) or any(a>=b for a,b in zip(m.bounds.min,m.bounds.max)):
            raise ValueError('Invalid native vehicle bounds')
        lines.append(str(model)+' '+' '.join(format(v,'.9g') for v in values))
    path.write_text('\n'.join(lines)+'\n',encoding='ascii')
def export(game,output_root):
    directory=(game/'Stream/World.dir').read_bytes();entries={}
    for i in range(0,len(directory),32):
        offset,size,name=struct.unpack_from('<II24s',directory,i)
        entries[name.split(b'\0')[0].decode('ascii','replace')]=(offset*2048,size*2048)
    models={};instances=[];dedup=set();skipped=[];bounds_warnings=[]
    with (game/'Stream/World.img').open('rb') as archive:
        def read(name):
            offset,size=entries[name];archive.seek(offset);return archive.read(size)
        for name in entries:
            if not name.lower().endswith('.col'):continue
            for m in BullyColFile.from_bytes(read(name)).models:
                if m.object_id in models:raise ValueError(('duplicate COL model',m.object_id,name))
                models[m.object_id]=m
        for name in entries:
            if not name.lower().endswith('.ipb') or name.lower().startswith(('ftest','ttest')):continue
            raw=read(name)
            if raw[23:27]!=b'inst':raise ValueError(('unexpected placement header',name))
            count=struct.unpack_from('<I',raw,27)[0]
            if 31+count*120>len(raw):raise ValueError(('truncated INST',name))
            for i in range(count):
                r=raw[31+i*120:31+(i+1)*120];model=struct.unpack_from('<I',r)[0]
                if model not in models:continue
                # IPB: model, 64-byte name, FLOAT area ID, XYZ, scale, XYZW,
                # FLOAT LOD distance, instance hash. Area is not part of the name.
                native_area=struct.unpack_from('<f',r,68)[0]
                if not np.isfinite(native_area) or native_area != int(native_area) or not -1 <= native_area < 128:
                    raise ValueError(('invalid placement area',name,i,native_area))
                area=int(native_area)
                # -1 denotes the area's LOD placement, not a second solid mesh.
                if area == -1:continue
                xyz=np.array(struct.unpack_from('<3f',r,72));scale=np.array(struct.unpack_from('<3f',r,84))
                q=np.array(struct.unpack_from('<4f',r,96));flags=struct.unpack_from('<I',r,112)[0]
                if not np.all(np.isfinite(np.r_[xyz,scale,q])) or np.linalg.norm(q)<0.9:raise ValueError(('invalid transform',name,i))
                key=(area,model,r[72:112])
                if key in dedup:continue
                dedup.add(key);instances.append((name,area,model,xyz,scale,q,flags))
    
    box_indices=[(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),(1,2,6),(1,6,5),(2,3,7),(2,7,6),(3,0,4),(3,4,7)]
    def geometry(m):
        # Header after bounds: spheres, lines, boxes, compressed mesh. Spheres
        # interleave XYZ/radius/material (20 bytes); boxes are 36-byte records.
        raw=m.raw;cursor=84;points=[];materials=[]
        def count():
            nonlocal cursor
            n=struct.unpack_from('<I',raw,cursor)[0];cursor+=4
            if n>100000:raise ValueError(('bad count',m.object_id,cursor,n))
            return n
        for _ in range(count()):
            center=np.array(struct.unpack_from('<3f',raw,cursor));radius=struct.unpack_from('<f',raw,cursor+12)[0];material=raw[cursor+16];cursor+=20
            if not 0<radius<500:raise ValueError(('bad sphere',m.object_id,radius))
            vertices=[]
            for j in range(9):
                a=np.pi*j/8
                for k in range(16):
                    b=np.pi*2*k/16;vertices.append(center+radius*np.array([np.sin(a)*np.cos(b),np.sin(a)*np.sin(b),np.cos(a)]))
            vertices=np.array(vertices)
            for j in range(8):
                for k in range(16):
                    a=j*16+k;b=j*16+(k+1)%16;c=(j+1)*16+(k+1)%16;d=(j+1)*16+k
                    # Latitude/longitude traversal is inward in this ordering.
                    # Reverse it so one-sided Skate contacts see the solid outside.
                    for face in [(a,c,b),(a,d,c)]:points.append(vertices[list(face)]);materials.append(material)
        lines=count()
        if lines:raise ValueError(('unsupported collision lines',m.object_id,lines))
        for _ in range(count()):
            a=struct.unpack_from('<3f',raw,cursor);b=struct.unpack_from('<3f',raw,cursor+16);material=raw[cursor+32];cursor+=36
            if any(x>y for x,y in zip(a,b)):raise ValueError(('inverted box',m.object_id))
            vertices=np.array([[a[0],a[1],a[2]],[b[0],a[1],a[2]],[b[0],b[1],a[2]],[a[0],b[1],a[2]],
                               [a[0],a[1],b[2]],[b[0],a[1],b[2]],[b[0],b[1],b[2]],[a[0],b[1],b[2]]])
            for face in box_indices:points.append(vertices[list(face)]);materials.append(material)
        n=count();vertices=np.frombuffer(raw,dtype='<i2',count=n*3,offset=cursor).reshape(n,3).astype(float)/128;cursor+=n*6;cursor=(cursor+3)&~3
        for _ in range(count()):
            a,b,c,material,light=struct.unpack_from('<HHHBB',raw,cursor);cursor+=8
            if max(a,b,c)>=n:raise ValueError(('invalid triangle indices',m.object_id))
            # COL mesh faces use clockwise winding. The recovered Skate solver
            # uses outward cross-product normals for one-sided contact volumes.
            points.append(vertices[[a,c,b]]);materials.append(material)
        if points:
            points=np.array(points)
            if not np.all(np.isfinite(points)) or np.abs(points).max()>10000:raise ValueError(('invalid model geometry',m.object_id))
            if np.any(points.min((0,1))<np.array(m.bounds.min)-0.04) or np.any(points.max((0,1))>np.array(m.bounds.max)+0.04):
                bounds_warnings.append({'model':m.object_id,'authored':[m.bounds.min,m.bounds.max],'geometry':[points.min((0,1)).tolist(),points.max((0,1)).tolist()]})
        return points,materials
    prepared={}
    for id,m in models.items():
        if not physical_collision(m):continue
        try:prepared[id]=geometry(m)
        except (ValueError,struct.error) as e:skipped.append((id,m.major_type,m.minor_type,str(e)))
    target=output_root/'world.bmgeo'
    target.parent.mkdir(parents=True,exist_ok=True);total=0;counts=collections.Counter();bounds=[]
    with target.open('wb') as output:
        output.write(b'BMGEO2\0\0'+struct.pack('<I',0))
        for name,area,model,xyz,scale,q,flags in instances:
            m=models[model];q=q/np.linalg.norm(q);x,y,z,w=q
            rotation=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                               [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                               [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
            if model not in prepared:continue
            points,materials=prepared[model]
            if not len(points):continue
            points=np.array(points);world=(points*scale)@rotation.T+xyz
            if np.prod(scale)<0:world=world[:,[0,2,1]]
            # The core's coordinate system is a rigid right-handed Y-up transform.
            core=world[:,:,[0,2,1]].copy();core[:,:,2]*=-1
            for tri,material in zip(core,materials):
                if np.linalg.norm(np.cross(tri[1]-tri[0],tri[2]-tri[0]))<1e-7:continue
                output.write(struct.pack('<9fHHI',*tri.ravel(),material,area,model));total+=1;counts[name]+=1
            bounds.append((world.min((0,1)).tolist(),world.max((0,1)).tolist()))
        output.seek(8);output.write(struct.pack('<I',total))
    report={'format':'BMGEO2','models':len(models),'instances':len(instances),'triangles':total,'bytes':target.stat().st_size,'skipped_models':skipped,'bounds_warnings':bounds_warnings,'placements':dict(counts)}
    export_vehicle_bounds(models,output_root/'vehicle-bounds.txt')
    return report
