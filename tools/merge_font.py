import struct

def get_font_names(path):
    with open(path, 'rb') as f:
        data = f.read()

    # TTC 여부 확인
    offsets = []
    if data[:4] == b'ttcf':
        num = struct.unpack_from('>I', data, 8)[0]
        for i in range(num):
            offsets.append(struct.unpack_from('>I', data, 12 + i*4)[0])
    else:
        offsets = [0]

    for idx, base in enumerate(offsets):
        num_tables = struct.unpack_from('>H', data, base + 4)[0]
        # 테이블 레코드 탐색
        for i in range(num_tables):
            pos = base + 12 + i * 16
            tag = data[pos:pos+4]
            if tag == b'name':
                tbl_offset = struct.unpack_from('>I', data, pos + 8)[0]
                count  = struct.unpack_from('>H', data, tbl_offset + 2)[0]
                str_off = struct.unpack_from('>H', data, tbl_offset + 4)[0]
                for j in range(count):
                    r = tbl_offset + 6 + j * 12
                    pid    = struct.unpack_from('>H', data, r)[0]
                    name_id= struct.unpack_from('>H', data, r + 6)[0]
                    length = struct.unpack_from('>H', data, r + 8)[0]
                    offset = struct.unpack_from('>H', data, r + 10)[0]
                    # nameID 1=Family, 4=Full name, 6=PostScript
                    if name_id in (1, 4, 6):
                        s = tbl_offset + str_off + offset
                        raw = data[s:s+length]
                        try:
                            name = raw.decode('utf-16-be') if pid == 3 else raw.decode('latin-1')
                            labels = {1:'Family', 4:'Full', 6:'PostScript'}
                            print(f"  [{idx}] {labels[name_id]:12s}: {name}")
                        except:
                            pass
                break

def patch_ttf_offsets(font_data, base_offset):
    """TTF 내부 테이블 오프셋을 TTC 파일 기준으로 재조정"""
    data = bytearray(font_data)

    # Offset Table: sfVersion(4), numTables(2), ...
    num_tables = struct.unpack_from('>H', data, 4)[0]

    # Table Records 시작 위치: 12바이트
    record_pos = 12
    for i in range(num_tables):
        # 레코드 구조: tag(4) + checksum(4) + offset(4) + length(4)
        old_offset = struct.unpack_from('>I', data, record_pos + 8)[0]
        new_offset = old_offset + base_offset
        struct.pack_into('>I', data, record_pos + 8, new_offset)
        record_pos += 16

    return bytes(data)


def make_ttc(font_paths, output_path):
    fonts = []
    for p in font_paths:
        with open(p, 'rb') as f:
            fonts.append(f.read())

    num_fonts = len(fonts)
    header_size = 12 + num_fonts * 4

    # 각 TTF의 base offset 계산
    offsets = []
    pos = header_size
    for font in fonts:
        offsets.append(pos)
        pos += len(font)

    with open(output_path, 'wb') as out:
        # TTC 헤더
        out.write(b'ttcf')
        out.write(struct.pack('>I', 0x00010000))  # version 1.0
        out.write(struct.pack('>I', num_fonts))
        for offset in offsets:
            out.write(struct.pack('>I', offset))

        # 오프셋 패치 후 TTF 데이터 기록
        for font, base in zip(fonts, offsets):
            patched = patch_ttf_offsets(font, base)
            out.write(patched)

    print(f"완료: {output_path}  ({pos:,} bytes)")
    get_font_names(output_path)
    for i, p in enumerate(font_paths):
        print(f"  index {i} → {p}")


make_ttc(
    ['fonts/NanumGothic.ttf',
     'fonts/NanumGothicBold.ttf'],
    'NanumGothic.ttc'
)

make_ttc(
    ['fonts/NanumSquareR.ttf',
     'fonts/NanumSquareB.ttf'],
    'NanumSquare.ttc'
)
make_ttc(
    ['fonts/NanumSquareRoundR.ttf',
     'fonts/NanumSquareRoundB.ttf'],
    'NanumSquareRound.ttc'
)

make_ttc(
    ['fonts/Poppins-Regular.ttf',
     'fonts/Poppins-Bold.ttf'],
    'Poppins.ttc'
)

make_ttc(
    ['fonts/NotoSansKR-Regular.otf',
     'fonts/NotoSansKR-Bold.otf'],
    'NotoSansKR.ttc'
)

make_ttc(
    ['fonts/Roboto-Regular.ttf',
     'fonts/Roboto-Bold.ttf'],
    'Roboto.ttc'
)