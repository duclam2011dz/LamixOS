# SPDX-License-Identifier: GPL-3.0-only
import ctypes as C
import itertools
import struct as S
import pytest
from conftest import Memory, Reservation, View, Span
from fixtures import bootinfo, offsets, record, table, segment, reservation

OK, INVALID, TRUNCATED, OVERFLOW, UNSUPPORTED, CONFLICT, SMALL = range(7)


def validate(lib, blob, spans=None):
    data = C.create_string_buffer(bytes(blob)); view = View(None, 123, 99)
    status = lib.lbp_validate(data, len(blob), spans, len(spans) if spans else 0, C.byref(view))
    if status:
        assert view.size == 123 and view.count == 99
    return status, view, data

@pytest.mark.parametrize('source', [1, 2])
def test_golden_bootinfo_and_iteration(lib, source):
    blob = bootinfo(source)
    assert bytes(blob[:16]) == b'LAMIXLBP\x01\0\0\0\x40\0\0\0'
    status, view, keep = validate(lib, blob)
    assert status == OK and view.size == len(blob) and view.count == 5
    for index, expected in enumerate(offsets(blob).values()):
        off = C.c_uint64(999)
        assert lib.lbp_record_at(C.byref(view), index, C.byref(off)) == OK
        assert off.value == expected
    assert lib.lbp_record_at(C.byref(view), 5, C.byref(off)) == INVALID

@pytest.mark.parametrize('off,fmt,value', [
    (0, 'B', 0), (8, 'H', 2), (12, 'I', 56), (16, 'Q', 2**64-1),
    (16, 'Q', 1048584), (24, 'Q', 8), (24, 'Q', 1), (40, 'Q', 65),
    (48, 'I', 257), (52, 'I', 1), (56, 'Q', 0x110001), (56, 'Q', 2**32),
    (64+8, 'Q', 0), (64+8, 'Q', 17), (64+4, 'I', 2),
    (64+16, 'I', 2), (64+20, 'I', 24), (64+24, 'Q', 2**64-1),
])
def test_header_and_record_negative(lib, off, fmt, value):
    blob = bootinfo(); S.pack_into('<'+fmt, blob, off, value)
    assert validate(lib, blob)[0] != OK


def test_truncations_and_view_transaction(lib):
    blob = bootinfo()
    for size in range(len(blob)):
        assert validate(lib, blob[:size])[0] != OK


def test_extensions_and_required_record(lib):
    assert validate(lib, bootinfo(extras=[record(99, b'future', False)]))[0] == OK
    assert validate(lib, bootinfo(extras=[record(99, b'', True)]))[0] == UNSUPPORTED
    blob = bootinfo(); S.pack_into('<H', blob, 10, 7)
    assert validate(lib, blob)[0] == OK
    blob = bootinfo(); S.pack_into('<I', blob, 64, 99)
    assert validate(lib, blob)[0] != OK
    blob = bootinfo(); S.pack_into('<I', blob, offsets(blob)[2], 1)
    assert validate(lib, blob)[0] == CONFLICT

@pytest.mark.parametrize('typ,relative,fmt,value', [
    (1,32+16,'I',99),(1,32,'Q',0x100001),(1,32+8,'Q',2**64-1),
    (2,16,'Q',0xffffffff80001000),(2,40+8,'Q',0x800000000000),
    (2,40,'Q',2**32),(2,40+24,'I',7),(2,40+28,'I',1),
    (3,32+20,'I',1),(3,32+24,'Q',1),(3,32,'Q',0x200000),
    (4,16,'Q',0x130001),(4,40,'Q',0x12fff0),(4,48,'Q',0x120000),
    (4,72+8,'Q',0x11f000),(4,72+24,'I',7),
    (5,16,'I',0),(5,20,'I',4096),(5,72,'I',3),(5,76,'I',1),
])
def test_semantic_negative(lib, typ, relative, fmt, value):
    blob = bootinfo(); S.pack_into('<'+fmt, blob, offsets(blob)[typ]+relative, value)
    assert validate(lib, blob)[0] != OK


def test_mapping_alias_wx_and_overlap(lib):
    blob = bootinfo(); pos=offsets(blob)[4]
    # Identity mapping một executable kernel page dưới quyền write.
    S.pack_into('<QQQII', blob, pos+72+32, 0x100000, 0x110000, 4096, 3, 0)
    assert validate(lib, blob)[0] == CONFLICT


