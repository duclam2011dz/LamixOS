# SPDX-License-Identifier: GPL-3.0-only
"""Encoder metadata trong memory dùng cho fixtures, không file I/O."""
import struct as S
import uuid
import zlib
from gpt_model import layout, ROLES, TYPES


def fixtures(block=512, size=8*1024**3):
    spec=layout(size,block);disk=uuid.UUID('12345678-1234-5678-9abc-def012345678').bytes_le
    entries=bytearray(16384)
    for i,((start,end),name,typ) in enumerate(zip(spec['partitions'],ROLES,TYPES)):
        ident=uuid.UUID(int=i+1).bytes_le
        S.pack_into('<16s16sQQQ72s',entries,i*128,typ.bytes_le,ident,start,end,0,name.encode('utf-16le'))
    def header(current,other,array):
        data=bytearray(block)
        S.pack_into('<8sIIIIQQQQ16sQIII',data,0,b'EFI PART',0x10000,92,0,0,current,other,
                    spec['first'],spec['last'],disk,array,128,128,zlib.crc32(entries))
        S.pack_into('<I',data,16,zlib.crc32(data[:92]))
        return data
    pmbr=bytearray(block);pmbr[446:450]=b'\0\0\2\0';pmbr[450]=0xee;pmbr[451:454]=b'\xff'*3
    S.pack_into('<II',pmbr,454,1,min(spec['blocks']-1,0xffffffff));pmbr[510:512]=b'\x55\xaa'
    return [header(1,spec['blocks']-1,2),header(spec['blocks']-1,1,spec['backup_array']),entries,bytearray(entries),pmbr,size,block]
