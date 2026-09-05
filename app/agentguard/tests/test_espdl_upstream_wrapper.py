#!/usr/bin/env python3
"""Keep the ESP-DL v3.2.0 face detector wrapper byte-for-byte upstream."""

from hashlib import sha256
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "human_face_detect.cpp":
        "c71b6005d7091b71b49a2408d150791c3731efe3c9ba048075ed122a2ce8623e",
    "human_face_detect.hpp":
        "5eca1e9009acbd32b804017588b0fc2092fe38ce9e0eab4ccefaad1f0f2b89a9",
}


def main() -> int:
    source_dir = ROOT / "third_party" / "human_face_detect"
    mismatches = []
    for name, expected in EXPECTED.items():
        actual = sha256((source_dir / name).read_bytes()).hexdigest()
        if actual != expected:
            mismatches.append(f"{name}: expected {expected}, got {actual}")

    if mismatches:
        print("ESP-DL upstream wrapper integrity: FAIL")
        print("\n".join(mismatches))
        return 1

    print("ESP-DL upstream wrapper integrity: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
