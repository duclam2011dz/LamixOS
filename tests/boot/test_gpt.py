# SPDX-License-Identifier: GPL-3.0-only
import struct as S
import uuid
import zlib
import pytest
from gpt_model import layout, validate, validate_pmbr, MIB, ROLES, TYPES, SYSTEM, USER


from gpt_fixture import fixtures


def fix_crc(data):
    data[16:20]=bytes(4);S.pack_into('<I',data,16,zlib.crc32(data[:92]))

@pytest.mark.parametrize('block,first', [(512,34),(4096,6)])
def test_reference_layout(block,first):
    data=fixtures(block);spec=validate(*data)
    assert spec['first']==first
    assert [s*block for s,e in spec['partitions']]==[x*MIB for x in (1,3,259,2307,4355)]
    assert spec['partitions'][4][1]==spec['blocks']-16384//block-2
    assert (spec['partitions'][4][1]-spec['partitions'][4][0]+1)*block>3*1024**3


def test_guid_bytes_and_derivation():
    assert TYPES[0].bytes_le.hex()=='4861682149646f6e744e656564454649'
    assert TYPES[1].bytes_le.hex()=='28732ac11ff8d211ba4b00a0c93ec93b'
    for role,expected in [('system',SYSTEM),('user-data',USER)]:
        assert uuid.uuid5(uuid.NAMESPACE_URL,'https://github.com/duclam2011dz/LamixOS/'+role)==expected

@pytest.mark.parametrize('which,off', [(0,0),(0,16),(0,92),(1,56),(2,127),(3,0),(4,440),(4,446),(4,462),(4,510)])
def test_metadata_corruption(which,off):
    data=fixtures();data[which][off]^=1
    with pytest.raises(ValueError):validate(*data)

@pytest.mark.parametrize('off,fmt,val',[(24,'Q',2),(32,'Q',3),(40,'Q',2048),(48,'Q',2**64-1),
                                      (72,'Q',1),(80,'I',129),(84,'I',256),(20,'I',1)])
def test_bad_header_with_valid_crc(off,fmt,val):
    data=fixtures();S.pack_into('<'+fmt,data[0],off,val);fix_crc(data[0])
    with pytest.raises(ValueError):validate(*data)

@pytest.mark.parametrize('off,fmt,val',[(32,'Q',0),(40,'Q',2**64-1),(32,'Q',2049),(48,'Q',1)])
def test_partition_corruption_with_valid_crcs(off,fmt,val):
    data=fixtures();S.pack_into('<'+fmt,data[2],off,val);data[3][:]=data[2]
    for h in data[:2]:S.pack_into('<I',h,88,zlib.crc32(data[2]));fix_crc(h)
    with pytest.raises(ValueError):validate(*data)


def test_duplicate_guid_and_unknown_partition():
    for pos in (128+16,5*128):
        data=fixtures();data[2][pos:pos+16]=data[2][16:32];data[3][:]=data[2]
        for h in data[:2]:S.pack_into('<I',h,88,zlib.crc32(data[2]));fix_crc(h)
        with pytest.raises(ValueError):validate(*data)

@pytest.mark.parametrize('size,block',[(4355*MIB,512),(8*1024**3+1,512),(8*1024**3,1024),(-512,512),(2**64,512)])
def test_invalid_disk(size,block):
    with pytest.raises(ValueError):layout(size,block)


def test_pmbr_saturation_and_4k_reserved():
    data=fixtures();pmbr=data[4]
    S.pack_into('<I',pmbr,458,0xffffffff)
    validate_pmbr(pmbr,2**33,512)
    S.pack_into('<I',pmbr,458,1)
    with pytest.raises(ValueError):validate_pmbr(pmbr,2**33,512)
    data=fixtures(4096);data[4][512]=1
    with pytest.raises(ValueError):validate(*data)
