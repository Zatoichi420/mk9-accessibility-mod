import sys,os,re,struct,glob
src,out=sys.argv[1],sys.argv[2]
os.makedirs(out,exist_ok=True)
n=0;bad=[]
for p in sorted(glob.glob(os.path.join(src,'ui_*.xxx'))):
    d=open(p,'rb').read()
    for m in re.finditer(rb'GFX[\x07-\x0b]',d):
        o=m.start(); ln=struct.unpack_from('<I',d,o-4)[0]
        if ln<100 or o+ln>len(d): bad.append(os.path.basename(p)); continue
        body=bytearray(d[o:o+ln]); body[0:3]=b'FWS'; struct.pack_into('<I',body,4,ln)
        open(os.path.join(out,os.path.basename(p)[:-4]+(f'_{o:x}' if o!=re.search(rb'GFX[\x07-\x0b]',d).start() else '')+'.swf'),'wb').write(body); n+=1
print('movies',n,'bad',bad)
