#!/usr/bin/env python3
"""apx_pack.py <image.bin> <out.APX> — finishes an Algoradix patch image:
writes load_size and CRC-32 into the header (see firmware/patchsdk/apx.h) and checks the limits."""
import sys, struct, zlib
LOAD_ADDR, LOAD_MAX = 0x00000400, 63 * 1024
src, dst = sys.argv[1], sys.argv[2]
img = bytearray(open(src, 'rb').read())
magic, abi, hdr_size, load_size, crc, load_addr, bss_start, bss_end = struct.unpack_from('<IHHIIIII', img, 0)
if magic != 0x31585041: sys.exit(f"{src}: no APX1 header at the start of the image")
if load_addr != LOAD_ADDR: sys.exit(f"{src}: linked for {load_addr:#x}, expected {LOAD_ADDR:#x}")
while len(img) & 3: img.append(0)
if bss_end == 0: bss_start = bss_end = LOAD_ADDR + len(img)
if bss_end - LOAD_ADDR > LOAD_MAX: sys.exit(f"{src}: image + bss = {bss_end - LOAD_ADDR} bytes, limit {LOAD_MAX}")
struct.pack_into('<I', img, 8, len(img)); struct.pack_into('<I', img, 12, 0)
struct.pack_into('<I', img, 12, zlib.crc32(bytes(img)) & 0xFFFFFFFF)
open(dst, 'wb').write(img)
name = bytes(img[28:44]).split(b'\0')[0].decode()
print(f"  {dst.split('/')[-1]:14s} {name:12s} image {len(img):6d} B  bss {bss_end - bss_start:6d} B  total {bss_end - LOAD_ADDR:6d} / {LOAD_MAX}")
