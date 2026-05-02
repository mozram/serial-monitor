# example_decoder.py
def decode_serial(raw_bytes: bytes) -> str:
    # Example: Prepend data length and hex dump
    length = len(raw_bytes)
    hex_dump = raw_bytes.hex(' ').upper()
    return f"[LEN:{length}] {hex_dump}"
