"""Generate original PCX demonstration art and data-only ZIP/REZ packages.

Python standard library only. No retail assets or third-party artwork are read.
"""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path

def pcx(width,height,variant):
    header=bytearray(128);header[:4]=bytes([10,5,1,8]);struct.pack_into('<4H',header,4,0,0,width-1,height-1)
    struct.pack_into('<2H',header,12,72,72);header[65]=1;struct.pack_into('<2H',header,66,width,1)
    data=bytearray()
    letters={'N':['10001','11001','10101','10011','10001'],'O':['01110','10001','10001','10001','01110'],
             'L':['10000','10000','10000','10000','11111'],'F':['11111','10000','11110','10000','10000'],
             'M':['10001','11011','10101','10001','10001'],'D':['11110','10001','10001','10001','11110'],
             'S':['01111','10000','01110','00001','11110'],' ':['00000']*5}
    words='NOLF MODS';scale=3;left=(width-len(words)*6*scale)//2;top=(height-5*scale)//2
    for y in range(height):
        row=[]
        for x in range(width):
            color=1 if (x//12+y//12)%2 else 2
            if x<4 or y<4 or x>=width-4 or y>=height-4:color=3
            tx=(x-left)//scale;ty=(y-top)//scale
            if tx>=0 and 0<=ty<5 and tx//6<len(words) and tx%6<5 and letters[words[tx//6]][ty][tx%6]=='1':color=3
            row.append(color)
        for pixel in row:data.extend([pixel])
    palette=bytearray(768)
    colors=[(0,0,0),(18,52,54),(28,82,69),(246,195,64)] if variant==0 else [(0,0,0),(80,24,67),(125,36,75),(255,224,120)]
    for i,rgb in enumerate(colors):palette[i*3:i*3+3]=bytes(rgb)
    return bytes(header+data+b'\x0c'+palette)

def rez(files):
    data=bytearray(b'RezMgr'+b'\0'*(127-6)+b'\0'*12);entries=bytearray()
    for path,raw in sorted(files.items()):
        name=Path(path).stem;ext=Path(path).suffix[1:]
        pos=len(data);data+=raw
        entries+=struct.pack('<5I',0,pos,len(raw),0,1)+ext.encode()[::-1].ljust(4,b'\0')+struct.pack('<I',0)+name.encode()+b'\0\0'
    directory=len(data);data+=entries
    root=len(data);parent=struct.pack('<4I',1,directory,len(entries),0)+b'INTERFACE\0';data+=parent
    struct.pack_into('<3I',data,127,1,root,len(parent));return bytes(data)

def generate(output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True);result=[]
    for variant,identifier in enumerate(('example-loose','example-rez')):
        art=pcx(192+variant*32,64+variant*16,variant)
        files={'INTERFACE/MODMANAGER.PCX':art,'INTERFACE/PAUSE.PCX':art}
        meta=dict(schema=2,id=identifier,name='Original demo - '+('loose ZIP' if variant==0 else 'REZ override'),author='NOLF Mod Manager example',
            version='1.0.0',category='Interface',description='Original geometric pause art. Demonstrates data-only importing and deterministic overrides.',
            manager_version='>=2.0,<3',files={n:hashlib.sha256(b).hexdigest() for n,b in files.items()},preview={'image':'INTERFACE/MODMANAGER.PCX','caption':'Original generated NOLF MODS pattern'})
        if variant:meta.update(requires=[{'id':'example-loose','version':'==1.0.0'}],overrides=[{'id':'example-loose','paths':list(files)}])
        path=output/(identifier+'.zip')
        with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('nolf-mod.json',json.dumps(meta,indent=2))
            if variant:archive.writestr('original-art.rez',rez(files))
            else:
                for n,b in files.items():archive.writestr('payload/'+n,b)
        result.append(dict(file=path.name,width=192+variant*32,height=64+variant*16,sha256=hashlib.sha256(art).hexdigest()))
    (output/'README.txt').write_text('Original synthetic demonstration art; no retail assets. These examples are dedicated to the public domain under CC0-1.0.\nImport the loose package first; it stays disabled. Enable it to replace the pause graphic. The REZ example requires it and declares two explicit replacements, which must be reviewed and approved. Restart to apply.\nThis unofficial add-on is not made or supported by Monolith Productions.\n')
    (output/'expected.json').write_text(json.dumps(result,indent=2));return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);print(json.dumps(generate(p.parse_args().output),indent=2))
