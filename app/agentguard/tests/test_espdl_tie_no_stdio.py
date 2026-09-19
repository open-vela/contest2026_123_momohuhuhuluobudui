#!/usr/bin/env python3

from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "src" / "espdl_tie_selftest.cpp"
FORBIDDEN = (
    "<cstdio>",
    "stderr",
    "stdout",
    "fprintf",
    "printf",
    "fputc",
    "fwrite",
    "print_vector",
)


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    violations = [token for token in FORBIDDEN if token in source]
    assert not violations, f"startup self-test uses stdio: {violations}"
    print("AgentGuard TIE no-stdio contract: PASS")


if __name__ == "__main__":
    main()
