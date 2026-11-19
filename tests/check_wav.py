#!/usr/bin/env python3
"""WAV validity check for the tone example (CTest test 6).

Usage: check_wav.py <tone_binary> <wav_path> <expected_rate> <expected_seconds>

Runs the example binary, then opens the output with Python's wave module
and asserts the expected rate/channels/duration and that it is non-silent.
Exits non-zero with a message on any failure.
"""
import subprocess
import sys
import wave


def main() -> int:
    binary, wav_path, expected_rate, expected_seconds = (
        sys.argv[1],
        sys.argv[2],
        int(sys.argv[3]),
        float(sys.argv[4]),
    )
    proc = subprocess.run([binary, wav_path], capture_output=True, text=True)
    if proc.returncode != 0:
        print(f"tone binary failed: {proc.stderr.strip()}")
        return 1
    try:
        with wave.open(wav_path, "rb") as w:
            channels = w.getnchannels()
            sampwidth = w.getsampwidth()
            rate = w.getframerate()
            frames = w.getnframes()
            raw = w.readframes(frames)
    except wave.Error as e:
        print(f"not a valid WAV: {e}")
        return 1
    if channels != 1:
        print(f"expected mono, got {channels} channels")
        return 1
    if sampwidth != 2:
        print(f"expected 16-bit, got {sampwidth * 8}-bit")
        return 1
    if rate != expected_rate:
        print(f"expected rate {expected_rate}, got {rate}")
        return 1
    duration = frames / rate
    if abs(duration - expected_seconds) > 0.01:
        print(f"expected duration {expected_seconds}s, got {duration:.3f}s")
        return 1
    peak = 0
    for i in range(0, len(raw), 2):
        v = int.from_bytes(raw[i : i + 2], "little", signed=True)
        peak = max(peak, abs(v))
    if peak < 1000:
        print(f"output is silent (peak {peak})")
        return 1
    print(f"OK: {wav_path} ({duration:.2f}s, {rate} Hz, mono, 16-bit, peak {peak})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
