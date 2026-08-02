# 07-cpi — instruction-by-instruction accounting of the marginal hop

The exact count behind the invoke-surcharge verdict (see README). Source:
objdump of the deployed binaries (`tn_example_07_caller_c.elf`,
`tn_example_07_callee_c.elf`). Encoding cost: 2 CU per compressed (2-byte)
instruction, 4 CU per full-width; loads/stores add their access width in
bytes. The marginal path is one extra `cpi_n` loop iteration.

## Caller: loop body (start, 0x30000e4–0x30000f6)

| Addr | Instruction | Enc CU | Data CU |
|---|---|---|---|
| e4 | beq s1,s0 (not taken) | 4 | |
| e8 | li a1,0 | 2 | |
| ea | li a0,0 | 2 | |
| ec | sd a2,8(sp) | 2 | 8 |
| ee | jal ex07_invoke_callee | 4 | |
| f2 | ld a2,8(sp) | 2 | 8 |
| f4 | addiw s1,s1,1 | 2 | |
| f6 | j 0xe4 | 2 | |
| **subtotal** | 8 instr | **20** | **16** |

## Caller: ex07_invoke_callee (0x300016a–0x300018e, success path)

| Addr | Instruction | Enc CU | Data CU |
|---|---|---|---|
| 16a | addi sp,-48 | 2 | |
| 16c | sw a0,16(sp) | 2 | 4 |
| 16e | sw a1,20(sp) | 2 | 4 |
| 170 | addi a4,sp,8 | 2 | |
| 172 | li a3,0 | 2 | |
| 174 | li a1,10 | 2 | |
| 176 | addi a0,sp,16 | 2 | |
| 178 | sd ra,40(sp) | 2 | 8 |
| 17a | sh a2,24(sp) | 4 | 2 |
| 17e | sd zero,8(sp) | 2 | 8 |
| 180 | jal tsys_invoke | 4 | |
| 184 | bnez a0 (not taken) | 2 | |
| 186 | ld a0,8(sp) | 2 | 8 |
| 188 | bnez a0 (not taken) | 2 | |
| 18a | ld ra,40(sp) | 2 | 8 |
| 18c | addi sp,48 | 2 | |
| 18e | ret | 2 | |
| **subtotal** | 17 instr | **38** | **42** |

## Caller: tsys_invoke wrapper (0x3000222–, auth = NULL path)

Prologue (addi + 6×sd + 5×mv + beqz-taken): 2 + 6×2 + 5×2 + 2 = 26 enc,
48 store. Post-jump at 0x30002ca (4×mv + li a7 + ecall + beqz-not-taken +
sd invoke_err + 6×ld + addi + ret): 8+2+4+4+4+12+2+2 = 38 enc, 8 store +
48 load.

| Component | Enc CU | Data CU |
|---|---|---|
| prologue (sd s2/s3/s5/s6/s7/ra, mv×5, beqz taken) | 26 | 48 |
| ecall block + err-store + epilogue (6×ld) | 38 | 56 |
| **subtotal** | 30 instr | **64** | **104** |

## Callee: entry stub (_start, identical to noop's)

31 instructions = 88 enc CU (counted in the 01 README's decomposition);
loads lbu(1) + lhu(2) + lhu(2) = 5; stores sd ra + sd s0 = 16.
**Subtotal: 88 + 21 = 109.**

## Callee: start op-0 path (0x3000060–0x30000c8)

| Addr | Instruction | Enc CU | Data CU |
|---|---|---|---|
| 60 | addi sp,-48 | 2 | |
| 62 | sd ra,40(sp) | 2 | 8 |
| 64 | li a5,10 | 2 | |
| 66 | bne (not taken) | 4 | |
| 6a–76 | lbu ×4 (op field) | 16 | 4 |
| 7a–84 | slli/or ×6 | 12 | |
| 86 | sext.w | 4 | |
| 8a | beqz (taken, op=0) | 2 | |
| c6 | li a0,0 | 2 | |
| c8 | jal tsdk_return | 4 | |
| **subtotal** | 18 instr | **50** | **12** |

## Callee: tsdk_return + tsys_exit

addi + li + sd ra + jal = 10 enc + 8 store; li a7 + ecall = 6 enc.
**Subtotal: 16 + 8 = 24.**

## Total

| | CU |
|---|---|
| Syscall bases: invoke 512 + callee entry set-segment 512 + exit 0 | 1,024 |
| Caller instruction + data (20+16 + 38+42 + 64+104) | 284 |
| Callee instruction + data (109 + 50+12 + 24) | 195 |
| **Counted** | **1,503** |
| **Measured marginal hop (`cpi_n` slope, 3× at every N pair)** | **1,511** |
| **Residual** | **8** |

Verdict: the residual is 8 CU — one to two instructions of hand-count
noise, nowhere near the ~256 a register-save surcharge would leave.
`tsys_invoke` costs its plain 512 base; the spec's derivation of that base
(32 registers × 8 bytes × 2) stands, uncounted twice.
