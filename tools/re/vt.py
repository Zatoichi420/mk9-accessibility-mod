import pefile,struct,sys,capstone
pe=pefile.PE(sys.argv[1]); ib=pe.OPTIONAL_HEADER.ImageBase
data=pe.get_memory_mapped_image()
for f in sys.argv[2:]:
    key=struct.pack('<I',int(f,16)); j=data.find(key); out=[]
    while j>=0: out.append(j); j=data.find(key,j+1)
    for j in out:
        sec=[s.Name.strip(b'\0').decode() for s in pe.sections if s.VirtualAddress<=j<s.VirtualAddress+s.Misc_VirtualSize]
        # find vtable start: walk back while entries look like .text pointers
        k=j
        while True:
            v=struct.unpack_from('<I',data,k-4)[0]
            if not(ib+0x1000<=v<ib+0x855000): break
            k-=4
        print(f, 'ptr at', hex(ib+j), sec, 'slot', (j-k)//4, 'vtbl', hex(ib+k))
