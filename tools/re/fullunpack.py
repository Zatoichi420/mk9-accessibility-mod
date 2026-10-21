import sys,struct,zlib,lzallright
d=open(sys.argv[1],'rb').read()
tag,bs,ct,ut=struct.unpack_from('<4I',d,0)
nb=(ut+bs-1)//bs; q=16; blocks=[struct.unpack_from('<2I',d,q+8*i) for i in range(nb)]; q+=8*nb
out=bytearray()
for c,u in blocks:
    raw=d[q:q+c]; q+=c
    try: out+=lzallright.LZOCompressor.decompress(raw,output_size_hint=u)
    except Exception: out+=zlib.decompress(raw)
open(sys.argv[2],'wb').write(out); print(len(out),ut)
