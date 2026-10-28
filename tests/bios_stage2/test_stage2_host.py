# SPDX-License-Identifier: GPL-3.0-only
import ctypes as C
from pathlib import Path
import random
import struct
import subprocess
import sys
import zlib
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).resolve().parent))
from stage2_fixture import create_stage2_image, hashes

ALLOC=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint32,C.POINTER(C.c_uint64),C.POINTER(C.c_void_p))
class Loaded(C.Structure):_fields_=[("physical",C.c_uint64),("virtual",C.c_uint64),("length",C.c_uint64),("permissions",C.c_uint32),("reserved",C.c_uint32)]

READ=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint64,C.c_void_p,C.c_uint32)
class Reader(C.Structure):_fields_=[('context',C.c_void_p),('read',READ),('size',C.c_uint64)]
class Disk(C.Structure):
    _fields_=[('reader',Reader),('bios_start',C.c_uint32),('bios_end',C.c_uint32),('esp_start',C.c_uint32),('esp_sectors',C.c_uint32),('disk',C.c_ubyte*16),('esp',C.c_ubyte*16),('system',C.c_ubyte*16)]
class Fat(C.Structure):
    _fields_=[('disk',C.POINTER(Disk))]+[(n,C.c_uint32) for n in ('fat_start','fat_sectors','data_start','clusters','root','sectors_per_cluster','fat_count','active_fat','mirrored','file_cluster','file_size')]
class Segment(C.Structure):_fields_=[(n,C.c_uint64) for n in ('offset','virtual','filesz','memsz')]+[('permissions',C.c_uint32),('reserved',C.c_uint32)]
class Plan(C.Structure):_fields_=[('entry',C.c_uint64),('required',C.c_uint64),('count',C.c_uint32),('reserved',C.c_uint32),('segments',Segment*32)]


def golden_elf():
    b=bytearray(0x3010);b[:16]=b'\x7fELF\x02\x01\x01'+bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH',b,16,2,62,1,0xffffffff80000000,64,0,0,64,56,4,0,0,0)
    for i,(flags,off,va,sz,mem) in enumerate([(5,0x1000,0xffffffff80000000,8,4096),(4,0x2000,0xffffffff80001000,56,4096),(6,0x3000,0xffffffff80002000,16,8192)]):
        struct.pack_into('<IIQQQQQQ',b,64+i*56,1,flags,off,va,0,sz,mem,4096)
    struct.pack_into('<IIQQQQQQ',b,64+3*56,4,4,0x2000,0xffffffff80001000,0,52,52,4)
    b[0x1000:0x1008]=b'TEXTDATA'
    struct.pack_into('<III8s8sHHIQQ',b,0x2000,6,32,1,b'LAMIX\0\0\0',b'LAMIXREQ',1,0,0,0,4)
    b[0x3000:]=b'INITIALIZED DATA'
    return b

@pytest.fixture(scope='session')
def loader_lib(tmp_path_factory):
    out=tmp_path_factory.mktemp('m05-host')/'loader.so'
    subprocess.run(['gcc','-std=c11','-O2','-fPIC','-shared','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),
                    str(ROOT/'lib/boot/loader.c'),str(ROOT/'lib/boot/contract.c'),str(ROOT/'boot/bios/stage2/div64.c'),'-o',str(out)],check=True)
    lib=C.CDLL(str(out))
    lib.lamix_crc32.argtypes=[C.c_void_p,C.c_uint32];lib.lamix_crc32.restype=C.c_uint32
    lib.lamix_divmod64.argtypes=[C.c_uint64,C.c_uint64,C.POINTER(C.c_uint64)];lib.lamix_divmod64.restype=C.c_uint64
    lib.lamix_gpt_open.argtypes=[Reader,C.c_uint32,C.c_uint32,C.c_void_p,C.c_uint32,C.POINTER(Disk)]
    lib.lamix_fat_open.argtypes=[C.POINTER(Disk),C.POINTER(Fat)]
    lib.lamix_fat_read.argtypes=[C.c_void_p,C.c_uint64,C.c_void_p,C.c_uint32]
    lib.lamix_elf_inspect.argtypes=[Reader,C.c_uint64,C.POINTER(Plan)]
    lib.lamix_elf_load.argtypes=[Reader,C.POINTER(Plan),ALLOC,C.c_void_p,C.POINTER(Loaded),C.c_uint32]
    return lib


def buffer_reader(data):
    @READ
    def read(ctx,offset,output,size):
        if offset>len(data) or size>len(data)-offset:return 0
        C.memmove(output,bytes(data[offset:offset+size]),size);return 1
    return Reader(None,read,len(data)),read


def inspect(lib,data,features=4):
    reader,keep=buffer_reader(data);p=Plan();C.memset(C.byref(p),0xa5,C.sizeof(p));before=bytes(p)
    status=lib.lamix_elf_inspect(reader,features,C.byref(p))
    if status:assert bytes(p)==before
    return status,p


def test_division_oracle(loader_lib):
    rng=random.Random(0x4c414d4958)
    pairs=[(n,d) for n in (0,1,2**32-1,2**32,2**63,2**64-1) for d in (1,2,3,2**32-1,2**63,2**64-1)]
    pairs += [(rng.getrandbits(64),rng.getrandbits(64) or 1) for _ in range(10000)]
    for n,d in pairs:
        remainder=C.c_uint64();q=loader_lib.lamix_divmod64(n,d,C.byref(remainder))
        assert (q,remainder.value)==divmod(n,d)
    assert loader_lib.lamix_crc32(b'123456789',9)==0xcbf43926


