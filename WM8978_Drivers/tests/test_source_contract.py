#!/usr/bin/env python3
"""Static contract checks for the WM8978 register tables and public API."""

from __future__ import annotations

import re
from pathlib import Path


if not __debug__:
    raise SystemExit(
        "SOURCE CONTRACT: ERROR: Python optimization disables required checks"
    )


ROOT = Path(__file__).resolve().parents[1]
REGS = (ROOT / "wm8978_regs.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "wm8978.c").read_text(encoding="utf-8")
HEADER = (ROOT / "wm8978.h").read_text(encoding="utf-8")


EXPECTED = {
    0: 0x000, 1: 0x000, 2: 0x000, 3: 0x000, 4: 0x050, 5: 0x000,
    6: 0x140, 7: 0x000, 8: 0x000, 9: 0x000, 10: 0x000, 11: 0x0FF,
    12: 0x0FF, 13: 0x000, 14: 0x100, 15: 0x0FF, 16: 0x0FF,
    18: 0x12C, 19: 0x02C, 20: 0x02C, 21: 0x02C, 22: 0x02C,
    24: 0x032, 25: 0x000, 27: 0x000, 28: 0x000, 29: 0x000,
    30: 0x000, 32: 0x038, 33: 0x00B, 34: 0x032, 35: 0x000,
    36: 0x008, 37: 0x00C, 38: 0x093, 39: 0x0E9, 41: 0x000,
    43: 0x000, 44: 0x033, 45: 0x010, 46: 0x010, 47: 0x100,
    48: 0x100, 49: 0x002, 50: 0x001, 51: 0x001, 52: 0x039,
    53: 0x039, 54: 0x039, 55: 0x039, 56: 0x001, 57: 0x001,
}

EXPECTED_WRITABLE = {
    0: 0x1FF, 1: 0x1FF, 2: 0x1FF, 3: 0x1EF, 4: 0x1FF, 5: 0x03F,
    6: 0x1FD, 7: 0x00F, 8: 0x03F, 9: 0x1F0, 10: 0x04F,
    11: 0x1FF, 12: 0x1FF, 13: 0x0FF, 14: 0x1FB, 15: 0x1FF,
    16: 0x1FF, 18: 0x17F, 19: 0x17F, 20: 0x17F, 21: 0x17F,
    22: 0x07F, 24: 0x1FF, 25: 0x07F, 27: 0x1FF, 28: 0x17F,
    29: 0x17F, 30: 0x17F, 32: 0x1BF, 33: 0x0FF, 34: 0x1FF,
    35: 0x00F, 36: 0x01F, 37: 0x03F, 38: 0x1FF, 39: 0x1FF,
    41: 0x00F, 43: 0x03F, 44: 0x177, 45: 0x1FF, 46: 0x1FF,
    47: 0x177, 48: 0x177, 49: 0x07F, 50: 0x1FF, 51: 0x1FF,
    52: 0x1FF, 53: 0x1FF, 54: 0x1FF, 55: 0x1FF, 56: 0x04F,
    57: 0x07F,
}

EXPECTED_ONE_SHOT = {
    11: 0x100, 12: 0x100, 15: 0x100, 16: 0x100,
    27: 0x100, 28: 0x100, 29: 0x100, 30: 0x100,
    45: 0x100, 46: 0x100, 52: 0x100, 53: 0x100,
    54: 0x100, 55: 0x100,
}


def extract_registers() -> dict[str, int]:
    pattern = re.compile(
        r"^#define\s+(WM8978_REG_[A-Z0-9_]+)\s+"
        r"\(\(uint8_t\)0x([0-9A-F]+)U\)",
        re.MULTILINE,
    )
    return {name: int(value, 16) for name, value in pattern.findall(REGS)}


def extract_defaults() -> dict[int, int]:
    pattern = re.compile(
        r"^#define\s+WM8978_R(\d{2})_RESET_VALUE\s+"
        r"\(\(uint16_t\)0x([0-9A-F]+)U\)",
        re.MULTILINE,
    )
    return {int(address): int(value, 16) for address, value in pattern.findall(REGS)}


def pack(address: int, value: int) -> tuple[int, int]:
    assert address in EXPECTED
    assert 0 <= value <= 0x1FF
    return ((address << 1) | (value >> 8), value & 0xFF)


def extract_source_hex_table(
    declaration: str,
    registers: dict[str, int],
) -> dict[int, int]:
    table_pattern = re.compile(
        rf"{re.escape(declaration)}.*?=\s*\{{(?P<body>.*?)\n\}};",
        re.DOTALL,
    )
    match = table_pattern.search(SOURCE)
    assert match is not None, f"missing source table {declaration}"
    entry_pattern = re.compile(
        r"\[(WM8978_REG_[A-Z0-9_]+)\]\s*=\s*0x([0-9A-F]+)U"
    )
    return {
        registers[name]: int(value, 16)
        for name, value in entry_pattern.findall(match.group("body"))
    }


def main() -> None:
    registers = extract_registers()
    addresses = set(registers.values())
    defaults = extract_defaults()
    writable = extract_source_hex_table("wm8978_writable_masks", registers)
    one_shot = extract_source_hex_table("wm8978_transient_masks", registers)

    assert len(registers) == 52, f"expected 52 register macros, got {len(registers)}"
    assert len(addresses) == 52, "register-address macros must be unique"
    assert addresses == set(EXPECTED), "implemented register whitelist drift"
    assert defaults == EXPECTED, "datasheet reset-default table drift"
    assert writable == EXPECTED_WRITABLE, "datasheet writable-mask table drift"
    assert one_shot == EXPECTED_ONE_SHOT, "one-shot shadow policy drift"
    assert {0x11, 0x17, 0x1A, 0x1F, 0x28, 0x2A}.isdisjoint(addresses)
    assert pack(0x04, 0x1AB) == (0x09, 0xAB)
    assert pack(0x34, 0x139) == (0x69, 0x39)

    for name in registers:
        assert f"[{name}] = 1U" in SOURCE, f"{name} missing from valid table"
        assert f"[{name}] = WM8978_R" in SOURCE or name == "WM8978_REG_SOFTWARE_RESET"

    assert "WM8978_R10_SOFTMUTE_RAW" in REGS
    assert (
        "WM8978_R41_DEPTH3D_MASK                  ((uint16_t)0x00FU)"
        in REGS
    )
    assert "WM8978_EQ_BW" not in REGS
    assert "WM8978_R19_EQ2BW" in REGS
    assert "WM8978_R20_EQ3BW" in REGS
    assert "WM8978_R21_EQ4BW" in REGS
    assert "[WM8978_REG_3D_CONTROL] = 0x00FU" in SOURCE
    assert "wm8978_set_soft_mute" not in HEADER
    assert "wm8978_transient_masks" in SOURCE
    assert "wm8978_writable_masks" in SOURCE
    assert "WM8978_LIFECYCLE_DESYNCHRONIZED" in HEADER
    assert "device->lifecycle = WM8978_LIFECYCLE_DESYNCHRONIZED" in SOURCE
    assert "WM8978_R18_EQ3DMODE" in SOURCE
    print(
        "SOURCE CONTRACT: PASS "
        "(52 registers, defaults, masks, one-shot policy, packing and guards)"
    )


if __name__ == "__main__":
    main()