def test_note_versions_features_and_truncation(lib):
    note = S.pack('<8sHHIQQ', b'LAMIXREQ', 1, 0, 0, 1, 2)
    assert lib.lbp_validate_note(note, 32, 1) == OK
    assert lib.lbp_validate_note(note, 32, 0) == UNSUPPORTED
    for size in range(32): assert lib.lbp_validate_note(note, size, 7) == TRUNCATED
    for off, fmt, val in [(0,'B',0),(8,'H',2),(10,'H',1),(12,'I',1),(16,'Q',8),(24,'Q',1)]:
        data=bytearray(note); S.pack_into('<'+fmt,data,off,val)
        assert lib.lbp_validate_note(bytes(data),32,7) != OK


def test_optional_framebuffer(lib):
    fb = record(7,S.pack('<QQIIIIIIII',0xe0000000,640*480*4,640,480,2560,32,0xff0000,0xff00,0xff,0xff000000),False)
    assert validate(lib,bootinfo(extras=[fb],features=1))[0] == OK
    assert validate(lib,bootinfo(extras=[fb]))[0] == INVALID
    for rel, val in [(32,0),(40,4),(44,24),(48,0xff),(24,1)]:
        b=bootinfo(extras=[fb],features=1);S.pack_into('<I',b,offsets(b)[7]+rel,val)
        assert validate(lib,b)[0] != OK


def test_acpi_spans_checksum(lib):
    rsdp=bytearray(b'RSD PTR '+bytes(12));rsdp[8]=(-sum(rsdp))%256
    external=C.create_string_buffer(bytes(rsdp));spans=(Span*1)(Span(0xf0000,C.cast(external,C.c_void_p),20))
    acpi=record(8,S.pack('<QII',0xf0000,20,0),False)
    blob=bootinfo(extras=[acpi],features=2)
    assert validate(lib,blob,spans)[0] == OK
    assert validate(lib,blob)[0] == TRUNCATED
    external[9]=b'X'
    assert validate(lib,blob,spans)[0] == INVALID


def test_modules_and_utf8(lib):
    reservations=[reservation(0x100000,4096,1,0),reservation(0x110000,4096,3,1),
                  reservation(0x120000,65536,4,2),reservation(0x130000,4096,5,2),
                  reservation(0x131000,4096,2,0)]
    base=bootinfo(reservations=reservations)
    def make(name):
        desc=S.pack('<QQQII',0x131000,1024,len(base)+64,len(name),0)
        return bootinfo(reservations=reservations,extras=[record(6,S.pack('<IIQ',1,32,1)+desc+name,False)])
    assert validate(lib,make('môđun'.encode()))[0] == OK
    for name in (b'\0',b'\xff',b'\xc0\x80',b'\xed\xa0\x80'):
        assert validate(lib,make(name))[0] != OK


def normalize(lib, data, reservations=(), capacity=64):
    a=(Memory*len(data))(*(Memory(*x) for x in data));r=(Reservation*len(reservations))(*(Reservation(*x) for x in reservations))
    tmp=(Memory*(4*(len(data)+len(reservations))+2))();out=(Memory*max(1,capacity))()
    C.memset(out,0xa5,C.sizeof(out));before=bytes(out);needed=C.c_uint64()
    status=lib.lbp_normalize(a,len(a),r,len(r),tmp,len(tmp),out,capacity,C.byref(needed))
    if status: assert bytes(out)==before
    return status,[tuple(getattr(x,f) for f,_ in Memory._fields_) for x in out[:min(needed.value,capacity)]] if not status else [],needed.value


def test_normalization_overlap_rounding_and_permutation(lib):
    data=[(1,0x8fff,1,0,0),(0x3000,0x1000,2,0,0)]
    expected=[(0,4096,2,0,0),(4096,8192,1,0,0),(12288,4096,2,0,0),(16384,20480,1,0,0)]
    for perm in itertools.permutations(data):
        status,out,n=normalize(lib,perm);assert status==OK and out==expected
        assert normalize(lib,out)[1]==out
    assert normalize(lib,data,capacity=1)[0]==SMALL


def test_normalization_reservations_gaps_and_conflicts(lib):
    assert normalize(lib,[(0x1000,0x8000,1,0,0)],[(0x3000,0x1000,1,0,0)])[1]==[
        (0x1000,0x2000,1,0,0),(0x3000,0x1000,2,0,0),(0x4000,0x5000,1,0,0)]
    assert normalize(lib,[(0x1000,0x1000,1,0,0),(0x3000,0x1000,1,0,0)],[(0x1000,0x3000,1,0,0)])[0]==CONFLICT
    assert normalize(lib,[(0x1000,0x8000,1,0,0),(0x3000,0x1000,3,0,0)],[(0x2000,0x3000,1,0,0)])[0]==CONFLICT
    assert normalize(lib,[(0x1000,0x1000,99,0,0)])[1][0][2]==2
    assert normalize(lib,[(0x1000,0x1000,1,0,0),(0x1000,0x1000,1,0,8)])[1][0][2]==2
    assert normalize(lib,[(2**64-4096,8192,1,0,0)])[0]==OVERFLOW
    assert normalize(lib,[(0,0,1,0,0)])[1]==[]

