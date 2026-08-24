#!/usr/bin/env python3
"""Execute a freestanding RV32I test ELF without external dependencies.

This intentionally implements only the base integer instructions emitted by
the dedicated ``-march=rv32i -mno-relax`` test build. It is not a board or
peripheral simulator; it exists to execute the platform-independent fake-port
test main() and propagate its return value to the test script.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


ELF_MACHINE_RISCV = 243
PT_LOAD = 1
PAGE_BITS = 12
PAGE_SIZE = 1 << PAGE_BITS
STACK_BOTTOM = 0x7FFF0000
STACK_TOP = 0x80000000
DEFAULT_MAX_STEPS = 5_000_000


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def s32(value: int) -> int:
    value &= 0xFFFFFFFF
    return value - 0x1_0000_0000 if value & 0x8000_0000 else value


def sign_extend(value: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (value & (sign - 1)) - (value & sign)


class Memory:
    def __init__(self) -> None:
        self.pages: dict[int, bytearray] = {}

    def map_range(self, address: int, size: int) -> None:
        if size < 0 or address < 0 or address + size > 0x1_0000_0000:
            raise ValueError("invalid memory mapping")
        if size == 0:
            return
        first = address >> PAGE_BITS
        last = (address + size - 1) >> PAGE_BITS
        for number in range(first, last + 1):
            self.pages.setdefault(number, bytearray(PAGE_SIZE))

    def _page(self, address: int) -> tuple[bytearray, int]:
        if not 0 <= address <= 0xFFFFFFFF:
            raise RuntimeError(f"memory address out of range: 0x{address:x}")
        number = address >> PAGE_BITS
        page = self.pages.get(number)
        if page is None:
            raise RuntimeError(f"unmapped memory access at 0x{address:08x}")
        return page, address & (PAGE_SIZE - 1)

    def read8(self, address: int) -> int:
        page, offset = self._page(address)
        return page[offset]

    def write8(self, address: int, value: int) -> None:
        page, offset = self._page(address)
        page[offset] = value & 0xFF

    def read16(self, address: int) -> int:
        return self.read8(address) | (self.read8(address + 1) << 8)

    def read32(self, address: int) -> int:
        return self.read16(address) | (self.read16(address + 2) << 16)

    def write16(self, address: int, value: int) -> None:
        self.write8(address, value)
        self.write8(address + 1, value >> 8)

    def write32(self, address: int, value: int) -> None:
        self.write16(address, value)
        self.write16(address + 2, value >> 16)

    def load(self, address: int, data: bytes, memory_size: int) -> None:
        if len(data) > memory_size:
            raise ValueError("ELF segment file size exceeds memory size")
        self.map_range(address, memory_size)
        for offset, value in enumerate(data):
            self.write8(address + offset, value)


def load_elf(path: Path) -> tuple[Memory, int]:
    data = path.read_bytes()
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise ValueError("not an ELF file")
    if data[4] != 1 or data[5] != 1:
        raise ValueError("only 32-bit little-endian ELF is supported")

    header = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    machine = header[1]
    entry = header[3]
    program_offset = header[4]
    program_entry_size = header[8]
    program_count = header[9]
    if machine != ELF_MACHINE_RISCV:
        raise ValueError(f"ELF machine {machine} is not RISC-V")
    if program_entry_size < 32:
        raise ValueError("invalid ELF program-header size")

    memory = Memory()
    loaded = 0
    for index in range(program_count):
        offset = program_offset + index * program_entry_size
        if offset + 32 > len(data):
            raise ValueError("truncated ELF program headers")
        fields = struct.unpack_from("<IIIIIIII", data, offset)
        segment_type, file_offset, virtual_address = fields[:3]
        file_size, memory_size = fields[4], fields[5]
        if segment_type != PT_LOAD:
            continue
        if file_offset + file_size > len(data):
            raise ValueError("truncated ELF load segment")
        memory.load(
            virtual_address,
            data[file_offset : file_offset + file_size],
            memory_size,
        )
        loaded += 1

    if loaded == 0:
        raise ValueError("ELF contains no loadable segment")
    memory.map_range(STACK_BOTTOM, STACK_TOP - STACK_BOTTOM)
    return memory, entry


class Rv32iMachine:
    def __init__(self, memory: Memory, entry: int) -> None:
        self.memory = memory
        self.registers = [0] * 32
        self.registers[1] = 0  # main() returns to the sentinel PC 0.
        self.registers[2] = STACK_TOP - 16
        self.pc = entry
        self.steps = 0

    def _load(self, funct3: int, address: int) -> int:
        if funct3 == 0:  # LB
            return u32(sign_extend(self.memory.read8(address), 8))
        if funct3 == 1:  # LH
            return u32(sign_extend(self.memory.read16(address), 16))
        if funct3 == 2:  # LW
            return self.memory.read32(address)
        if funct3 == 4:  # LBU
            return self.memory.read8(address)
        if funct3 == 5:  # LHU
            return self.memory.read16(address)
        raise RuntimeError(f"unsupported load funct3 {funct3}")

    def _store(self, funct3: int, address: int, value: int) -> None:
        if funct3 == 0:  # SB
            self.memory.write8(address, value)
            return
        if funct3 == 1:  # SH
            self.memory.write16(address, value)
            return
        if funct3 == 2:  # SW
            self.memory.write32(address, value)
            return
        raise RuntimeError(f"unsupported store funct3 {funct3}")

    def step(self) -> None:
        pc = self.pc
        if pc & 3:
            raise RuntimeError(f"unaligned instruction address 0x{pc:08x}")
        instruction = self.memory.read32(pc)
        opcode = instruction & 0x7F
        rd = (instruction >> 7) & 0x1F
        funct3 = (instruction >> 12) & 0x7
        rs1 = (instruction >> 15) & 0x1F
        rs2 = (instruction >> 20) & 0x1F
        funct7 = (instruction >> 25) & 0x7F
        a = self.registers[rs1]
        b = self.registers[rs2]
        next_pc = u32(pc + 4)
        result: int | None = None

        if opcode == 0x37:  # LUI
            result = instruction & 0xFFFFF000
        elif opcode == 0x17:  # AUIPC
            result = u32(pc + (instruction & 0xFFFFF000))
        elif opcode == 0x6F:  # JAL
            immediate = (
                (((instruction >> 31) & 1) << 20)
                | (((instruction >> 12) & 0xFF) << 12)
                | (((instruction >> 20) & 1) << 11)
                | (((instruction >> 21) & 0x3FF) << 1)
            )
            result = next_pc
            next_pc = u32(pc + sign_extend(immediate, 21))
        elif opcode == 0x67:  # JALR
            if funct3 != 0:
                raise RuntimeError("invalid JALR funct3")
            immediate = sign_extend(instruction >> 20, 12)
            target = u32(a + immediate) & ~1
            result = next_pc
            next_pc = target
        elif opcode == 0x63:  # BRANCH
            immediate = (
                (((instruction >> 31) & 1) << 12)
                | (((instruction >> 7) & 1) << 11)
                | (((instruction >> 25) & 0x3F) << 5)
                | (((instruction >> 8) & 0xF) << 1)
            )
            conditions = {
                0: a == b,
                1: a != b,
                4: s32(a) < s32(b),
                5: s32(a) >= s32(b),
                6: a < b,
                7: a >= b,
            }
            if funct3 not in conditions:
                raise RuntimeError(f"unsupported branch funct3 {funct3}")
            if conditions[funct3]:
                next_pc = u32(pc + sign_extend(immediate, 13))
        elif opcode == 0x03:  # LOAD
            immediate = sign_extend(instruction >> 20, 12)
            result = self._load(funct3, u32(a + immediate))
        elif opcode == 0x23:  # STORE
            immediate = ((instruction >> 25) << 5) | ((instruction >> 7) & 0x1F)
            self._store(
                funct3,
                u32(a + sign_extend(immediate, 12)),
                b,
            )
        elif opcode == 0x13:  # OP-IMM
            immediate = sign_extend(instruction >> 20, 12)
            shift = (instruction >> 20) & 0x1F
            if funct3 == 0:
                result = u32(a + immediate)
            elif funct3 == 2:
                result = int(s32(a) < immediate)
            elif funct3 == 3:
                result = int(a < u32(immediate))
            elif funct3 == 4:
                result = a ^ u32(immediate)
            elif funct3 == 6:
                result = a | u32(immediate)
            elif funct3 == 7:
                result = a & u32(immediate)
            elif funct3 == 1 and funct7 == 0:
                result = u32(a << shift)
            elif funct3 == 5 and funct7 == 0:
                result = a >> shift
            elif funct3 == 5 and funct7 == 0x20:
                result = u32(s32(a) >> shift)
            else:
                raise RuntimeError(
                    f"unsupported OP-IMM funct3/funct7 {funct3}/{funct7}"
                )
        elif opcode == 0x33:  # OP
            key = (funct7, funct3)
            operations = {
                (0x00, 0): lambda: u32(a + b),
                (0x20, 0): lambda: u32(a - b),
                (0x00, 1): lambda: u32(a << (b & 0x1F)),
                (0x00, 2): lambda: int(s32(a) < s32(b)),
                (0x00, 3): lambda: int(a < b),
                (0x00, 4): lambda: a ^ b,
                (0x00, 5): lambda: a >> (b & 0x1F),
                (0x20, 5): lambda: u32(s32(a) >> (b & 0x1F)),
                (0x00, 6): lambda: a | b,
                (0x00, 7): lambda: a & b,
            }
            operation = operations.get(key)
            if operation is None:
                raise RuntimeError(f"unsupported OP funct7/funct3 {key}")
            result = operation()
        elif opcode == 0x0F:  # FENCE/FENCE.I; no concurrent device here.
            pass
        else:
            raise RuntimeError(
                f"unsupported opcode 0x{opcode:02x} "
                f"(instruction 0x{instruction:08x})"
            )

        if result is not None and rd != 0:
            self.registers[rd] = u32(result)
        self.registers[0] = 0
        self.pc = next_pc
        self.steps += 1

    def run(self, max_steps: int) -> int:
        while self.pc != 0:
            if self.steps >= max_steps:
                raise RuntimeError(
                    f"instruction limit {max_steps} reached at 0x{self.pc:08x}"
                )
            try:
                self.step()
            except Exception as error:
                raise RuntimeError(
                    f"RV32I execution failed at PC 0x{self.pc:08x} "
                    f"after {self.steps} steps: {error}"
                ) from error
        return s32(self.registers[10])


def self_test_machine() -> None:
    """Check basic execution and non-zero main-return propagation."""
    memory = Memory()
    memory.map_range(0x1000, 8)
    memory.write32(0x1000, 0x00700513)  # addi a0, zero, 7
    memory.write32(0x1004, 0x00008067)  # jalr zero, 0(ra)
    memory.map_range(STACK_BOTTOM, STACK_TOP - STACK_BOTTOM)
    machine = Rv32iMachine(memory, 0x1000)
    result = machine.run(8)
    if result != 7:
        raise RuntimeError(f"internal RV32I self-test returned {result}, expected 7")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("--max-steps", type=int, default=DEFAULT_MAX_STEPS)
    args = parser.parse_args()

    try:
        self_test_machine()
        memory, entry = load_elf(args.elf)
        machine = Rv32iMachine(memory, entry)
        result = machine.run(args.max_steps)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"RV32I TEST ERROR: {error}", file=sys.stderr)
        return 2

    if result != 0:
        print(
            f"RV32I BEHAVIOR TEST: FAIL ({result} assertion failures, "
            f"{machine.steps} instructions)",
            file=sys.stderr,
        )
        return 1

    print(f"RV32I BEHAVIOR TEST: PASS ({machine.steps} instructions)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
