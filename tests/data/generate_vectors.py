import hashlib
import hmac
import os
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def pattern(count, seed):
    return bytes((i * 7 + seed) & 0xFF for i in range(count))


def lcg_bytes(count, seed):
    state = seed & 0xFFFFFFFF
    out = bytearray()
    for _ in range(count):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        out.append((state >> 24) & 0xFF)
    return bytes(out)


WORDS = ["star", "engine", "actor", "level", "shadow", "light", "module", "graph", "script", "mesh",
         "package", "texture", "vertex", "frame", "audio", "widget", "pak", "cook", "component", "world"]


def text_corpus(count, seed):
    state = seed & 0xFFFFFFFF
    out = bytearray()
    while len(out) < count:
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        out += WORDS[(state >> 20) % len(WORDS)].encode()
        out += b" " if (state >> 8) % 7 else b"\n"
    return bytes(out[:count])


def run_openssl(args, data):
    result = subprocess.run(["openssl"] + args, input=data, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True)
    return result.stdout


def rotl32(v, n):
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFF


def chacha_block(key, nonce, counter, rounds):
    assert len(key) == 32 and len(nonce) == 8
    counter &= 0xFFFFFFFFFFFFFFFF
    state = [0x61707865, 0x3320646E, 0x79622D32, 0x6B206574]
    state += [int.from_bytes(key[i:i + 4], "little") for i in range(0, 32, 4)]
    state += [counter & 0xFFFFFFFF, counter >> 32]
    state += [int.from_bytes(nonce[i:i + 4], "little") for i in range(0, 8, 4)]
    x = list(state)

    def quarter(a, b, c, d):
        x[a] = (x[a] + x[b]) & 0xFFFFFFFF
        x[d] = rotl32(x[d] ^ x[a], 16)
        x[c] = (x[c] + x[d]) & 0xFFFFFFFF
        x[b] = rotl32(x[b] ^ x[c], 12)
        x[a] = (x[a] + x[b]) & 0xFFFFFFFF
        x[d] = rotl32(x[d] ^ x[a], 8)
        x[c] = (x[c] + x[d]) & 0xFFFFFFFF
        x[b] = rotl32(x[b] ^ x[c], 7)

    for _ in range(rounds // 2):
        quarter(0, 4, 8, 12)
        quarter(1, 5, 9, 13)
        quarter(2, 6, 10, 14)
        quarter(3, 7, 11, 15)
        quarter(0, 5, 10, 15)
        quarter(1, 6, 11, 12)
        quarter(2, 7, 8, 13)
        quarter(3, 4, 9, 14)
    return b"".join(((x[i] + state[i]) & 0xFFFFFFFF).to_bytes(4, "little") for i in range(16))


def chacha_xor(key, nonce, counter, data, rounds=12):
    out = bytearray()
    for index in range(0, len(data), 64):
        stream = chacha_block(key, nonce, counter + index // 64, rounds)
        chunk = data[index:index + 64]
        out += bytes(a ^ b for a, b in zip(chunk, stream))
    return bytes(out)


def openssl_chacha20(key, nonce, counter, data):
    iv = (counter & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "little") + nonce
    return run_openssl(["enc", "-chacha20", "-K", key.hex(), "-iv", iv.hex()], data)


def hex_literal(data):
    return '"' + data.hex() + '"'


def raw_deflate(data, level=6, strategy=zlib.Z_DEFAULT_STRATEGY, wbits=15, mem_level=8, flush_every=0):
    comp = zlib.compressobj(level, zlib.DEFLATED, -wbits, mem_level, strategy)
    out = b""
    if flush_every:
        for i in range(0, len(data), flush_every):
            out += comp.compress(data[i:i + flush_every])
            out += comp.flush(zlib.Z_FULL_FLUSH)
        out += comp.flush(zlib.Z_FINISH)
    else:
        out = comp.compress(data) + comp.flush(zlib.Z_FINISH)
    return out


def generate_crypto():
    lines = ["#pragma once", "", "namespace vectors {", ""]

    rfc_key = bytes(range(32))
    rfc_counter = 1 | (0x09000000 << 32)
    rfc_nonce = bytes.fromhex("0000004a00000000")
    rfc_block = chacha_block(rfc_key, rfc_nonce, rfc_counter, 20)
    assert rfc_block.hex().startswith("10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e")
    assert chacha_block(bytes(32), bytes(8), 0, 8).hex().startswith("3e00ef2f895f40d67f5bb8e81f09a5a1")
    assert chacha_block(bytes(32), bytes(8), 0, 12).hex().startswith("9bf49a6a0755f953811fce125f2683d5")
    assert chacha_block(bytes(32), bytes(8), 0, 20).hex().startswith("76b8e0ada0f13d90405d6ae55386bd28")

    check_cases = [
        (bytes(32), bytes(8), 0, lcg_bytes(200, 61)),
        (rfc_key, rfc_nonce, rfc_counter, lcg_bytes(130, 62)),
        (lcg_bytes(32, 63), lcg_bytes(8, 64), 0xFFFFFFFF, lcg_bytes(300, 65)),
        (lcg_bytes(32, 66), lcg_bytes(8, 67), 0xFFFFFFFFFFFFFFFF, lcg_bytes(129, 68)),
        (lcg_bytes(32, 69), lcg_bytes(8, 70), 0x100000000, lcg_bytes(64, 71)),
    ]
    for key, nonce, counter, data in check_cases:
        expected = openssl_chacha20(key, nonce, counter, data)
        assert expected == chacha_xor(key, nonce, counter, data, 20)

    block_cases = [
        (bytes(32), bytes(8), 0),
        (bytes([0xFF] * 32), bytes([0xFF] * 8), 0xFFFFFFFFFFFFFFFF),
        (rfc_key, rfc_nonce, rfc_counter),
        (lcg_bytes(32, 81), lcg_bytes(8, 82), 1),
        (lcg_bytes(32, 83), lcg_bytes(8, 84), 0xFFFFFFFF),
        (lcg_bytes(32, 85), lcg_bytes(8, 86), 0x100000000),
        (lcg_bytes(32, 87), lcg_bytes(8, 88), 0x123456789ABCDEF0),
    ]
    lines.append("struct ChaChaBlockVector { const char* key; const char* nonce; unsigned long long counter; int rounds; const char* block; };")
    lines.append("static const ChaChaBlockVector kChaChaBlocks[] = {")
    for key, nonce, counter in block_cases:
        for rounds in (8, 12, 20):
            lines.append("    { %s, %s, %dULL, %d, %s }," % (hex_literal(key), hex_literal(nonce), counter, rounds, hex_literal(chacha_block(key, nonce, counter, rounds))))
    lines.append("};")
    lines.append("")

    xor_cases = [
        (bytes(32), bytes(8), 0, b"\x00" * 64),
        (lcg_bytes(32, 91), lcg_bytes(8, 92), 0, lcg_bytes(1, 93)),
    ]
    for index, size in enumerate([15, 63, 64, 65, 127, 128, 129, 300, 1000]):
        xor_cases.append((lcg_bytes(32, 100 + index), lcg_bytes(8, 120 + index), 0, lcg_bytes(size, 140 + index)))
    xor_cases.append((lcg_bytes(32, 151), lcg_bytes(8, 152), 0xFFFFFFFF, lcg_bytes(257, 153)))
    xor_cases.append((lcg_bytes(32, 154), lcg_bytes(8, 155), 0xFFFFFFFFFFFFFFFF, lcg_bytes(200, 156)))
    xor_cases.append((lcg_bytes(32, 157), lcg_bytes(8, 158), 7, lcg_bytes(5000, 159)))
    lines.append("struct ChaChaXorVector { const char* key; const char* nonce; unsigned long long counter; const char* plain; const char* cipher; };")
    lines.append("static const ChaChaXorVector kChaChaXor[] = {")
    for key, nonce, counter, data in xor_cases:
        lines.append("    { %s, %s, %dULL, %s, %s }," % (hex_literal(key), hex_literal(nonce), counter, hex_literal(data), hex_literal(chacha_xor(key, nonce, counter, data))))
    lines.append("};")
    lines.append("")

    assert zlib.crc32(b"123456789") == 0xCBF43926
    assert zlib.crc32(b"") == 0
    crc_messages = [b"", b"a", b"abc", b"123456789", b"The quick brown fox jumps over the lazy dog"]
    for length in [2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 255, 256, 257, 1000, 4096]:
        crc_messages.append(lcg_bytes(length, 200 + length))
    crc_messages.append(bytes(100))
    crc_messages.append(bytes([0xFF] * 100))
    lines.append("struct CrcVector { const char* data; unsigned crc; };")
    lines.append("static const CrcVector kCrc[] = {")
    for data in crc_messages:
        lines.append("    { %s, 0x%08xu }," % (hex_literal(data), zlib.crc32(data)))
    lines.append("};")
    lines.append("")

    sha_messages = [b"", b"abc", b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", b"a" * 1000000]
    assert hashlib.sha256(b"abc").hexdigest() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    assert hashlib.sha256(b"").hexdigest() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    assert hashlib.sha256(sha_messages[3]).hexdigest() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"
    lines.append("struct ShaVector { const char* name; unsigned long long length; unsigned seed; unsigned fill; const char* digest; };")
    lines.append("static const ShaVector kSha[] = {")
    lines.append('    { "empty", 0, 0, 0, %s },' % hex_literal(hashlib.sha256(b"").digest()))
    lines.append('    { "abc", 3, 0, 0, %s },' % hex_literal(hashlib.sha256(b"abc").digest()))
    lines.append('    { "two block", 56, 0, 0, %s },' % hex_literal(hashlib.sha256(sha_messages[2]).digest()))
    lines.append('    { "million a", 1000000, 0, 1, %s },' % hex_literal(hashlib.sha256(sha_messages[3]).digest()))
    for length in [1, 55, 56, 57, 63, 64, 65, 119, 120, 121, 127, 128, 129, 191, 192, 193, 1000, 4096]:
        lines.append('    { "pattern %d", %d, 3, 0, %s },' % (length, length, hex_literal(hashlib.sha256(pattern(length, 3)).digest())))
    lines.append("};")
    lines.append("")

    hmac_cases = [
        (bytes.fromhex("0b" * 20), b"Hi There"),
        (b"Jefe", b"what do ya want for nothing?"),
        (bytes.fromhex("aa" * 20), bytes.fromhex("dd" * 50)),
        (bytes(range(1, 26)), bytes.fromhex("cd" * 50)),
        (bytes.fromhex("aa" * 131), b"Test Using Larger Than Block-Size Key - Hash Key First"),
        (bytes.fromhex("aa" * 131), b"This is a test using a larger than block-size key and a larger than block-size data. The key needs to be hashed before being used by the HMAC algorithm."),
        (b"", b""),
        (b"", b"data with empty key"),
        (b"key", b""),
        (bytes([0x42] * 64), pattern(200, 9)),
        (bytes([0x42] * 65), pattern(200, 9)),
        (bytes([0x42] * 63), pattern(200, 9)),
    ]
    assert hmac.new(hmac_cases[0][0], hmac_cases[0][1], hashlib.sha256).hexdigest() == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"
    assert hmac.new(hmac_cases[1][0], hmac_cases[1][1], hashlib.sha256).hexdigest() == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"
    lines.append("struct HmacVector { const char* key; const char* data; const char* mac; };")
    lines.append("static const HmacVector kHmac[] = {")
    for key, data in hmac_cases:
        lines.append("    { %s, %s, %s }," % (hex_literal(key), hex_literal(data), hex_literal(hmac.new(key, data, hashlib.sha256).digest())))
    lines.append("};")
    lines.append("")

    cipher_cases = [
        ("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "Content/Textures/wall.png", lcg_bytes(100, 1)),
        ("Short-Key", "Levels/Main.starmap", lcg_bytes(16, 2)),
        ("k", "a", lcg_bytes(1, 3)),
        ("another key with spaces", "Content/Audio/jump.wav", lcg_bytes(4097, 4)),
        ("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "Content/Textures/wall.png", lcg_bytes(33, 5)),
    ]
    lines.append("struct CookCipherVector { const char* key; const char* context; const char* plain; const char* cipher; };")
    lines.append("static const CookCipherVector kCookCipher[] = {")
    for key, context, data in cipher_cases:
        key_bytes = hashlib.sha256(key.encode()).digest()
        nonce = hmac.new(key_bytes, ("StarPak-ChaCha12:" + context).encode(), hashlib.sha256).digest()[:8]
        encrypted = chacha_xor(key_bytes, nonce, 0, data)
        lines.append('    { "%s", "%s", %s, %s },' % (key, context, hex_literal(data), hex_literal(encrypted)))
    lines.append("};")
    lines.append("")
    lines.append("}")
    lines.append("")

    with open(os.path.join(HERE, "CryptoVectors.h"), "w", newline="\n") as handle:
        handle.write("\n".join(lines))


def generate_inflate():
    vectors = []

    def add(name, raw, **kwargs):
        compressed = raw_deflate(raw, **kwargs)
        assert zlib.decompress(compressed, -15) == raw
        vectors.append((name, raw, compressed))

    add("one_byte", b"x", level=9)
    add("two_bytes", b"ab", level=9)
    add("short_match", b"abcabc", level=9)
    add("max_length_match", b"a" + b"b" * 400, level=9)
    add("stored_small", lcg_bytes(1000, 11), level=0)
    add("stored_full_flush", lcg_bytes(2400, 12), level=0, flush_every=700)
    add("fixed_text", text_corpus(2500, 13), strategy=zlib.Z_FIXED)
    add("dynamic_text_level9", text_corpus(9000, 14), level=9)
    add("dynamic_text_level6", text_corpus(9000, 14), level=6)
    add("dynamic_text_level1", text_corpus(9000, 14), level=1)
    add("dynamic_small_window", text_corpus(6000, 15), level=9, wbits=9)
    add("dynamic_mem_level_1", text_corpus(6000, 16), level=9, mem_level=1)
    add("huffman_only", text_corpus(3000, 17), strategy=zlib.Z_HUFFMAN_ONLY)
    add("rle_runs", b"".join(bytes([i & 0xFF]) * (1 + (i * 37) % 90) for i in range(120)), strategy=zlib.Z_RLE)
    add("filtered_pattern", pattern(4000, 5), strategy=zlib.Z_FILTERED)
    add("single_byte_run", b"a" * 100000, level=9)
    add("all_byte_values", bytes(range(256)) * 4, level=9)
    add("incompressible", lcg_bytes(3000, 18), level=6)
    add("multi_block_flush", text_corpus(12000, 19), level=6, flush_every=3000)
    add("multi_block_mixed", text_corpus(4000, 20) + lcg_bytes(1500, 21) + text_corpus(4000, 22), level=6, flush_every=2500)

    lines = ["#pragma once", "", "#include <cstddef>", "#include <cstdint>", "", "namespace vectors {", ""]
    lines.append("struct InflateVector { const char* name; uint64_t rawSize; const char* rawSha256; const uint8_t* data; size_t size; };")
    lines.append("")
    for index, (name, raw, compressed) in enumerate(vectors):
        body = ",".join("0x%02x" % b for b in compressed)
        lines.append("static const uint8_t kInflateData%d[] = {%s};" % (index, body))
    lines.append("")
    lines.append("static const InflateVector kInflate[] = {")
    for index, (name, raw, compressed) in enumerate(vectors):
        lines.append('    { "%s", %d, "%s", kInflateData%d, %d },' % (name, len(raw), hashlib.sha256(raw).hexdigest(), index, len(compressed)))
    lines.append("};")
    lines.append("")
    lines.append("}")
    lines.append("")

    with open(os.path.join(HERE, "InflateVectors.h"), "w", newline="\n") as handle:
        handle.write("\n".join(lines))


if __name__ == "__main__":
    generate_crypto()
    generate_inflate()
    print("generated")
