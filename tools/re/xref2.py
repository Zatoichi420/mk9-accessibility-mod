import pefile,struct,sys,capstone
pe=pefile.PE(sys.argv[1]); ib=pe.OPTIONAL_HEADER.ImageBase
data=pe.get_memory_mapped_image()
text=[s for s in pe.sections if s.Name.startswith(b'.text')][0]
tv=text.VirtualAddress; tb=data[tv:tv+text.Misc_VirtualSize]
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
def fstart(h):
    o=h
    while o>0 and h-o<0x4000:
        if tb[o-1]==0xcc and tb[o-2]==0xcc and tb[o]!=0xcc: return o
        o-=1
for s in sys.argv[2:]:
    i=data.find(s.encode())
    while i>=0:
        st=i
        while data[st-1]!=0 and i-st<200: st-=1
        va=ib+st; key=struct.pack('<I',va); hits=[]
        j=tb.find(key)
        while j>=0: hits.append(j); j=tb.find(key,j+1)
        print(f'{data[st:st+70]!r} va={va:#x} refs={len(hits)}')
        for h in hits[:4]:
            f=fstart(h); print(f'    ref {ib+tv+h:#x} func {ib+tv+f if f else 0:#x}')
        i=data.find(s.encode(),i+1)