def test_golden_elf_and_truncations(loader_lib):
    b=golden_elf();status,p=inspect(loader_lib,b)
    assert status==0 and p.count==3 and p.segments[2].memsz==8192 and p.segments[0].permissions==5
    for size in (0,1,63,64,100,287,4096,8192,len(b)-1):assert inspect(loader_lib,b[:size])[0]!=0

@pytest.mark.parametrize('offset,fmt,value',[(0,'B',0),(4,'B',1),(5,'B',2),(16,'H',3),(18,'H',3),(24,'Q',0x800000000000),(32,'Q',2**64-1),(54,'H',64),(56,'H',129),
    (64+4,'I',7),(64+8,'Q',2**64-1),(64+16,'Q',0xffffffff80000001),(64+24,'Q',0x100000),(64+32,'Q',8193),(64+48,'Q',3),
    (64+56+16,'Q',0xffffffff80000000),(64+112+40,'Q',65*1024**2),(64,'I',2),(0x2000,'I',2**32-1),(0x2000+24,'H',2),(0x2000+36,'I',1)])
def test_elf_negative_transaction(loader_lib,offset,fmt,value):
    b=golden_elf();struct.pack_into('<'+fmt,b,offset,value);assert inspect(loader_lib,b)[0]!=0


def test_note_negotiation(loader_lib):
    b=golden_elf();struct.pack_into('<QQ',b,0x2024,4,0)
    assert inspect(loader_lib,b,0)[0]==4 and inspect(loader_lib,b,4)[0]==0
    struct.pack_into('<QQ',b,0x2024,2,0);assert inspect(loader_lib,b,4)[0]==4

@pytest.fixture
def synthetic_build(tmp_path):
    b=tmp_path/'host-artifacts';b.mkdir();(b/'stage1.bin').write_bytes(bytes(440));(b/'stage2.bin').write_bytes(bytes(32768+1024));(b/'kernel.elf').write_bytes(golden_elf());return b

@pytest.mark.parametrize('variant,stage',[('valid','OK'),('fragmented','OK'),('fragmented-dir','OK'),('active-fat','OK'),('gpt-backup-crc','GPT'),('gpt-array-crc','GPT'),('fat-bpb','FAT'),('fat-loop','FAT'),('fat-mirror','FAT'),('missing-a','FAT'),('dir-loop','FAT'),('fat-free','FAT'),('fat-short','FAT'),('gpt-overlap','GPT'),('gpt-duplicate','GPT'),('gpt-guid','GPT')])
def test_host_image_pipeline(loader_lib,tmp_path,synthetic_build,variant,stage):
    path=tmp_path/'fixture.img';m=create_stage2_image(path,synthetic_build,variant);before=hashes(path,m)
    with path.open('rb') as file:
        @READ
        def read(ctx,offset,output,size):
            file.seek(offset);data=file.read(size)
            if len(data)!=size:return 0
            C.memmove(output,data,size);return 1
        reader=Reader(None,read,path.stat().st_size);d=Disk();scratch=C.create_string_buffer(16384)
        C.memset(C.byref(d),0xa5,C.sizeof(d));before_disk=bytes(d)
        status=loader_lib.lamix_gpt_open(reader,2048,6143,scratch,len(scratch),C.byref(d))
        if status:assert bytes(d)==before_disk
        if stage=='GPT':assert status!=0
        else:
            assert status==0 and d.esp_start==6144
            f=Fat();C.memset(C.byref(f),0xa5,C.sizeof(f));before_fat=bytes(f)
            status=loader_lib.lamix_fat_open(C.byref(d),C.byref(f))
            if status:assert bytes(f)==before_fat
            if stage=='FAT':assert status!=0
            else:
                assert status==0
                @READ
                def read_file(ctx,offset,dest,size):return loader_lib.lamix_fat_read(C.byref(f),offset,dest,size)
                p=Plan();assert loader_lib.lamix_elf_inspect(Reader(None,read_file,f.file_size),4,C.byref(p))==0
    assert hashes(path,m)==before
    with pytest.raises(FileExistsError):create_stage2_image(path,synthetic_build,variant)
    with pytest.raises(ValueError):create_stage2_image('/dev/null',synthetic_build)


@pytest.mark.parametrize('failure',[None,'oom','overlap','read','capacity'])
def test_shared_elf_load_transaction(loader_lib,failure):
    image=golden_elf();status,plan=inspect(loader_lib,image);assert status==0
    reader,keep=buffer_reader(image if failure!='read' else image[:4096])
    buffers=[]
    @ALLOC
    def allocate(ctx,size,physical,writable):
        if failure=='oom' and buffers:return 0
        b=C.create_string_buffer(size);C.memset(b,0xa5,size);buffers.append(b)
        physical[0]=0x100000 if failure=='overlap' else 0x100000+len(buffers)*0x100000
        writable[0]=C.addressof(b);return 1
    loaded=(Loaded*32)();C.memset(loaded,0xa5,C.sizeof(loaded));before=bytes(loaded)
    status=loader_lib.lamix_elf_load(reader,C.byref(plan),allocate,None,loaded,1 if failure=='capacity' else 32)
    if failure:
        assert status!=0 and bytes(loaded)==before
    else:
        assert status==0 and len(buffers)==3
        for i,b in enumerate(buffers):
            s=plan.segments[i]
            assert b.raw[:s.filesz]==image[s.offset:s.offset+s.filesz]
            assert not any(b.raw[s.filesz:loaded[i].length])
