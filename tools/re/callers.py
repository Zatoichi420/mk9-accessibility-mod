import pefile,struct,sys
pe=pefile.PE(sys.argv[1]); ib=pe.OPTIONAL_HEADER.ImageBase; data=pe.get_memory_mapped_image()
t=[s for s in pe.sections if s.Name.startswith(b'.text')][0]; tv=t.VirtualAddress; n=t.Misc_VirtualSize
import numpy as np
tb=np.frombuffer(data[tv:tv+n],dtype=np.uint8)
rel=np.frombuffer(data[tv+1:tv+n]+b'\0\0\0',dtype=np.uint8)
for f in sys.argv[2:]:
    tgt=int(f,16)
    out=[]
    for op in (0xe8,0xe9):
        idx=np.nonzero(tb==op)[0]
        idx=idx[idx+5<=n]
        r=(tb[idx+1].astype(np.int64)|(tb[idx+2].astype(np.int64)<<8)|(tb[idx+3].astype(np.int64)<<16)|(tb[idx+4].astype(np.int64)<<24))
        r=np.where(r>=2**31,r-2**32,r)
        dest=ib+tv+idx+5+r
        for i in idx[dest==tgt]: out.append((hex(op),hex(ib+tv+i)))
    print(f,len(out),out[:12])
