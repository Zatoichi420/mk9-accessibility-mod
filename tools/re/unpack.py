import sys,os,struct,glob,lzallright
TAG=b'\xc1\x83\x2a\x9e'
lz=lzallright.LZOCompressor()
def unpack(d):
    # find chunk table: first chunk's CompOffset == position of 2nd package tag
    second=d.find(TAG,4)
    if second<0: return d
    key=struct.pack('<I',second)
    p=d.find(key,12,4096)
    while p>=0:
        n=struct.unpack_from('<I',d,p-12)[0]; flags=struct.unpack_from('<I',d,p-16)[0]
        if 0<n<10000 and flags in (1,2,4): break
        p=d.find(key,p+1,4096)
    else: return None
    base=p-8
    chunks=[struct.unpack_from('<4I',d,base+16*i) for i in range(n)]
    total=max(u+us for u,us,_,_ in chunks)
    out=bytearray(total); out[:chunks[0][0]]=d[:chunks[0][0]]
    for u,us,c,cs in chunks:
        tag,bs,ctot,utot=struct.unpack_from('<4I',d,c); q=c+16
        nb=(utot+bs-1)//bs; blocks=[struct.unpack_from('<2I',d,q+8*i) for i in range(nb)]; q+=8*nb
        pos=u
        for bc,bu in blocks:
            raw=d[q:q+bc]; q+=bc
            dec=lzallright.LZOCompressor.decompress(raw,output_size_hint=bu) if flags==2 else __import__('zlib').decompress(raw)
            out[pos:pos+len(dec)]=dec; pos+=len(dec)
    struct.pack_into('<I',out,base-16,0)  # mark uncompressed
    return bytes(out)
src,dst=sys.argv[1],sys.argv[2]; os.makedirs(dst,exist_ok=True)
for f in sorted(glob.glob(os.path.join(src,sys.argv[3] if len(sys.argv)>3 else 'ui_*.xxx'))):
    d=open(f,'rb').read(); r=unpack(d)
    if r is None: print('FAIL',os.path.basename(f)); continue
    open(os.path.join(dst,os.path.basename(f)),'wb').write(r)
print('done')
