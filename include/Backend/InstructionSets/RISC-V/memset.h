/*
 * Copyright (C) 2013 Regents of the University of California
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

 #include <string>

namespace RISCV::ASM {
    inline static std::string memset_s = R"(
# void *memset(void *, int, size_t)
shit.memset:
	move t0, a0  # Preserve return value

	# Defer to byte-oriented fill for small sizes
	sltiu a3, a2, 16
	bnez a3, 4f

	# Round to nearest XLEN-aligned address
	# greater than or equal to start address

	addi a3, t0, 7
	andi a3, a3, -8
	beq a3, t0, 2f  # Skip if already aligned
	# Handle initial misalignment
	sub a4, a3, t0
 1:
	sb a1, 0(t0)
	addi t0, t0, 1
	bltu t0, a3, 1b
	sub a2, a2, a4  # Update count

 2: # Duff's device with 32 XLEN stores per iteration
	# Broadcast value into all bytes
	andi a1, a1, 0xff
	slli a3, a1, 8
	or a1, a3, a1
	slli a3, a1, 16
	or a1, a3, a1
	slli a3, a1, 32
	or a1, a3, a1

	# Calculate end address
	andi a4, a2, -8
	add a3, t0, a4

	andi a4, a4, 248  # Calculate remainder
	beqz a4, 3f        # Shortcut if no remainder
	neg a4, a4
	addi a4, a4, 256  # Calculate initial offset

	# Adjust start address with offset
	sub t0, t0, a4

	# Jump into loop body
	# Assumes 32-bit instruction lengths
	la a5, 3f
	srli a4, a4, 1
	add a5, a5, a4
	jr a5
 3:
	sd a1,    0(t0)
	sd a1,    8(t0)
	sd a1,   16(t0)
	sd a1,   24(t0)
	sd a1,   32(t0)
	sd a1,   40(t0)
	sd a1,   48(t0)
	sd a1,   56(t0)
	sd a1,   64(t0)
	sd a1,   72(t0)
	sd a1,   80(t0)
	sd a1,   88(t0)
	sd a1,   96(t0)
	sd a1,  104(t0)
	sd a1,  112(t0)
	sd a1,  120(t0)
	sd a1,  128(t0)
	sd a1,  136(t0)
	sd a1,  144(t0)
	sd a1,  152(t0)
	sd a1,  160(t0)
	sd a1,  168(t0)
	sd a1,  176(t0)
	sd a1,  184(t0)
	sd a1,  192(t0)
	sd a1,  200(t0)
	sd a1,  208(t0)
	sd a1,  216(t0)
	sd a1,  224(t0)
	sd a1,  232(t0)
	sd a1,  240(t0)
	sd a1,  248(t0)
	addi t0, t0, 256
	bltu t0, a3, 3b
	andi a2, a2, 7  # Update count

 4:
	# Handle trailing misalignment
	beqz a2, 6f
	add a3, t0, a2
 5:
	sb a1, 0(t0)
	addi t0, t0, 1
	bltu t0, a3, 5b
 6:
	ret
)";
}