@pytest.mark.parametrize('typ,expected', [(1,6),(2,6),(3,6),(4,6),(5,7),(6,7),(7,1),(8,3),(9,4),(10,5),(11,2),(14,2),(15,2),(99,2)])
def test_uefi_decoder(lib,typ,expected):
    raw=S.pack('<IIQQQQQ',typ,0,0x1000,0,2,0,0xfeed)
    out=(Memory*2)();n=C.c_uint64()
    assert lib.lbp_decode_uefi(raw,len(raw),48,1,out,2,C.byref(n))==OK
    assert (out[0].base,out[0].length,out[0].type,out[0].flags)==(0x1000,8192,expected,1)
    assert lib.lbp_decode_uefi(raw,len(raw),48,2,out,2,C.byref(n))==UNSUPPORTED
    assert lib.lbp_decode_uefi(raw,len(raw)-1,48,1,out,2,C.byref(n))==TRUNCATED

@pytest.mark.parametrize('stride', [20,24])
def test_e820_decoder_and_transaction(lib,stride):
    raw=S.pack('<QQI',0x1000,4096,1)+(S.pack('<I',1) if stride==24 else b'')
    out=(Memory*2)();n=C.c_uint64()
    assert lib.lbp_decode_e820(raw,len(raw),stride,out,2,C.byref(n))==OK and out[0].type==1
    before=bytes(out)
    bad=raw+S.pack('<QQI',2**64-10,4096,1)+(S.pack('<I',1) if stride==24 else b'')
    assert lib.lbp_decode_e820(bad,len(bad),stride,out,2,C.byref(n))==OVERFLOW and bytes(out)==before
    if stride==24:
        disabled=raw[:-4]+S.pack('<I',0)
        assert lib.lbp_decode_e820(disabled,24,24,out,2,C.byref(n))==OK and n.value==0
        persistent=raw[:-4]+S.pack('<I',3)
        assert lib.lbp_decode_e820(persistent,24,24,out,2,C.byref(n))==OK and out[0].type==2


def test_normalization_runtime_attributes_and_priority(lib):
    for typ in (1,6):
        assert normalize(lib,[(0x1000,4096,typ,1,0x8000000000000000)])[1][0][2]==7
        assert normalize(lib,[(0x1000,4096,typ,1,0x8000)])[1][0][2]==2
    assert normalize(lib,[(0x1000,4096,1,1,0x8000000000000000)],[(0x1000,4096,1,0,0)])[0]==CONFLICT
    for perm in itertools.permutations([(0x1000,4096,1,0,0),(0x1000,4096,7,0,0),(0x1000,4096,3,0,0)]):
        assert normalize(lib,perm)[1]==[(0x1000,4096,3,0,0)]


def test_normalization_page_oracle(lib):
    # Oracle độc lập duyệt byte trên miền nhỏ, không dùng thuật toán endpoints C.
    import random
    rng=random.Random(20261026)
    for _ in range(40):
        data=[]
        for _ in range(4):
            b=rng.randrange(0,4*4096);l=rng.randrange(1,3*4096)
            data.append((b,l,rng.choice([1,2]),0,0))
        status,out,_=normalize(lib,data)
        assert status==OK
        for page in range(7):
            b=page*4096
            usable=all(any(x<=byte<x+l and t==1 for x,l,t,_,_ in data) and
                       not any(x<=byte<x+l and t==2 for x,l,t,_,_ in data)
                       for byte in range(b,b+4096))
            returned=any(x<=b and b+4096<=x+l and t==1 for x,l,t,_,_ in out)
            # Conservative normalization có thể từ chối page do partial descriptor;
            # không bao giờ được trả page mà oracle thấy không an toàn.
            assert not returned or usable
        assert normalize(lib,out)[1]==out
        assert normalize(lib,list(reversed(data)))[1]==out


def test_query_and_null_arguments(lib):
    n=C.c_uint64();inp=(Memory*1)(Memory(4096,4096,1,0,0));scratch=(Memory*6)()
    assert lib.lbp_normalize(inp,1,None,0,None,0,None,0,C.byref(n))==SMALL and n.value==6
    assert lib.lbp_normalize(inp,1,None,0,scratch,6,None,0,C.byref(n))==SMALL and n.value==1
    assert lib.lbp_normalize(None,0,None,0,None,0,None,0,C.byref(n))==OK and n.value==0
    assert lib.lbp_normalize(None,1,None,0,scratch,6,None,0,C.byref(n))==INVALID
    raw=S.pack('<QQI',4096,4096,1)
    assert lib.lbp_decode_e820(raw,20,20,None,0,C.byref(n))==SMALL and n.value==1
    assert lib.lbp_decode_e820(raw,20,21,None,0,C.byref(n))==UNSUPPORTED
    assert lib.lbp_validate_note(None,32,7)==INVALID


