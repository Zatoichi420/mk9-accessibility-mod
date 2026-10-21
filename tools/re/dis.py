import pefile,sys,capstone
pe=pefile.PE(sys.argv[1]); ib=pe.OPTIONAL_HEADER.ImageBase; data=pe.get_memory_mapped_image()
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
a=int(sys.argv[2],16); n=int(sys.argv[3])
for i,ins in enumerate(md.disasm(data[a-ib:a-ib+n*8],a)):
    if i>=n: break
    print(f'{ins.address:#x}: {ins.mnemonic} {ins.op_str}')
