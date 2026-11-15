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


def aes_ecb(key, block):
    return run_openssl(["enc", "-aes-256-ecb", "-nopad", "-K", key.hex()], block)


def aes_ctr(key, counter, data):
    return run_openssl(["enc", "-aes-256-ctr", "-K", key.hex(), "-iv", counter.hex()], data)


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

    fips_key = bytes(range(32))
    fips_plain = bytes.fromhex("00112233445566778899aabbccddeeff")
    fips_cipher = aes_ecb(fips_key, fips_plain)
    assert fips_cipher.hex() == "8ea2b7ca516745bfeafc49904b496089"

    ecb_cases = [(fips_key, fips_plain)]
    for seed in range(1, 7):
        ecb_cases.append((lcg_bytes(32, seed), lcg_bytes(16, seed + 100)))
    ecb_cases.append((bytes(32), bytes(16)))
    ecb_cases.append((bytes([0xFF] * 32), bytes([0xFF] * 16)))
    lines.append("struct AesBlockVector { const char* key; const char* plain; const char* cipher; };")
    lines.append("static const AesBlockVector kAesBlocks[] = {")
    for key, plain in ecb_cases:
        lines.append("    { %s, %s, %s }," % (hex_literal(key), hex_literal(plain), hex_literal(aes_ecb(key, plain))))
    lines.append("};")
    lines.append("")

    sp_key = bytes.fromhex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4")
    sp_counter = bytes.fromhex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff")
    sp_plain = bytes.fromhex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710")
    sp_cipher = aes_ctr(sp_key, sp_counter, sp_plain)
    assert sp_cipher.hex() == "601ec313775789a5b7a7f504bbf3d228f443e3ca4d62b59aca84e990cacaf5c52b0930daa23de94ce87017ba2d84988ddfc9c58db67aada613c2dd08457941a6"

    ctr_cases = [(sp_key, sp_counter, sp_plain)]
    carry_counters = [
        bytes.fromhex("000000000000000000000000fffffffe"),
        bytes.fromhex("00000000000000000000ffffffffffff"),
        bytes.fromhex("ffffffffffffffffffffffffffffffff"),
        bytes.fromhex("0102030405060708090a0b0c0d0e0fff"),
        bytes(16),
    ]
    sizes = [1, 15, 16, 17, 31, 33, 100, 257]
    for index, counter in enumerate(carry_counters):
        key = lcg_bytes(32, 50 + index)
        data = lcg_bytes(sizes[index % len(sizes)] + 48, 70 + index)
        ctr_cases.append((key, counter, data))
    for size in sizes:
        ctr_cases.append((lcg_bytes(32, 900 + size), lcg_bytes(16, 800 + size), lcg_bytes(size, 700 + size)))
    ctr_cases.append((lcg_bytes(32, 33), bytes.fromhex("ffffffffffffffffffffffffffffff00"), lcg_bytes(5000, 34)))
    lines.append("struct AesCtrVector { const char* key; const char* counter; const char* plain; const char* cipher; };")
    lines.append("static const AesCtrVector kAesCtr[] = {")
    for key, counter, data in ctr_cases:
        lines.append("    { %s, %s, %s, %s }," % (hex_literal(key), hex_literal(counter), hex_literal(data), hex_literal(aes_ctr(key, counter, data))))
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
        nonce = hmac.new(key_bytes, ("StarPak-CTR:" + context).encode(), hashlib.sha256).digest()
        encrypted = aes_ctr(key_bytes, nonce[:16], data)
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
