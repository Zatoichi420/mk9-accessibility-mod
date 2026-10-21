import pefile,struct,sys,numpy as np,capstone
pe=pefile.PE(sys.argv[1]); ib=pe.OPTIONAL_HEADER.ImageBase; data=pe.get_memory_mapped_image()
t=[s for s in pe.sections if s.Name.startswith(b'.text')][0]; tv=t.VirtualAddress; n=t.Misc_VirtualSize
tb=np.frombuffer(data[tv:tv+n],dtype=np.uint8)
idx=np.nonzero(tb==0xe8)[0]; idx=idx[idx+5<=n]
r=(tb[idx+1].astype(np.int64)|(tb[idx+2].astype(np.int64)<<8)|(tb[idx+3].astype(np.int64)<<16)|(tb[idx+4].astype(np.int64)<<24))
r=np.where(r>=2**31,r-2**32,r); dest=ib+tv+idx+5+r
lo,hi=int(sys.argv[2],16),int(sys.argv[3],16)
# function starts = after CC padding
starts=[]
for a in range(lo,hi):
    o=a-ib
    if data[o-1]==0xcc and data[o]!=0xcc: starts.append(a)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
strs=set()
for s in starts:
    cnt=int((dest==s).sum())
    # find string refs inside function (push imm in .rdata pointing to ascii)
    refs=[]
    for ins in md.disasm(data[s-ib:s-ib+0x400],s):
        if ins.mnemonic=='int3': break
        if ins.mnemonic=='push' and ins.op_str.startswith('0x'):
            v=int(ins.op_str,16)
            if 0xc55000<=v<0xde0000:
                b=data[v-ib:v-ib+60].split(b'\0')[0]
                if len(b)>2 and all(32<=c<127 for c in b): refs.append(b.decode())
    print(f'{s:#x} callers={cnt} {refs[:4]}')
