/*
 * AVR disassembler
 *
 * Copyright (c) 2019-2020 Richard Henderson <rth@twiddle.net>
 * Copyright (c) 2019-2020 Michael Rolnik <mrolnik@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"

typedef struct {
    disassemble_info *info;
    uint32_t addr;
    uint32_t pc;
    uint8_t len;
    uint8_t bytes[8];
} DisasContext;

/* decoder helper */
static uint32_t decode_insn_load_bytes(DisasContext *ctx, uint32_t insn,
                           int i, int n)
{
    uint32_t addr = ctx->addr;

    g_assert(ctx->len == i);
    g_assert(n <= ARRAY_SIZE(ctx->bytes));

    while (++i <= n) {
        ctx->info->read_memory_func(addr++, &ctx->bytes[i - 1], 1, ctx->info);
        insn |= ctx->bytes[i - 1] << (32 - i * 8);
    }
    ctx->addr = addr;
    ctx->len = n;

    return insn;
}

/* Include the auto-generated decoder.  */
static uint32_t decode_insn_load(DisasContext *ctx);
static bool decode_insn(DisasContext *ctx, uint32_t insn);
#include "decode-insn.c.inc"

#define output(mnemonic, format, ...) \
    (pctx->info->fprintf_func(pctx->info->stream, "%-9s " format, \
                              mnemonic, ##__VA_ARGS__))

int nes6502_print_insn(bfd_vma addr, disassemble_info *info)
{
    DisasContext ctx;
    uint32_t insn;
    int i;

    ctx.info = info;
    ctx.pc = ctx.addr = addr;
    ctx.len = 0;

    insn = decode_insn_load(&ctx);
    if (!decode_insn(&ctx, insn)) {
        ctx.info->fprintf_func(ctx.info->stream, ".byte\t");
        for (i = 0; i < ctx.addr - addr; i++) {
            if (i > 0) {
                ctx.info->fprintf_func(ctx.info->stream, ",");
            }
            ctx.info->fprintf_func(ctx.info->stream, "0x%02x", insn >> 24);
            insn <<= 8;
        }
    }
    return ctx.addr - addr;
}


#define INSN(opcode, format, ...)                                       \
static bool trans_##opcode(DisasContext *pctx, arg_##opcode * a)        \
{                                                                       \
    output(#opcode, format, ##__VA_ARGS__);                             \
    return true;                                                        \
}

#define INSN_MNEMONIC(opcode, mnemonic, format, ...)                    \
static bool trans_##opcode(DisasContext *pctx, arg_##opcode * a)        \
{                                                                       \
    output(mnemonic, format, ##__VA_ARGS__);                            \
    return true;                                                        \
}

/*
 *   C       Z       N       V       S       H       T       I
 *   0       1       2       3       4       5       6       7
 */

/*
 * Arithmetic Instructions
 */
INSN(CMP,    "%d", a->imm8)
INSN(CMP_ZEROPAGE,    "%d", a->imm8)
INSN(CMP_ZEROPAGEX,   "%d", a->imm8)
INSN(CMP_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(CMP_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)
INSN(CMP_ABSOLUTEY,  "%d, %d", a->addr1, a->addr2)
INSN(CMP_INDIRECTX,  "%d", a->imm8)
INSN(CMP_INDIRECTY,  "%d", a->imm8)
INSN(DEX,    "")
INSN(DEY,    "")
INSN(ANDIM,         "%d", a->imm8)
INSN(AND_ZERPAGE,    "%d", a->imm8)
INSN(AND_ZERPAGEX,   "%d", a->imm8)
INSN(AND_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(AND_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)
INSN(AND_ABSOLUTEY,  "%d, %d", a->addr1, a->addr2)
INSN(AND_INDIRECTX,  "%d", a->imm8)
INSN(AND_INDIRECTY,  "%d", a->imm8)
INSN(ADCIM,         "%d", a->imm8)
INSN(ADC_ZEROPAGE,    "%d", a->imm8)
INSN(ADC_ZEROPAGEX,   "%d", a->imm8)
INSN(ADC_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(ADC_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)
INSN(ADC_ABSOLUTEY,  "%d, %d", a->addr1, a->addr2)
INSN(ADC_INDIRECTX,  "%d", a->imm8)
INSN(ADC_INDIRECTY,  "%d", a->imm8)
INSN(CPYIM,         "%d", a->imm8)
INSN(CPY_ZEROPAGE,  "%d", a->imm8)
INSN(CPY_ABSOLUTE,  "%d, %d", a->addr1, a->addr2)
INSN(DEC_ZEROPAGE,  "%d", a->imm8)
INSN(DEC_ZEROPAGEX,  "%d", a->imm8)
INSN(DEC_ABSOLUTE,  "%d, %d", a->addr1, a->addr2)
INSN(DEC_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)
INSN(INY,           "")
INSN(INX,           "")
INSN(CPXIM,  "%d", a->imm8)
INSN(CPX_ZEROPAGE,  "%d", a->imm8)
INSN(CPX_ABSOLUTE,  "%d, %d", a->addr1, a->addr2)
INSN(INC_ZERPAGE,    "%d", a->imm8)
INSN(INC_ZERPAGEX,   "%d", a->imm8)
INSN(INC_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(INC_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)

/*
 * Branch Instructions
 */
INSN(BPL,    "%d", a->imm8)
INSN(JSR,    "%d, %d", a->addr1, a->addr2)
INSN(BMI,    "%d", a->imm8)
INSN(RTI,    "")
INSN(JMP_ABSOLUTE,    "%d, %d", a->addr1, a->addr2)
INSN(JMP_INDIRECT,    "%d, %d", a->addr1, a->addr2)
INSN(BVC,    "%d", a->imm8)
INSN(RTS,    "")
INSN(BVS,    "%d", a->imm8)
INSN(BCC,    "%d", a->imm8)
INSN(BCS,    "%d", a->imm8)
INSN(BNE,    "%d", a->imm8)
INSN(BEQ,    "%d", a->imm8)


/*
 * Data Transfer Instructions
 */
INSN(LDAIM,             "%d", a->imm)
INSN(LDA_ZEROPAGE,       "%d", a->imm8)
INSN(LDA_ZEROPAGEX,      "%d", a->imm8)
INSN(LDAAB,             "%d, %d", a->addr1, a->addr2)
INSN(LDA_ABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(LDA_ABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(LDA_INDIRECTX,      "%d", a->imm8)
INSN(LDA_INDIRECTY,      "%d", a->imm8)
INSN(LDXIM,             "%d", a->imm)
INSN(LDX_ZEROPAGE,       "%d", a->imm8)
INSN(LDX_ZEROPAGEY,      "%d", a->imm8)
INSN(LDX_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(LDX_ABSOLUTEY,     "%d, %d", a->addr1, a->addr2)
INSN(LDYIM,             "%d", a->imm8)
INSN(LDY_ZEROPAGE,       "%d", a->imm8)
INSN(LDY_ZEROPAGEX,      "%d", a->imm8)
INSN(LDY_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(LDY_ABSOLUTEX,     "%d, %d", a->addr1, a->addr2)
INSN(STA_ZEROPAGE,       "%d", a->imm8)
INSN(STA_ZEROPAGEX,      "%d", a->imm8)
INSN(STAAB,             "%d, %d", a->addr1, a->addr2)
INSN(STA_ABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(STA_ABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(STA_INDIRECTX,      "%d", a->imm8)
INSN(STA_INDIRECTY,      "%d", a->imm8)
INSN(ORA_IM,             "%d", a->imm8)
INSN(ORA_ZEROPAGE,       "%d", a->imm8)
INSN(ORA_ZEROPAGEX,      "%d", a->imm8)
INSN(ORA_ABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(ORA_ABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(ORA_ABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(ORA_INDIRECTX,      "%d", a->imm8)
INSN(ORA_INDIRECTY,      "%d", a->imm8)
INSN(PHP,               "")
INSN(PLP,               "")
INSN(SEC,               "")
INSN(EORIM,             "%d", a->imm8)
INSN(EOR_ZEROPAGE,       "%d", a->imm8)
INSN(EOR_ZEROPAGEX,      "%d", a->imm8)
INSN(EOR_ABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(EOR_ABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(EOR_ABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(EOR_INDIRECTX,      "%d", a->imm8)
INSN(EOR_INDIRECTY,      "%d", a->imm8)
INSN(PUSHA,             "")
INSN(PLA,               "")
INSN(STY_ZEROPAGE,       "%d", a->imm8)
INSN(STY_ZEROPAGEX,      "%d", a->imm8)
INSN(STY_ABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(STX_ZEROPAGE,       "%d", a->imm8)
INSN(STX_ZEROPAGEX,      "%d", a->imm8)
INSN(STX_ABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(TXA,               "")
INSN(TYA,               "")
INSN(TAY,               "")
INSN(TAX,               "")
INSN(TSX,               "")
INSN(SBCIM,             "%d", a->imm8)
INSN(SBC_ZEROPAGE,       "%d", a->imm8)
INSN(SBC_ZEROPAGEX,      "%d", a->imm8)
INSN(SBC_ABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(SBC_ABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(SBC_ABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(SBC_INDIRECTX,      "%d", a->imm8)
INSN(SBC_INDIRECTY,      "%d", a->imm8)

/*
 * Bit and Bit-test Instructions
 */
INSN(ASL,           "")
INSN(ASL_ZEROPAGE,   "%d", a->imm8)
INSN(ASL_ZEROPAGEX,  "%d", a->imm8)
INSN(ASL_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(ASL_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)
INSN(CLC,           "")
INSN(BIT_ZEROPAGE,   "%d", a->imm8)
INSN(BIT_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(ROLA,          "")
INSN(ROL_ZEROPAGE,   "%d", a->imm8)
INSN(ROL_ZEROPAGEX,  "%d", a->imm8)
INSN(ROL_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(ROL_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)
INSN(LSRA,          "")
INSN(LSR_ZEROPAGE,   "%d", a->imm8)
INSN(LSR_ZEROPAGEX,  "%d", a->imm8)
INSN(LSR_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(LSR_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)
INSN(CLI,          "")
INSN(RORA,          "")
INSN(ROR_ZEROPAGE,   "%d", a->imm8)
INSN(ROR_ZEROPAGEX,  "%d", a->imm8)
INSN(ROR_ABSOLUTE,   "%d, %d", a->addr1, a->addr2)
INSN(ROR_ABSOLUTEX,  "%d, %d", a->addr1, a->addr2)
INSN(CLV,          "")
INSN(SED,          "")

/*
 * MCU Control Instructions
 */
INSN(NOP,    "")
INSN(SEI,    "")
INSN(CLD,    "")
INSN(TXS,    "")
INSN(BRK,    "")

/*
 * Extended Instruction Set
 */
INSN(ASOZEROPAGE,       "%d", a->imm8)
INSN(ASOZEROPAGEX,      "%d", a->imm8)
INSN(ASOABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(ASOABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(ASOABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(ASOINDIRECTX,      "%d", a->imm8)
INSN(ASOINDIRECTY,      "%d", a->imm8)

INSN(RLAZEROPAGE,       "%d", a->imm8)
INSN(RLAZEROPAGEX,      "%d", a->imm8)
INSN(RLAABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(RLAABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(RLAABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(RLAINDIRECTX,      "%d", a->imm8)
INSN(RLAINDIRECTY,      "%d", a->imm8)

INSN(LSEZEROPAGE,       "%d", a->imm8)
INSN(LSEZEROPAGEX,      "%d", a->imm8)
INSN(LSEABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(LSEABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(LSEABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(LSEINDIRECTX,      "%d", a->imm8)
INSN(LSEINDIRECTY,      "%d", a->imm8)

INSN(RRAZEROPAGE,       "%d", a->imm8)
INSN(RRAZEROPAGEX,      "%d", a->imm8)
INSN(RRAABSOLUTE,       "%d, %d", a->addr1, a->addr2)
INSN(RRAABSOLUTEX,      "%d, %d", a->addr1, a->addr2)
INSN(RRAABSOLUTEY,      "%d, %d", a->addr1, a->addr2)
INSN(RRAINDIRECTX,      "%d", a->imm8)
INSN(RRAINDIRECTY,      "%d", a->imm8)
INSN(INS_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)

/*
 * Extended Instruction Set
 */
INSN(NOP_04_ZEROPAGE,      "%d", a->imm8)
INSN(NOP_44_ZEROPAGE,      "%d", a->imm8)
INSN(NOP_64_ZEROPAGE,      "%d", a->imm8)

INSN(NOP_0C_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(NOP_1C_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(NOP_3C_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(NOP_5C_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(NOP_7C_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(NOP_DC_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(NOP_FC_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)

INSN(NOP_80_IM,      "%d", a->imm8)
INSN(NOP_82_IM,      "%d", a->imm8)
INSN(NOP_89_IM,      "%d", a->imm8)
INSN(NOP_C2_IM,      "%d", a->imm8)
INSN(NOP_E2_IM,      "%d", a->imm8)