def test_uefi_runtime_nv_overflow_and_transaction(lib):
    out=(Memory*2)();n=C.c_uint64()
    for attr,expected in [(0x8000000000000000,7),(0x8000,2)]:
        raw=S.pack('<IIQQQQ',7,0,4096,0,1,attr)
        assert lib.lbp_decode_uefi(raw,40,40,1,out,2,C.byref(n))==OK and out[0].type==expected
    before=bytes(out)
    raw=S.pack('<IIQQQQ',7,0,4096,0,2**64-1,0)
    assert lib.lbp_decode_uefi(raw,40,40,1,out,2,C.byref(n))==OVERFLOW and bytes(out)==before


def test_acpi_extended_and_span_overlap(lib):
    raw=bytearray(36);raw[:8]=b'RSD PTR ';raw[15]=2;S.pack_into('<I',raw,20,36)
    raw[8]=(-sum(raw[:20]))%256;raw[32]=(-sum(raw))%256
    external=C.create_string_buffer(bytes(raw));span=Span(0xf0000,C.cast(external,C.c_void_p),36)
    blob=bootinfo(extras=[record(8,S.pack('<QII',0xf0000,36,2),False)],features=2)
    assert validate(lib,blob,(Span*1)(span))[0]==OK
    assert validate(lib,blob,(Span*2)(span,span))[0]==CONFLICT
    external[32]=b'\0'
    assert validate(lib,blob,(Span*1)(span))[0]==INVALID


def test_extended_memory_stride(lib):
    blob=bootinfo();pos=offsets(blob)[1];old_size=S.unpack_from('<Q',blob,pos+8)[0]
    entries=[bytes(blob[pos+32+i*32:pos+64+i*32])+bytes(8) for i in range(3)]
    replacement=record(1,S.pack('<IIQ',1,40,3)+b''.join(entries))
    blob[pos:pos+old_size]=replacement;S.pack_into('<Q',blob,16,len(blob))
    assert validate(lib,blob)[0]==OK
    blob[pos+64]=1
    assert validate(lib,blob)[0]==INVALID


def test_partial_bad_and_conflicting_allocation(lib):
    assert normalize(lib,[(4097,1,3,0,0)])[1]==[(4096,4096,3,0,0)]
    assert normalize(lib,[(4096,4096,1,0,0),(4096,4096,1,0,8)],[(4096,4096,1,0,0)])[0]==CONFLICT
    assert normalize(lib,[(4096,4096,1,0,0)],[(8192,4096,1,0,0)])[0]==CONFLICT


def test_future_minor_fixed_record_extension(lib):
    blob=bootinfo();pos=offsets(blob)[5];size=S.unpack_from('<Q',blob,pos+8)[0]
    blob[pos+size:pos+size]=b'newfield';S.pack_into('<Q',blob,pos+8,size+8);S.pack_into('<Q',blob,16,len(blob))
    assert validate(lib,blob)[0]!=OK
    S.pack_into('<H',blob,10,1)
    assert validate(lib,blob)[0]==OK


def test_e820_nonvolatile_attributes_cannot_be_usable(lib):
    assert normalize(lib,[(4096,4096,1,0,3)])[1][0][2]==2
    blob=bootinfo();pos=offsets(blob)[1]+32+32
    S.pack_into('<Q',blob,pos+24,3)
    assert validate(lib,blob)[0]!=OK


def test_bootinfo_writable_alias(lib):
    blob=bootinfo();pos=offsets(blob)[4];size=S.unpack_from('<Q',blob,pos+8)[0]
    blob[pos+size:pos+size]=segment(0x110000,0x300000,4096,3)
    S.pack_into('<Q',blob,pos+8,size+32);S.pack_into('<Q',blob,pos+64,4);S.pack_into('<Q',blob,16,len(blob))
    assert validate(lib,blob)[0]==CONFLICT


def test_nonvolatile_acpi_reclaim_never_becomes_allocatable(lib):
    for flags,attrs in [(2,0),(0,3),(1,0x8000)]:
        assert normalize(lib,[(4096,4096,4,flags,attrs)])[1][0][2]==2
    raw=S.pack('<IIQQQQ',9,0,4096,0,1,0x8000)
    out=(Memory*1)();n=C.c_uint64()
    assert lib.lbp_decode_uefi(raw,40,40,1,out,1,C.byref(n))==OK and out[0].type==2
    blob=bootinfo();pos=offsets(blob)[1]+32+32
    S.pack_into('<IIQ',blob,pos+16,4,2,0)
    assert validate(lib,blob)[0]!=OK
