# example_decoder.py
# Required stateless example: format the bytes in one supplied buffer.
def decode_serial(raw_bytes: bytes) -> str:
    # Example: Prepend data length and hex dump
    length = len(raw_bytes)
    hex_dump = raw_bytes.hex(' ').upper()
    return f"[LEN:{length}] {hex_dump}"


# Optional streaming example: this delimiter-based buffer keeps incomplete
# records between calls. Replace the newline delimiter with your protocol's
# frame-completion condition.
_stream_buffer = bytearray()


def decode_stream(raw_bytes: bytes) -> str:
    _stream_buffer.extend(raw_bytes)
    records = []

    while True:
        line_end = _stream_buffer.find(b"\n")
        if line_end == -1:
            break

        record = bytes(_stream_buffer[:line_end + 1])
        del _stream_buffer[:line_end + 1]
        records.append(f"[LEN:{len(record)}] {record.hex(' ').upper()}")

    return "".join(record + "\n" for record in records)


def reset_stream() -> None:
    # The application calls this before replaying history to avoid stale state.
    _stream_buffer.clear()
