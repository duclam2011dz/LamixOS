# SPDX-License-Identifier: GPL-3.0-only
"""Fixture M05 test-only, chỉ tạo file mới; không mount/device/formatter sản phẩm."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import zlib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/boot'))
from gpt_fixture import fixtures
from gpt_model import validate


def create_stage2_image(path,build,variant='valid',kernel='kernel.elf'):
    path=Path(path);parent=path.parent.resolve(strict=True)
    if not any(parent.is_relative_to(p) for p in (ROOT/'build',ROOT/'out',Path('/tmp'))):raise ValueError('fixture path ngoài build/out/tmp')
    path=parent/path.name;build=Path(build)
    if path.exists() or path.is_symlink():raise FileExistsError(path)
    p,b,entries,back,pmbr,size,block=fixtures()
    validate(p,b,entries,back,pmbr,size,block)
    stage1=(build/'stage1.bin').read_bytes()
    if len(stage1)!=440:raise ValueError('Stage1 size')
    pmbr[:440]=stage1
    stage=bytearray((build/'stage2.bin').read_bytes());elf=bytearray((build/kernel).read_bytes())
    if not 32768<=len(stage)<=2*1024*1024 or not 0<len(elf)<=16*1024*1024:raise ValueError('payload size')
    header_changes={'s2-version':(8,'H',2),'s2-core-lba':(12,'I',65),'s2-empty':(16,'I',0),
                    's2-memory':(20,'I',0x40001),'s2-load':(24,'I',0x21000),'s2-entry':(28,'I',0x40000),'s2-reserved':(40,'B',1)}
    if variant in header_changes:
        off,fmt,value=header_changes[variant];struct.pack_into('<'+fmt,stage,32704+off,value)
        struct.pack_into('<I',stage,32704+36,0);struct.pack_into('<I',stage,32704+36,zlib.crc32(stage[32704:32768]))
    if variant=='header-crc':stage[32704+36]^=1
    if variant=='core-crc':stage[32768+4]^=1
    if variant in ('large-core','large-core-crc'):
        core=bytes(stage[32768:]).ljust(70000,b'\0');stage=stage[:32768]+core
        struct.pack_into('<I',stage,32704+16,len(core));struct.pack_into('<I',stage,32704+32,zlib.crc32(core))
        struct.pack_into('<I',stage,32704+36,0);struct.pack_into('<I',stage,32704+36,zlib.crc32(stage[32704:32768]))
        if variant.endswith('-crc'):stage[-1]^=1

    if variant=='elf-magic':elf[0]=0
    if variant in ('note-required-nx','note-required-acpi'):
        phoff=struct.unpack_from('<Q',elf,32)[0];stride,count=struct.unpack_from('<HH',elf,54)
        note=next(phoff+i*stride for i in range(count) if struct.unpack_from('<I',elf,phoff+i*stride)[0]==4)
        off=struct.unpack_from('<Q',elf,note+8)[0]
        at=off+12+((struct.unpack_from('<I',elf,off)[0]+3)&~3)
        struct.pack_into('<QQ',elf,at+16,4 if variant.endswith('nx') else 2,0)
    if variant=='note-duplicate':
        # Đổi một program header không dùng để tạo PT_NOTE trùng.
        off=struct.unpack_from('<Q',elf,32)[0];stride,count=struct.unpack_from('<HH',elf,54)
        note=next(elf[off+i*stride:off+(i+1)*stride] for i in range(count) if struct.unpack_from('<I',elf,off+i*stride)[0]==4)
        for i in range(count):
            if struct.unpack_from('<I',elf,off+i*stride)[0]==0x6474e551:elf[off+i*stride:off+(i+1)*stride]=note;break
    if variant=='oom':
        off=struct.unpack_from('<Q',elf,32)[0];stride,count=struct.unpack_from('<HH',elf,54)
        loads=[off+i*stride for i in range(count) if struct.unpack_from('<I',elf,off+i*stride)[0]==1]
        struct.pack_into('<Q',elf,loads[-1]+40,63*1024*1024)
    if variant.startswith('inject-'):
        import subprocess
        flag={'inject-e820-bad':'LAMIX_TEST_E820_BAD','inject-e820-truncated':'LAMIX_TEST_E820_TRUNCATED',
              'inject-e820-overflow':'LAMIX_TEST_E820_OVERFLOW','inject-a20':'LAMIX_TEST_A20_FAIL','inject-segments':'LAMIX_TEST_SEGMENT_LIMITS'}[variant]
        with tempfile.TemporaryDirectory(prefix='m05-adapter-',dir=parent) as temporary:
            artifact=Path(temporary)/'bootstrap.bin'
            subprocess.run(['nasm','--reproducible','-f','bin','-D'+flag+'=1','-o',str(artifact),str(ROOT/'boot/bios/stage2/bootstrap.asm')],check=True)
            prefix=bytearray(artifact.read_bytes())
        prefix[-64:]=stage[32704:32768];stage=prefix+stage[32768:]
    if variant in ('gpt-overlap','gpt-duplicate','gpt-guid'):
        if variant=='gpt-overlap':struct.pack_into('<Q',entries,128+32,2048)
        if variant=='gpt-duplicate':entries[3*128:3*128+16]=entries[:16]
        if variant=='gpt-guid':entries[128+16:128+32]=entries[16:32]
        back[:]=entries
        for header in (p,b):
            struct.pack_into('<I',header,88,zlib.crc32(entries));struct.pack_into('<I',header,16,0);struct.pack_into('<I',header,16,zlib.crc32(header[:92]))
    if variant in ('elf-paddr','elf-wx','elf-entry','elf-overlap','elf-wrap','elf-dynamic'):
        phoff=struct.unpack_from('<Q',elf,32)[0];stride,count=struct.unpack_from('<HH',elf,54)
        loads=[phoff+i*stride for i in range(count) if struct.unpack_from('<I',elf,phoff+i*stride)[0]==1]
        if variant=='elf-dynamic':struct.pack_into('<I',elf,loads[0],2)
        if variant=='elf-paddr':struct.pack_into('<Q',elf,loads[0]+24,0x100000)
        if variant=='elf-wx':struct.pack_into('<I',elf,loads[0]+4,7)
        if variant=='elf-entry':struct.pack_into('<Q',elf,24,0x800000000000)
        if variant=='elf-overlap':struct.pack_into('<Q',elf,loads[1]+16,struct.unpack_from('<Q',elf,loads[0]+16)[0])
        if variant=='elf-wrap':struct.pack_into('<Q',elf,loads[-1]+40,2**64-1)
    if variant=='gpt-backup-crc':b[16]^=1
    if variant=='gpt-array-crc':back[16]^=1
    manifest=[]
    with path.open('xb') as disk:
        disk.truncate(size)
        def write(offset,data,tag):
            disk.seek(offset);disk.write(data);manifest.append((offset,len(data),tag))
        write(0,pmbr,'pmbr');write(512,p,'primary');write(1024,entries,'entries')
        write(size-33*512,back,'backup-entries');write(size-512,b,'backup-header')
        write(2048*512,stage,'stage2')
        esp=6144;total=524288;fat_size=4096;data_start=32+2*fat_size
        boot=bytearray(512);boot[:3]=b'\xeb\x58\x90';boot[3:11]=b'LAMIXTST'
        struct.pack_into('<HBHBHHBHHHII',boot,11,512,1,32,2,0,0,0xf8,0,63,255,esp,total)
        struct.pack_into('<IHHIHH',boot,36,fat_size,0,0,2,1,6)
        boot[64]=0x80;boot[66]=0x29;struct.pack_into('<I',boot,67,0x4c414d49)
        boot[71:82]=b'LAMIX TEST ';boot[82:90]=b'FAT32   ';boot[510:]=b'\x55\xaa'
        if variant=='fat-bpb':boot[13]=3
        if variant=='active-fat':struct.pack_into('<H',boot,40,0x81)
        write(esp*512,boot,'fat-bpb');write((esp+6)*512,boot,'fat-backup-bpb')
        fsinfo=bytearray(512);struct.pack_into('<I',fsinfo,0,0x41615252);struct.pack_into('<III',fsinfo,484,0x61417272,0xffffffff,0xffffffff);fsinfo[510:]=b'\x55\xaa'
        write((esp+1)*512,fsinfo,'fsinfo')
        clusters=(len(elf)+511)//512
        chain=[10+i*(2 if variant=='fragmented' else 1) for i in range(clusters)]
        fat=bytearray(((max(chain)+128)//128)*512)
        for c,v in [(0,0x0ffffff8),(1,0x0fffffff),(2,0x0fffffff),(3,0x0fffffff),(4,0x0fffffff),(5,0x0fffffff)]:struct.pack_into('<I',fat,c*4,v)
        for i,c in enumerate(chain):struct.pack_into('<I',fat,c*4,chain[i+1] if i+1<len(chain) else 0x0fffffff)
        if variant=='fragmented-dir':struct.pack_into('<I',fat,2*4,6);struct.pack_into('<I',fat,6*4,0x0fffffff)
        if variant=='dir-loop':struct.pack_into('<I',fat,2*4,2)
        if variant=='fat-free':struct.pack_into('<I',fat,chain[0]*4,0)
        if variant=='fat-short':struct.pack_into('<I',fat,chain[0]*4,0x0fffffff)
        if variant=='fat-loop':struct.pack_into('<I',fat,chain[-1]*4,chain[0])
        for i in range(2):
            copy=bytearray(fat)
            if (variant=='fat-mirror' and i==1) or (variant=='active-fat' and i==0):struct.pack_into('<I',copy,chain[0]*4,0x0ffffff7)
            write((esp+32+i*fat_size)*512,copy,'fat'+str(i))
        def dirent(name,cluster,attr,size=0):
            e=bytearray(32);e[:11]=name;e[11]=attr;struct.pack_into('<H',e,20,cluster>>16);struct.pack_into('<HI',e,26,cluster&0xffff,size);return e
        for c,name,target,attr,length in [(2,b'EFI        ',3,16,0),(3,b'LAMIX      ',4,16,0),(4,b'A          ',5,16,0),(5,b'KERNEL  ELF',chain[0],32,len(elf))]:
            directory=bytearray(512)
            if variant=='fragmented-dir' and c==2:
                for at in range(0,512,32):directory[at]=0xe5
                write((esp+data_start+c-2)*512,directory,'directory2-prefix')
                directory=bytearray(512);c=6
            if not(variant=='missing-a' and c==4):directory[:32]=dirent(name,target,attr,length)
            write((esp+data_start+c-2)*512,directory,'directory'+str(c))
        for i,c in enumerate(chain):write((esp+data_start+c-2)*512,elf[i*512:(i+1)*512].ljust(512,b'\0'),'kernel'+str(i))
        for lba in (259*2048,2307*2048,4355*2048):write(lba*512,b'LAMIX PROTECTED'.ljust(512,b'\xa5'),'sentinel')
    phoff=struct.unpack_from('<Q',elf,32)[0];stride,count=struct.unpack_from('<HH',elf,54)
    first_load=next(phoff+i*stride for i in range(count) if struct.unpack_from('<I',elf,phoff+i*stride)[0]==1)
    payload_index=struct.unpack_from('<Q',elf,first_load+8)[0]//512
    if payload_index>=len(chain):raise ValueError('fixture ELF payload ngoài chain')
    return dict(core_file_bytes=struct.unpack_from('<I',stage,32704+16)[0],extents=manifest,core_sector=2048+64,fat_sector=esp+32,kernel_sector=esp+data_start+chain[payload_index]-2,
                allocated_bytes=path.stat().st_blocks*512)


def hashes(path,manifest):
    result={}
    with Path(path).open('rb') as f:
        for offset,size,name in manifest['extents']:
            f.seek(offset);result[name+':'+str(offset)]=hashlib.sha256(f.read(size)).hexdigest()
    return result
