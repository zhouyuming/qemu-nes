#include "qemu/osdep.h"
#include "qemu/qemu-print.h"
#include "tcg/tcg.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "tcg/tcg-op.h"
#include "exec/cpu_ldst.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"
#include "exec/log.h"
#include "exec/translator.h"
#include "exec/gen-icount.h"
#include "exec/address-spaces.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
/*
 *  Define if you want a BREAK instruction translated to a breakpoint
 *  Active debugging connection is assumed
 *  This is for
 *  https://github.com/seharris/qemu-avr-tests/tree/master/instruction-tests
 *  tests
 */
#undef BREAKPOINT_ON_BREAK

static TCGv cpu_pc;

static TCGv cpu_A;
static TCGv cpu_X;
static TCGv cpu_Y;

static TCGv cpu_Cf;
static TCGv cpu_Zf;
static TCGv cpu_Nf;
static TCGv cpu_Vf;
static TCGv cpu_Sf;
static TCGv cpu_Hf;
static TCGv cpu_Tf;
static TCGv cpu_If;

static TCGv cpu_carry_flag;
static TCGv cpu_zero_flag;
static TCGv cpu_interrupt_flag;
static TCGv cpu_decimal_flag;
static TCGv cpu_break_flag;
static TCGv cpu_unused_flag;
static TCGv cpu_overflow_flag;
static TCGv cpu_negative_flag;
static TCGv cpu_stack_point;
static TCGv cpu_P;

static TCGv op_address;
static TCGv op_value;

// static int not_tcg_op_address;
// static int not_tcg_op_value;

static TCGv cpu_rampD;
static TCGv cpu_rampX;
static TCGv cpu_rampY;
static TCGv cpu_rampZ;

static TCGv cpu_eind;

static TCGv cpu_skip;


#define DISAS_EXIT   DISAS_TARGET_0  /* We want return to the cpu main loop.  */
#define DISAS_LOOKUP DISAS_TARGET_1  /* We have a variable condition exit.  */
#define DISAS_CHAIN  DISAS_TARGET_2  /* We have a single condition exit.  */

typedef struct DisasContext DisasContext;

/* This is the state at translation time. */
struct DisasContext {
    DisasContextBase base;

    CPUNES6502State *env;
    CPUState *cs;

    target_long npc;
    uint32_t opcode;

    /* Routine used to access memory */
    int memidx;
    
    TCGv skip_var0;
    TCGv skip_var1;
    TCGCond skip_cond;
};
static FILE *log_p;
void nes6502_cpu_tcg_init(void)
{
#define NES_REG_OFFS(x) offsetof(CPUNES6502State, x)
    cpu_pc = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(pc_w), "pc");
    cpu_Cf = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(sregC), "Cf");
    cpu_Zf = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(sregZ), "Zf");
    cpu_Nf = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(sregN), "Nf");
    cpu_Vf = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(sregV), "Vf");
    cpu_Sf = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(sregS), "Sf");
    cpu_Hf = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(sregH), "Hf");
    cpu_Tf = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(sregT), "Tf");
    cpu_If = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(sregI), "If");
    cpu_rampD = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(rampD), "rampD");
    cpu_rampX = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(rampX), "rampX");
    cpu_rampY = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(rampY), "rampY");
    cpu_rampZ = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(rampZ), "rampZ");
    cpu_eind = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(eind), "eind");
    cpu_skip = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(skip), "skip");

    cpu_A = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(reg_A), "reg_A");
    cpu_X = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(reg_X), "reg_X");
    cpu_Y = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(reg_Y), "reg_Y");
    
    cpu_carry_flag = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(carry_flag), "carry_flag");
    cpu_zero_flag = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(zero_flag), "zero_flag");
    cpu_interrupt_flag = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(interrupt_flag), "interrupt_flag");
    cpu_decimal_flag = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(decimal_flag), "decimal_flag");
    cpu_break_flag = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(break_flag), "break_flag");
    cpu_unused_flag = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(unused_flag), "unused_flag");
    cpu_overflow_flag = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(overflow_flag), "overflow_flag");
    cpu_negative_flag = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(negative_flag), "negative_flag");
    cpu_stack_point = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(stack_point), "stack_point");
    cpu_P = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(p), "p");

    op_address = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(op_address), "op_address");
    op_value = tcg_global_mem_new_i32(cpu_env, NES_REG_OFFS(op_value), "op_value");

#undef NES_REG_OFFS

    log_p = fopen("log.txt", "w+");
    if (log_p == NULL)
    {
        fprintf(stderr, "Open log file failed.\n");
        exit(1);
    }
}

/* decoder helper */
static uint32_t decode_insn_load_bytes(DisasContext *ctx, uint32_t insn,
                           int i, int n)
{
    while (++i <= n) {
        // uint8_t b = cpu_ldub_code(ctx->env, ctx->npc++);
        // insn |= b << (16 - i * 8);

        uint8_t b = cpu_ldub_code(ctx->env, ctx->base.pc_next++);
        insn |= b << (32 - i * 8);
    }
    return insn;
}

static uint32_t decode_insn_load(DisasContext *ctx);
static bool decode_insn(DisasContext *ctx, uint32_t insn);
#include "decode-insn.c.inc"

// static void nes6502_cpu_dump_state(CPUState *cs, FILE *f, int flags)
// {
//     NES6502CPU *cpu = NES6502_CPU(cs);
//     CPUNES6502State *env = &cpu->env;

//     // psw = rx_cpu_pack_psw(env);
//     qemu_fprintf(f, "negative_flag=0x%08x\n", env->negative_flag);
//     // for (i = 0; i < 16; i += 4) {
//     //     qemu_fprintf(f, "r%d=0x%08x r%d=0x%08x r%d=0x%08x r%d=0x%08x\n",
//     //                  i, env->regs[i], i + 1, env->regs[i + 1],
//     //                  i + 2, env->regs[i + 2], i + 3, env->regs[i + 3]);
//     // }
// }

static void cpu_update_zn_flags(TCGv R)
{
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, R, 0); /* Zf = R == 0 */
    // tcg_gen_shri_tl(cpu_negative_flag, R, 7); /* Cf = t1(7) */
    tcg_gen_setcondi_tl(TCG_COND_GTU, cpu_negative_flag, R, 127); /* 大于127 */
    
}

static void cpu_compare(TCGv r, TCGv val)
{
    // tcg_gen_setcond_tl(TCG_COND_GE, cpu_carry_flag, r, val);
    // tcg_gen_setcond_tl(TCG_COND_EQ, cpu_zero_flag, r, val);
    // tcg_gen_setcond_tl(TCG_COND_LT, cpu_negative_flag, r, val);

    tcg_gen_sub_tl(val, r, val);
    tcg_gen_setcondi_i32(TCG_COND_GE, cpu_carry_flag, val, 0);
    tcg_gen_setcondi_i32(TCG_COND_EQ, cpu_zero_flag, val, 0);
    
    tcg_gen_shri_tl(val, val, 7);
    tcg_gen_andi_tl(val, val, 0x01);
    tcg_gen_setcondi_i32(TCG_COND_EQ, cpu_negative_flag, val, 1);
    // gen_helper_print_p(cpu_env, cpu_P);
    // gen_helper_print_carry_flag(cpu_env, cpu_carry_flag);
    // gen_helper_print_zero_flag(cpu_env, cpu_zero_flag);
    // gen_helper_print_negative_flag(cpu_env, cpu_negative_flag);
    // gen_helper_print_A(cpu_env, r);
    // gen_helper_print_op_value(cpu_env, op_value);

}

static void cpu_pack_p(void)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_andi_tl(tmp, cpu_P, 0x01);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, tmp, 1);

    tcg_gen_andi_tl(tmp, cpu_P, 0x02);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, tmp, 0x2);

    tcg_gen_andi_tl(tmp, cpu_P, 0x04);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_interrupt_flag, tmp, 0x4);

    tcg_gen_andi_tl(tmp, cpu_P, 0x08);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_decimal_flag, tmp, 0x8);

    tcg_gen_andi_tl(tmp, cpu_P, 0x10);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_break_flag, tmp, 0x10);

    tcg_gen_andi_tl(tmp, cpu_P, 0x20);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_unused_flag, tmp, 0x20);

    tcg_gen_andi_tl(tmp, cpu_P, 0x40);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_overflow_flag, tmp, 0x40);

    tcg_gen_andi_tl(tmp, cpu_P, 0x80);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_negative_flag, tmp, 0x80);
}

static void cpu_stack_pushb_tcg(TCGv addr, TCGv tmp)
{
    tcg_gen_qemu_st_tl(tmp, addr, 0, MO_UB);
    tcg_gen_subi_tl(cpu_stack_point, cpu_stack_point, 1);
    tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);
}

static void cpu_stack_pushb(uint8_t data)
{
    TCGv tmp;
    tmp = tcg_temp_new();
    tcg_gen_movi_tl(tmp, data);

    TCGv addr = tcg_temp_new();
    // tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);
    tcg_gen_addi_tl(addr, cpu_stack_point, 0x100);
    // tcg_gen_andi_tl(addr, addr, 0x7FF);
    cpu_stack_pushb_tcg(addr, tmp);
}

static void cpu_stack_pushb_a(TCGv tmp)
{
    TCGv addr = tcg_temp_new();
    // tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);
    tcg_gen_addi_tl(addr, cpu_stack_point, 0x100);
    tcg_gen_andi_tl(addr, addr, 0x7FF);
    cpu_stack_pushb_tcg(addr, tmp);
}

// static void cpu_stack_pushw_pushb(TCGv addr, uint8_t data)
// {
//     TCGv tmp;
//     tmp = tcg_temp_new();
//     tcg_gen_movi_tl(tmp, data);
//     cpu_stack_pushb_tcg(addr, tmp);
// }

static void cpu_stack_pushw(uint16_t data)
{
    TCGv addr = tcg_temp_new();
    // tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);
    tcg_gen_addi_tl(addr, cpu_stack_point, 0xFF);
    // tcg_gen_andi_tl(addr, addr, 0xFFFF);

    TCGv tmp = tcg_temp_new();
    tcg_gen_movi_tl(tmp, data&0x00ff);
    tcg_gen_qemu_st_tl(tmp, addr, 0, MO_UB);

    tcg_gen_addi_tl(addr, addr, 1);
    // tcg_gen_andi_tl(addr, addr, 0x7FF);
    tcg_gen_movi_tl(tmp, data>>8);
    tcg_gen_qemu_st_tl(tmp, addr, 0, MO_UB);

    tcg_gen_subi_tl(cpu_stack_point, cpu_stack_point, 2);
    tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);
}

static void cpu_stack_popb(TCGv ret)
{
    tcg_gen_addi_tl(cpu_stack_point, cpu_stack_point, 1);
    // tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);
    TCGv addr = tcg_temp_new();
    tcg_gen_addi_tl(addr, cpu_stack_point, 0x100);
    tcg_gen_andi_tl(addr, addr, 0x7FF);
    tcg_gen_qemu_ld_tl(ret, addr, 0, MO_UB);
}

static void cpu_stack_popw(TCGv ret)
{
    tcg_gen_addi_tl(cpu_stack_point, cpu_stack_point, 2);
    tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);

    TCGv addr = tcg_temp_new();
    tcg_gen_addi_tl(addr, cpu_stack_point, 0xFF);
    // tcg_gen_andi_tl(addr, addr, 0x7FF);

    TCGv v1 = tcg_temp_new();
    tcg_gen_qemu_ld_tl(v1, addr, 0, MO_UB);

    TCGv v2 = tcg_temp_new();
    tcg_gen_addi_tl(addr, addr, 1);
    // tcg_gen_andi_tl(addr, addr, 0x7FF);
    tcg_gen_qemu_ld_tl(v2, addr, 0, MO_UB);
    tcg_gen_shli_tl(v2, v2, 8);

    tcg_gen_add_tl(ret, v1, v2);

    // tcg_gen_qemu_ld_tl(ret, addr, 0, MO_LEUW);
}

static void cpu_address_zero_page(int pc)
{
    tcg_gen_movi_tl(op_address, pc);
    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_zero_page_x(int pc)
{
    TCGv pc_value = tcg_temp_new_i32();
    tcg_gen_movi_tl(pc_value, pc);

    tcg_gen_add_tl(pc_value, pc_value, cpu_X);
    tcg_gen_andi_tl(op_address, pc_value, 0xFF);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_zero_page_y(int pc)
{
    TCGv pc_value = tcg_temp_new_i32();
    tcg_gen_movi_tl(pc_value, pc);
    tcg_gen_add_tl(pc_value, pc_value, cpu_Y);
    tcg_gen_andi_tl(op_address, pc_value, 0xFF);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static uint16_t cpu_address_absolute(int pc1, int pc2)
{
    uint16_t addr;
    addr = (pc1 | (pc2 << 8));

    tcg_gen_movi_tl(op_address, addr);
    // uint8_t v = address_space_ldub(&address_space_memory, addr,
    //                             MEMTXATTRS_UNSPECIFIED, NULL);
    // printf("cpu_address_absolute v 0x%x\n", v);
    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
    return addr;
}

static void cpu_address_absolute_x(int pc1, int pc2)
{
    uint16_t addr;
    addr = (pc1 | (pc2 << 8));

    tcg_gen_movi_tl(op_address, addr);
    tcg_gen_add_tl(op_address, op_address, cpu_X);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_absolute_y(int pc1, int pc2)
{
    uint16_t addr;
    addr = (pc1 | (pc2 << 8));

    tcg_gen_movi_tl(op_address, addr);
    tcg_gen_add_tl(op_address, op_address, cpu_Y);
    tcg_gen_andi_tl(op_address, op_address, 0xFFFF);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_indirect(int pc1, int pc2)
{
    uint16_t arg_addr;
    arg_addr = (pc1 | (pc2 << 8));

    // The famous 6502 bug when instead of reading from $C0FF/$C100 it reads from $C0FF/$C000
    if ((arg_addr & 0xFF) == 0xFF) {
        // Buggy code
        TCGv t_1 = tcg_temp_new();
        tcg_gen_movi_tl(t_1, arg_addr & 0xFF00);
        tcg_gen_shli_tl(t_1, t_1, 8);

        TCGv value_1 = tcg_temp_new_i32();  
        tcg_gen_qemu_ld_tl(value_1, t_1, 0, MO_UB);

        TCGv value_2 = tcg_temp_new_i32();  
        TCGv t_2 = tcg_temp_new();
        tcg_gen_movi_tl(t_2, arg_addr);
        tcg_gen_qemu_ld_tl(value_2, t_2, 0, MO_UB);

        tcg_gen_add_tl(op_address, value_1, value_2);
    }
    else {
        // Normal code
        tcg_gen_movi_tl(op_address, arg_addr);
        TCGv value_1 = tcg_temp_new_i32();  
        tcg_gen_qemu_ld_tl(value_1, op_address, 0, MO_UB);

        TCGv value_2 = tcg_temp_new_i32();
        tcg_gen_addi_tl(op_address, op_address, 1);
        tcg_gen_qemu_ld_tl(value_2, op_address, 0, MO_UB);

        tcg_gen_shli_tl(value_2, value_2, 8);

        tcg_gen_add_tl(op_address, value_1, value_2);

        // tcg_gen_qemu_ld_tl(op_address, op_address, 0, MO_LEUW);
    }
}

static void cpu_address_indirect_x(int arg_addr)
{
    TCGv addr = tcg_temp_new_i32();   
    tcg_gen_movi_tl(addr, arg_addr);

    TCGv op_address_1 = tcg_temp_new_i32();
    tcg_gen_addi_tl(op_address_1, addr, 1);
    tcg_gen_add_tl(op_address_1, op_address_1, cpu_X);

    TCGv value_h = tcg_temp_new_i32();   
    tcg_gen_andi_tl(op_address_1, op_address_1, 0x00FF);
    tcg_gen_qemu_ld_tl(value_h, op_address_1, 0, MO_UB);
    tcg_gen_shli_tl(value_h, value_h, 8);

    tcg_gen_add_tl(op_address_1, addr, cpu_X);
    tcg_gen_andi_tl(op_address_1, op_address_1, 0x00FF);
    TCGv value_l = tcg_temp_new_i32(); 
    tcg_gen_qemu_ld_tl(value_l, op_address_1, 0, MO_UB);

    tcg_gen_or_tl(op_address, value_h, value_l);
    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_indirect_y(int arg_addr)
{
    TCGv addr = tcg_temp_new_i32();   
    tcg_gen_movi_tl(addr, arg_addr);

    TCGv op_address_1 = tcg_temp_new_i32();
    tcg_gen_addi_tl(op_address_1, addr, 1);
    tcg_gen_andi_tl(op_address_1, op_address_1, 0x00FF);

    TCGv value_h = tcg_temp_new_i32();
    tcg_gen_qemu_ld_tl(value_h, op_address_1, 0, MO_UB);
    tcg_gen_shli_tl(value_h, value_h, 8);

    TCGv value_l = tcg_temp_new_i32(); 
    tcg_gen_qemu_ld_tl(value_l, addr, 0, MO_UB);

    tcg_gen_or_tl(op_address, value_h, value_l);

    tcg_gen_add_tl(op_address, op_address, cpu_Y);
    tcg_gen_andi_tl(op_address, op_address, 0x00FFFF);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static int cpu_address_relative(int addr, int pc)
{
    if (addr & 0x80)
        addr -= 0x100;

    addr += pc;
    // printf("cpu_address_relative 0x%x\n", addr);
    tcg_gen_movi_tl(op_address, addr);
    return addr;
}

static void cpu_address_immediate(int pc)
{
    tcg_gen_movi_tl(op_value, pc);
}

/*
 * Arithmetic Instructions
 */

static bool trans_CMP(DisasContext *ctx, arg_CMP *a) // 0xc9
{
    cpu_address_immediate(a->imm8);
    cpu_compare(cpu_A, op_value);
    return true;   
    
    // tcg_gen_setcondi_tl(TCG_COND_GE, cpu_carry_flag, cpu_A, a->imm8);
    // tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, cpu_A, a->imm8);
    // tcg_gen_setcondi_tl(TCG_COND_LT, cpu_negative_flag, cpu_A, a->imm8);

    // TCGv t1 = tcg_temp_new();
    // tcg_gen_subi_tl(t1, cpu_A, a->imm8);
    // tcg_gen_shri_tl(t1, t1, 7);
    // tcg_gen_andi_tl(t1, t1, 0x01);
    // tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_negative_flag, t1, 1);
    return true;
}

static bool trans_CMP_ZEROPAGE(DisasContext *ctx, arg_CMP_ZEROPAGE *a) // 0xc5
{
    // printf("trans_CMP_ZEROPAGE\n");
    // gen_helper_print_sp(cpu_env, cpu_stack_point);
    cpu_address_zero_page(a->imm8);
    cpu_compare(cpu_A, op_value);
    return true;
}

static bool trans_CMP_ZEROPAGEX(DisasContext *ctx, arg_CMP_ZEROPAGEX *a) // 0xd5
{
    cpu_address_zero_page_x(a->imm8);
    cpu_compare(cpu_A, op_value);
    return true;
}

static bool trans_CMP_ABSOLUTE(DisasContext *ctx, arg_CMP_ABSOLUTE *a) // 0xcd
{
    cpu_address_absolute(a->addr1, a->addr2);
    cpu_compare(cpu_A, op_value);
    return true;
}

static bool trans_CMP_ABSOLUTEX(DisasContext *ctx, arg_CMP_ABSOLUTEX *a) // 0xdd
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    cpu_compare(cpu_A, op_value);
    return true;
}

static bool trans_CMP_ABSOLUTEY(DisasContext *ctx, arg_CMP_ABSOLUTEY *a) // 0xd9
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    cpu_compare(cpu_A, op_value);
    return true;
}

static bool trans_CMP_INDIRECTX(DisasContext *ctx, arg_CMP_INDIRECTX *a) // 0xc1
{
    cpu_address_indirect_x(a->imm8);
    cpu_compare(cpu_A, op_value);
    return true;
}

static bool trans_CMP_INDIRECTY(DisasContext *ctx, arg_CMP_INDIRECTY *a) // 0xd1
{
    cpu_address_indirect_y(a->imm8);
    cpu_compare(cpu_A, op_value);
    return true;
}

static void dec_r_common(TCGv r)
{
    TCGLabel *t, *done;
    t = gen_new_label();
    done = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, r, 0, t);

    tcg_gen_subi_tl(r, r, 1);
    tcg_gen_andi_tl(r, r, 0xFF);
    // tcg_gen_movi_tl(cpu_negative_flag, 0);
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_shri_tl(tmp, r, 7);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_negative_flag, tmp, 1);
    tcg_gen_br(done);

    gen_set_label(t);
    tcg_gen_movi_tl(r, 0xff);
    tcg_gen_movi_tl(cpu_negative_flag, 1);
    gen_set_label(done);
}

static bool trans_DEX(DisasContext *ctx, arg_DEX *a) // 0xca
{
    // TCGv r = cpu_X;
    dec_r_common(cpu_X);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, cpu_X, 0);
    // gen_helper_print_x(cpu_env, cpu_X);
    // gen_helper_print_zero_flag(cpu_env, cpu_zero_flag);
    return true;
}

// Decrement Index Y by One
static bool trans_DEY(DisasContext *ctx, arg_DEY *a) // 0x88
{
    // TCGv r = cpu_Y;
    dec_r_common(cpu_Y);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, cpu_Y, 0);
    // tcg_gen_movi_tl(cpu_zero_flag, 1);

    return true;
}

static bool trans_CPXIM(DisasContext *ctx, arg_CPXIM *a) // 0xe0
{
    TCGv val = tcg_temp_new_i32();
    tcg_gen_movi_tl(val, a->imm8);
    cpu_compare(cpu_X, val);
    return true;
}

static bool trans_CPX_ZEROPAGE(DisasContext *ctx, arg_CPX_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm8);
    cpu_compare(cpu_X, op_value);
    return true;
}

static bool trans_CPX_ABSOLUTE(DisasContext *ctx, arg_CPX_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr1, a->addr2);
    cpu_compare(cpu_X, op_value);
    return true;
}

static bool trans_ANDIM(DisasContext *ctx, arg_ANDIM *a) // 0x29
{
    tcg_gen_andi_tl(cpu_A, cpu_A, a->imm8);
    TCGv val = cpu_A;
    cpu_update_zn_flags(val);
    return true;
}

static void and_common(void)
{
    tcg_gen_and_tl(cpu_A, cpu_A, op_value);
    TCGv val = cpu_A;
    cpu_update_zn_flags(val);
}

static bool trans_AND_ZERPAGE(DisasContext *ctx, arg_AND_ZERPAGE *a) // 0x25
{
    cpu_address_zero_page(a->imm8);
    and_common();
    return true;
}

static bool trans_AND_ZERPAGEX(DisasContext *ctx, arg_AND_ZERPAGEX *a) // 0x35
{
    cpu_address_zero_page_x(a->imm8);
    and_common();
    return true;
}

static bool trans_AND_ABSOLUTE(DisasContext *ctx, arg_AND_ABSOLUTE *a) // 0x2d
{
    cpu_address_absolute(a->addr1, a->addr2);
    and_common();
    return true;
}

static bool trans_AND_ABSOLUTEX(DisasContext *ctx, arg_AND_ABSOLUTEX *a) // 0x3d
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    and_common();
    return true;
}

static bool trans_AND_ABSOLUTEY(DisasContext *ctx, arg_AND_ABSOLUTEY *a) // 0x39
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    and_common();
    return true;
}

static bool trans_AND_INDIRECTX(DisasContext *ctx, arg_AND_INDIRECTX *a) // 0x21
{
    cpu_address_indirect_x(a->imm8);
    and_common();
    return true;
}

static bool trans_AND_INDIRECTY(DisasContext *ctx, arg_AND_INDIRECTY *a) // 0x31
{
    cpu_address_indirect_y(a->imm8);
    and_common();
    return true;
}

static void adc_common(TCGv val)
{
    TCGv res = tcg_temp_new_i32();
    tcg_gen_add_tl(res, cpu_A, val);

    TCGv tmp_carry = tcg_temp_new_i32();
    tcg_gen_setcondi_tl(TCG_COND_EQ, tmp_carry, cpu_carry_flag, 1);

    tcg_gen_add_tl(res, res, tmp_carry);

    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_andi_tl(tmp, res, 0x100);
    tcg_gen_setcondi_tl(TCG_COND_NE, cpu_carry_flag, tmp, 0);

    tcg_gen_xor_tl(tmp, cpu_A, val);
    tcg_gen_not_tl(tmp, tmp);

    TCGv vv = tcg_temp_new_i32();
    tcg_gen_xor_tl(vv, cpu_A, res);

    TCGv t = tcg_temp_new_i32();
    tcg_gen_and_tl(t, vv, tmp);
    tcg_gen_andi_tl(t, t, 0x80);

    tcg_gen_not_tl(t, t);
    tcg_gen_not_tl(t, t);

    tcg_gen_setcondi_tl(TCG_COND_NE, cpu_overflow_flag, t, 0);

    tcg_gen_andi_tl(cpu_A, res, 0xFF);
    cpu_update_zn_flags(cpu_A);
}

static bool trans_ADCIM(DisasContext *ctx, arg_ADCIM *a) // 0x69
{
    TCGv vv = tcg_temp_new_i32();
    tcg_gen_movi_tl(vv, a->imm8);
    // printf("trans_ADCIM imm8 0x%x\n", a->imm8);
    adc_common(vv);
    return true;
}

static bool trans_ADC_ZEROPAGE(DisasContext *ctx, arg_ADC_ZEROPAGE *a) // 0x65
{
    cpu_address_zero_page(a->imm8);
    adc_common(op_value);
    return true;
}

static bool trans_ADC_ZEROPAGEX(DisasContext *ctx, arg_ADC_ZEROPAGEX *a) // 0x75
{
    cpu_address_zero_page_x(a->imm8);
    adc_common(op_value);
    return true;
}

static bool trans_ADC_ABSOLUTE(DisasContext *ctx, arg_ADC_ABSOLUTE *a) // 0x6d
{
    cpu_address_absolute(a->addr1, a->addr2);
    adc_common(op_value);
    return true;
}

static bool trans_ADC_ABSOLUTEX(DisasContext *ctx, arg_ADC_ABSOLUTEX *a) // 0x7d
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    adc_common(op_value);
    return true;
}

static bool trans_ADC_ABSOLUTEY(DisasContext *ctx, arg_ADC_ABSOLUTEY *a) // 0x79
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    adc_common(op_value);
    return true;
}

static bool trans_ADC_INDIRECTX(DisasContext *ctx, arg_ADC_INDIRECTX *a) // 0x61
{
    cpu_address_indirect_x(a->imm8);
    adc_common(op_value);
    return true;
}

static bool trans_ADC_INDIRECTY(DisasContext *ctx, arg_ADC_INDIRECTY *a) // 0x71
{
    cpu_address_indirect_y(a->imm8);
    adc_common(op_value);
    return true;
}

// Compare Memory and Index Y
static bool trans_CPYIM(DisasContext *ctx, arg_CPYIM *a) // 0xc0
{
    TCGv val = tcg_temp_new_i32();
    tcg_gen_movi_tl(val, a->imm8);
    cpu_compare(cpu_Y, val);

    return true;
}

static bool trans_CPY_ZEROPAGE(DisasContext *ctx, arg_CPY_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm8);
    cpu_compare(cpu_Y, op_value);
    return true;
}

static bool trans_CPY_ABSOLUTE(DisasContext *ctx, arg_CPY_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr1, a->addr2);
    cpu_compare(cpu_Y, op_value);
    return true;
}

static void inc_common(void)
{
    TCGv tmp = tcg_temp_new_i32();

    TCGLabel *t, *done;
    t = gen_new_label();
    done = gen_new_label();

    tcg_gen_brcondi_i32(TCG_COND_EQ, op_value, 0x00ff, t);
    tcg_gen_addi_tl(tmp, op_value, 1);
    cpu_update_zn_flags(tmp);
    tcg_gen_br(done);

    gen_set_label(t);
    tcg_gen_movi_tl(tmp, 0);
    tcg_gen_movi_tl(cpu_zero_flag, 1);
    tcg_gen_movi_tl(cpu_negative_flag, 0);
    gen_set_label(done);

    tcg_gen_qemu_st_tl(tmp, op_address, 0, MO_UB);
}

static bool trans_INC_ZERPAGE(DisasContext *ctx, arg_INC_ZERPAGE *a) // 0xe6
{
    cpu_address_zero_page(a->imm8);
    inc_common();
    return true;
}

static bool trans_INC_ZERPAGEX(DisasContext *ctx, arg_INC_ZERPAGEX *a) // 0xf6
{
    cpu_address_zero_page_x(a->imm8);
    inc_common();
    return true;
}

static bool trans_INC_ABSOLUTE(DisasContext *ctx, arg_INC_ABSOLUTE *a) // 0xee
{
    cpu_address_absolute(a->addr1, a->addr2);
    inc_common();
    return true;
}

static bool trans_INC_ABSOLUTEX(DisasContext *ctx, arg_INC_ABSOLUTEX *a) // 0xfe
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    inc_common();
    return true;
}

static void dec_common(void)
{
    TCGv res = tcg_temp_new();

    TCGLabel *t, *done;
    t = gen_new_label();
    done = gen_new_label();

    tcg_gen_brcondi_i32(TCG_COND_EQ, op_value, 0, t);
    tcg_gen_subi_tl(res, op_value, 1);
    tcg_gen_andi_tl(res, res, 0xFF);
    cpu_update_zn_flags(res);
    tcg_gen_br(done);

    gen_set_label(t);
    tcg_gen_movi_tl(res, 0xFF);
    tcg_gen_movi_tl(cpu_zero_flag, 0);
    tcg_gen_movi_tl(cpu_negative_flag, 1);
    gen_set_label(done);

    tcg_gen_qemu_st_tl(res, op_address, 0, MO_UB);
}

static bool trans_DEC_ZEROPAGE(DisasContext *ctx, arg_DEC_ZEROPAGE *a) // 0xc6
{
    cpu_address_zero_page(a->imm8);
    dec_common();
    return true;
}

static bool trans_DEC_ZEROPAGEX(DisasContext *ctx, arg_DEC_ZEROPAGEX *a) // 0xd6
{
    cpu_address_zero_page_x(a->imm8);
    dec_common();
    return true;
}

static bool trans_DEC_ABSOLUTE(DisasContext *ctx, arg_DEC_ABSOLUTE *a) // 0xce
{
    cpu_address_absolute(a->addr1, a->addr2);
    dec_common();
    return true;
}

static bool trans_DEC_ABSOLUTEX(DisasContext *ctx, arg_DEC_ABSOLUTEX *a) // 0xde
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    dec_common();
    return true;
}

static void inc_xy_common(TCGv r)
{
    TCGLabel *t, *done;
    t = gen_new_label();
    done = gen_new_label();

    tcg_gen_andi_tl(r, r, 0xFF);
    tcg_gen_brcondi_i32(TCG_COND_EQ, r, 0xff, t);
    tcg_gen_addi_i32(r, r, 1);
    tcg_gen_andi_tl(r, r, 0xFF);
    cpu_update_zn_flags(r);
    tcg_gen_br(done);

    gen_set_label(t);
    tcg_gen_movi_tl(r, 0);
    tcg_gen_movi_tl(cpu_zero_flag, 1);
    tcg_gen_movi_tl(cpu_negative_flag, 0);
    gen_set_label(done);
}

static bool trans_INY(DisasContext *ctx, arg_INY *a) // 0xc8
{
    inc_xy_common(cpu_Y);
    return true;
}

// Increment Index X by One
static bool trans_INX(DisasContext *ctx, arg_INX *a) // 0xe8
{
    inc_xy_common(cpu_X);
    return true;
}

static inline void gen_ldu(TCGv reg, TCGv mem)
{
    tcg_gen_qemu_ld_i32(reg, mem, 0, MO_TE);
}

static inline void gen_st(TCGv reg, TCGv mem)
{
    tcg_gen_qemu_st_i32(reg, mem, 0, MO_TE);
}

/*
 * Branch Instructions
 */
static void gen_goto_tb(DisasContext *ctx, int n, target_ulong dest)
{
    const TranslationBlock *tb = ctx->base.tb;

    if (translator_use_goto_tb(&ctx->base, dest)) {
        tcg_gen_goto_tb(n);
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_exit_tb(tb, n);
    } else {
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_lookup_and_goto_ptr();
    }
    ctx->base.is_jmp = DISAS_NORETURN;
}

static bool trans_LDAIM(DisasContext *ctx, arg_LDAIM *a) // 0xa9
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    TCGv R = cpu_A;
    cpu_update_zn_flags(R);
    return true;
}

static bool trans_LDA_ZEROPAGE(DisasContext *ctx, arg_LDA_ZEROPAGE *a) // 0xa5
{
    cpu_address_zero_page(a->imm8);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_ZEROPAGEX(DisasContext *ctx, arg_LDA_ZEROPAGEX *a) // 0xb5
{
    cpu_address_zero_page_x(a->imm8);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDAAB(DisasContext *ctx, arg_LDAAB *a) // 0xAD
{
    cpu_address_absolute(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_ABSOLUTEX(DisasContext *ctx, arg_LDA_ABSOLUTEX *a) // 0xBD
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_ABSOLUTEY(DisasContext *ctx, arg_LDA_ABSOLUTEY *a) // 0xb9
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_INDIRECTX(DisasContext *ctx, arg_LDA_INDIRECTX *a) // 0xa1
{
    cpu_address_indirect_x(a->imm8);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_INDIRECTY(DisasContext *ctx, arg_LDA_INDIRECTY *a) // 0xb1
{
    cpu_address_indirect_y(a->imm8);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDXIM(DisasContext *ctx, arg_LDXIM *a) // 0xa2
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    TCGv R = cpu_X;
    cpu_update_zn_flags(R);
    return true;
}

static bool trans_LDX_ZEROPAGE(DisasContext *ctx, arg_LDX_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm8);
    tcg_gen_mov_tl(cpu_X, op_value);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_LDX_ZEROPAGEY(DisasContext *ctx, arg_LDX_ZEROPAGEY *a)
{
    cpu_address_zero_page_y(a->imm8);
    tcg_gen_mov_tl(cpu_X, op_value);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_LDX_ABSOLUTE(DisasContext *ctx, arg_LDX_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_X, op_value);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_LDX_ABSOLUTEY(DisasContext *ctx, arg_LDX_ABSOLUTEY *a)
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_X, op_value);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_LDYIM(DisasContext *ctx, arg_LDYIM *a) // 0xa0
{
    tcg_gen_movi_i32(cpu_Y, a->imm8);
    TCGv R = cpu_Y;
    cpu_update_zn_flags(R);
    return true;
}

static bool trans_LDY_ZEROPAGE(DisasContext *ctx, arg_LDY_ZEROPAGE *a) // 0xa4
{
    cpu_address_zero_page(a->imm8);
    tcg_gen_mov_tl(cpu_Y, op_value);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_LDY_ZEROPAGEX(DisasContext *ctx, arg_LDY_ZEROPAGEX *a) // 0xb4
{
    cpu_address_zero_page_x(a->imm8);
    tcg_gen_mov_tl(cpu_Y, op_value);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_LDY_ABSOLUTE(DisasContext *ctx, arg_LDY_ABSOLUTE *a) // 0xac
{
    cpu_address_absolute(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_Y, op_value);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_LDY_ABSOLUTEX(DisasContext *ctx, arg_LDY_ABSOLUTEX *a) // 0xbc
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_Y, op_value);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_STA_ZEROPAGE(DisasContext *ctx, arg_STA_ZEROPAGE *a) // 0x85
{
    cpu_address_zero_page(a->imm8);
    // printf("trans_STA_ZEROPAGE imm8 0x%x\n", a->imm8);
    tcg_gen_qemu_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

static bool trans_STA_ZEROPAGEX(DisasContext *ctx, arg_STA_ZEROPAGEX *a) // 0x95
{
    cpu_address_zero_page_x(a->imm8);
    tcg_gen_qemu_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

static bool trans_STAAB(DisasContext *ctx, arg_STAAB *a) // 0x8d
{
    cpu_address_absolute(a->addr1, a->addr2);
    // address_space_stb(&address_space_memory, 8192, 64,
    //                       MEMTXATTRS_UNSPECIFIED, NULL);
    tcg_gen_qemu_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

static bool trans_STA_ABSOLUTEX(DisasContext *ctx, arg_STA_ABSOLUTEX *a) // 0x9D
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    tcg_gen_qemu_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

static bool trans_STA_ABSOLUTEY(DisasContext *ctx, arg_STA_ABSOLUTEY *a) // 0x99
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    tcg_gen_qemu_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

static bool trans_STA_INDIRECTX(DisasContext *ctx, arg_STA_INDIRECTX *a) // 0x81
{
    cpu_address_indirect_x(a->imm8);
    tcg_gen_qemu_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

static bool trans_STA_INDIRECTY(DisasContext *ctx, arg_STA_INDIRECTY *a) // 0x91
{
    cpu_address_indirect_y(a->imm8);
    // tcg_gen_movi_tl(cpu_A, 3);
    tcg_gen_qemu_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

// OR Memory with Accumulator  A OR M -> A
static bool trans_ORA_IM(DisasContext *ctx, arg_ORA_IM *a) // 0x9
{
    // printf("trans_ORA_IM  imm 0x%x\n", a->imm8);
    tcg_gen_ori_tl(cpu_A, cpu_A, a->imm8);
    cpu_update_zn_flags(cpu_A); 
    return true;
}

static bool trans_ORA_ZEROPAGE(DisasContext *ctx, arg_ORA_ZEROPAGE *a) // 0x05
{
    cpu_address_zero_page(a->imm8);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A); 
    return true;
}

static bool trans_ORA_ZEROPAGEX(DisasContext *ctx, arg_ORA_ZEROPAGEX *a) // 0x15
{
    cpu_address_zero_page_x(a->imm8);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A); 
    return true;
}

static bool trans_ORA_ABSOLUTE(DisasContext *ctx, arg_ORA_ABSOLUTE *a) // 0x0d
{
    cpu_address_absolute(a->addr1, a->addr2);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A); 
    return true;
}

static bool trans_ORA_ABSOLUTEX(DisasContext *ctx, arg_ORA_ABSOLUTEX *a) // 0X1D
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A); 
    return true;
}

static bool trans_ORA_ABSOLUTEY(DisasContext *ctx, arg_ORA_ABSOLUTEY *a) // 0x19
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A); 
    return true;
}

static bool trans_ORA_INDIRECTX(DisasContext *ctx, arg_ORA_INDIRECTX *a) // 0x01
{
    cpu_address_indirect_x(a->imm8);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A); 
    return true;
}

static bool trans_ORA_INDIRECTY(DisasContext *ctx, arg_ORA_INDIRECTY *a) // 0x11
{
    cpu_address_indirect_y(a->imm8);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A); 
    return true;
}

// Push Processor Status on Stack
static bool trans_PHP(DisasContext *ctx, arg_PHP *a)
{
    TCGv status = tcg_temp_new_i32();
    tcg_gen_ori_tl(status, cpu_P, 0x30);
    cpu_stack_pushb_a(status);
    return true;
}

// Pull Processor Status from Stack
static bool trans_PLP(DisasContext *ctx, arg_PLP *a)
{
    //TODO:
    cpu_stack_popb(cpu_P);
    tcg_gen_andi_tl(cpu_P, cpu_P, 0xEF);
    tcg_gen_ori_tl(cpu_P, cpu_P, 0x20);

    cpu_pack_p();
    return true;
}

// Set Carry Flag
static bool trans_SEC(DisasContext *ctx, arg_SEC *a)
{
    tcg_gen_movi_tl(cpu_carry_flag, 1);
    return true;
}

static bool trans_EORIM(DisasContext *ctx, arg_EORIM *a)
{
    tcg_gen_xori_tl(cpu_A, cpu_A, a->imm8);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static void eor_common(void)
{
    tcg_gen_xor_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
}

static bool trans_EOR_ZEROPAGE(DisasContext *ctx, arg_EOR_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm8);
    eor_common();
    return true;
}

static bool trans_EOR_ZEROPAGEX(DisasContext *ctx, arg_EOR_ZEROPAGEX *a)
{
    cpu_address_zero_page_x(a->imm8);
    eor_common();
    return true;
}

static bool trans_EOR_ABSOLUTE(DisasContext *ctx, arg_EOR_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr1, a->addr2);
    eor_common();
    return true;
}

static bool trans_EOR_ABSOLUTEX(DisasContext *ctx, arg_EOR_ABSOLUTEX *a)
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    eor_common();
    return true;
}

static bool trans_EOR_ABSOLUTEY(DisasContext *ctx, arg_EOR_ABSOLUTEY *a)
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    eor_common();
    return true;
}

static bool trans_EOR_INDIRECTX(DisasContext *ctx, arg_EOR_INDIRECTX *a)
{
    cpu_address_indirect_x(a->imm8);
    eor_common();
    return true;
}

static bool trans_EOR_INDIRECTY(DisasContext *ctx, arg_EOR_INDIRECTY *a)
{
    cpu_address_indirect_y(a->imm8);
    eor_common();
    return true;
}

// Push Accumulator on Stack
static bool trans_PUSHA(DisasContext *ctx, arg_PUSHA *a) // 0x48
{
    cpu_stack_pushb_a(cpu_A);
    return true;
}

// Pull Accumulator from Stack
static bool trans_PLA(DisasContext *ctx, arg_PLA *a) // 0x68
{
    cpu_stack_popb(cpu_A);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_STY_ZEROPAGE(DisasContext *ctx, arg_STY_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm8);
    tcg_gen_qemu_st_tl(cpu_Y, op_address, 0, MO_UB);
    return true;
}

static bool trans_STY_ZEROPAGEX(DisasContext *ctx, arg_STY_ZEROPAGEX *a)
{
    cpu_address_zero_page_x(a->imm8);
    tcg_gen_qemu_st_tl(cpu_Y, op_address, 0, MO_UB);
    return true;
}

static bool trans_STY_ABSOLUTE(DisasContext *ctx, arg_STY_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr1, a->addr2);
    tcg_gen_qemu_st_tl(cpu_Y, op_address, 0, MO_UB);
    return true;
}

static bool trans_STX_ZEROPAGE(DisasContext *ctx, arg_STX_ZEROPAGE *a) // 0x86
{
    cpu_address_zero_page(a->imm8);
    // tcg_gen_movi_tl(cpu_X, 0x86);
    tcg_gen_qemu_st_tl(cpu_X, op_address, 0, MO_UB);
    return true;
}

static bool trans_STX_ZEROPAGEX(DisasContext *ctx, arg_STX_ZEROPAGEX *a)
{
    cpu_address_zero_page_x(a->imm8);
    tcg_gen_qemu_st_tl(cpu_X, op_address, 0, MO_UB);
    return true;
}

static bool trans_STX_ABSOLUTE(DisasContext *ctx, arg_STX_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr1, a->addr2);
    tcg_gen_qemu_st_tl(cpu_X, op_address, 0, MO_UB);
    return true;
}

// Transfer Index X to Accumulator
static bool trans_TXA(DisasContext *ctx, arg_TXA *a)
{
    tcg_gen_mov_tl(cpu_A, cpu_X);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_TYA(DisasContext *ctx, arg_TYA *a)
{
    tcg_gen_mov_tl(cpu_A, cpu_Y);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_TAY(DisasContext *ctx, arg_TAY *a) // 0xa8
{
    tcg_gen_mov_tl(cpu_Y, cpu_A);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_TAX(DisasContext *ctx, arg_TAX *a)
{
    tcg_gen_mov_tl(cpu_X, cpu_A);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_TSX(DisasContext *ctx, arg_TSX *a)
{
    tcg_gen_mov_tl(cpu_X, cpu_stack_point);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static void sbc_common(TCGv val)
{
    TCGv res = tcg_temp_new_i32();
    tcg_gen_sub_tl(res, cpu_A, val);

    TCGv t_c = tcg_temp_new_i32();
    tcg_gen_setcondi_tl(TCG_COND_NE, t_c, cpu_carry_flag, 1);

    tcg_gen_sub_tl(res, res, t_c);

    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_andi_tl(tmp, res, 0x100);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, tmp, 0);

    // TCGv tmp = tcg_temp_new_i32();
    tcg_gen_xor_tl(tmp, cpu_A, val);

    TCGv tt = tcg_temp_new_i32();
    tcg_gen_xor_tl(tt, cpu_A, res);

    TCGv t = tcg_temp_new_i32();
    tcg_gen_and_tl(t, tt, tmp);
    tcg_gen_andi_tl(t, t, 0x80);

    tcg_gen_setcondi_tl(TCG_COND_NE, cpu_overflow_flag, t, 0);

    tcg_gen_andi_tl(cpu_A, res, 0xFF);
    cpu_update_zn_flags(cpu_A);
}

static bool trans_SBCIM(DisasContext *ctx, arg_SBCIM *a) // 0xe9
{
    TCGv res = tcg_temp_new_i32();
    tcg_gen_movi_tl(res, a->imm8);
    sbc_common(res);
    return true;
}

static bool trans_SBC_ZEROPAGE(DisasContext *ctx, arg_SBC_ZEROPAGE *a) // 0xe5
{
    cpu_address_zero_page(a->imm8);
    sbc_common(op_value);
    return true;
}

static bool trans_SBC_ZEROPAGEX(DisasContext *ctx, arg_SBC_ZEROPAGEX *a) // 0xf5
{
    cpu_address_zero_page_x(a->imm8);
    sbc_common(op_value);
    return true;
}

static bool trans_SBC_ABSOLUTE(DisasContext *ctx, arg_SBC_ABSOLUTE *a) // 0xed
{
    cpu_address_absolute(a->addr1, a->addr2);
    sbc_common(op_value);
    return true;
}

static bool trans_SBC_ABSOLUTEX(DisasContext *ctx, arg_SBC_ABSOLUTEX *a) // 0xfd
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    sbc_common(op_value);
    return true;
}

static bool trans_SBC_ABSOLUTEY(DisasContext *ctx, arg_SBC_ABSOLUTEY *a) // 0xf9
{
    cpu_address_absolute_y(a->addr1, a->addr2);
    sbc_common(op_value);
    return true;
}

static bool trans_SBC_INDIRECTX(DisasContext *ctx, arg_SBC_INDIRECTX *a) // 0xe1
{
    cpu_address_indirect_x(a->imm8);
    sbc_common(op_value);
    return true;
}

static bool trans_SBC_INDIRECTY(DisasContext *ctx, arg_SBC_INDIRECTY *a) // 0xf1
{
    cpu_address_indirect_y(a->imm8);
    sbc_common(op_value);
    return true;
}

/*
 * Data Transfer Instructions
 */

/*
 * Bit and Bit-test Instructions
 */

// static void cpu_modify_flag_asl(TCGv val)
// {
//     TCGv data = tcg_temp_new_i32();
//     tcg_gen_andi_tl(data, val, 0x80);
//     tcg_gen_mov_tl(cpu_carry_flag, data);
// }

// Shift Left One Bit (Memory or Accumulator) C <- [76543210] <- 0
static bool trans_ASL(DisasContext *ctx, arg_ASL *a) // 0xa
{
    TCGv data = tcg_temp_new_i32();
    tcg_gen_andi_tl(data, cpu_A, 0x080);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, data, 0x080);

    tcg_gen_shli_tl(cpu_A, cpu_A, 1);
    tcg_gen_andi_tl(cpu_A, cpu_A, 0x00FF);

    TCGv cpua = cpu_A;
    cpu_update_zn_flags(cpua);
    return true;
}

static void trans_ASL_COMMON(void)
{
    TCGv data = tcg_temp_new_i32();
    tcg_gen_andi_tl(data, op_value, 0x080);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, data, 0x080);

    tcg_gen_shli_tl(op_value, op_value, 1);
    tcg_gen_andi_tl(op_value, op_value, 0x0FF);
    
    cpu_update_zn_flags(op_value);

    tcg_gen_qemu_st_tl(op_value, op_address, 0, MO_UB);
}

static bool trans_ASL_ZEROPAGE(DisasContext *ctx, arg_ASL_ZEROPAGE *a) // 0x06
{
    cpu_address_zero_page(a->imm8);
    trans_ASL_COMMON();
    return true;
}

static bool trans_ASL_ZEROPAGEX(DisasContext *ctx, arg_ASL_ZEROPAGEX *a) // 0x16
{
    cpu_address_zero_page_x(a->imm8);
    trans_ASL_COMMON();
    return true;
}

static bool trans_ASL_ABSOLUTE(DisasContext *ctx, arg_ASL_ABSOLUTE *a) // 0x0e
{
    cpu_address_absolute(a->addr1, a->addr2);
    trans_ASL_COMMON();
    return true;
}

static bool trans_ASL_ABSOLUTEX(DisasContext *ctx, arg_ASL_ABSOLUTEX *a) // 0x1e
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    trans_ASL_COMMON();
    return true;
}

// Clear Carry Flag
static bool trans_CLC(DisasContext *ctx, arg_CLC *a)
{
    tcg_gen_movi_tl(cpu_carry_flag, 0);
    return true;
}

static void bit_common(void)
{
    TCGv val = tcg_temp_new_i32();
    tcg_gen_and_tl(val, cpu_A, op_value);

    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, val, 0);

    tcg_gen_andi_tl(val, op_value, 0x40);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_overflow_flag, val, 0x40);

    tcg_gen_andi_tl(val, op_value, 0x80);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_negative_flag, val, 0x80);
}

// Test Bits in Memory with Accumulator
static bool trans_BIT_ZEROPAGE(DisasContext *ctx, arg_BIT_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm8);
    bit_common();
    return true;
}

static bool trans_BIT_ABSOLUTE(DisasContext *ctx, arg_BIT_ABSOLUTE *a) // 0x2c
{
    cpu_address_absolute(a->addr1, a->addr2);
    bit_common();
    return true;
}

static bool trans_ROLA(DisasContext *ctx, arg_ROLA *a) // 0x2a
{
    TCGv val = tcg_temp_new_i32();
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_shli_tl(tmp, cpu_A, 1);
    tcg_gen_setcondi_tl(TCG_COND_EQ, val, cpu_carry_flag, 1);
    tcg_gen_or_tl(val, tmp, val);

    tcg_gen_setcondi_tl(TCG_COND_GE, cpu_carry_flag, val, 0xFF);
    tcg_gen_andi_tl(cpu_A, val, 0x0FF);

    TCGv r = cpu_A;
    cpu_update_zn_flags(r);
    return true;
}

static void rol_common(void)
{
    TCGv val = tcg_temp_new_i32();
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_shli_tl(tmp, op_value, 1);
    tcg_gen_setcondi_tl(TCG_COND_EQ, val, cpu_carry_flag, 1);
    tcg_gen_add_tl(val, tmp, val);

    tcg_gen_setcondi_tl(TCG_COND_GE, cpu_carry_flag, val, 0xFF);
    tcg_gen_andi_tl(op_value, val, 0x0FF);

    tcg_gen_qemu_st_tl(op_value, op_address, 0, MO_UB);

    TCGv r = op_value;
    cpu_update_zn_flags(r);
}

static bool trans_ROL_ZEROPAGE(DisasContext *ctx, arg_ROL_ZEROPAGE *a) // 0x26
{
    cpu_address_zero_page(a->imm8);
    rol_common();
    return true;
}

static bool trans_ROL_ZEROPAGEX(DisasContext *ctx, arg_ROL_ZEROPAGEX *a) // 0x36
{
    cpu_address_zero_page_x(a->imm8);
    rol_common();
    return true;
}

static bool trans_ROL_ABSOLUTE(DisasContext *ctx, arg_ROL_ABSOLUTE *a) // 0x2e
{
    cpu_address_absolute(a->addr1, a->addr2);
    rol_common();
    return true;
}

static bool trans_ROL_ABSOLUTEX(DisasContext *ctx, arg_ROL_ABSOLUTEX *a) // 0x3e
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    rol_common();
    return true;
}

// Shift One Bit Right (Memory or Accumulator)
static bool trans_LSRA(DisasContext *ctx, arg_LSRA *a) // 0x4a
{
    TCGv val = tcg_temp_new_i32();
    tcg_gen_shri_tl(val, cpu_A, 1);

    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_andi_tl(tmp, cpu_A, 0x01);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, tmp, 1);

    tcg_gen_andi_tl(cpu_A, val, 0x0FF);
    cpu_update_zn_flags(val);
    return true;
}

static void lsr_common(void)
{
    TCGv val = tcg_temp_new_i32();
    tcg_gen_andi_tl(val, op_value, 0x01);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, val, 1);

    tcg_gen_shri_tl(op_value, op_value, 1);
    tcg_gen_andi_tl(op_value, op_value, 0xFF);

    tcg_gen_qemu_st_tl(op_value, op_address, 0, MO_UB);
    cpu_update_zn_flags(op_value);
}

static bool trans_LSR_ZEROPAGE(DisasContext *ctx, arg_LSR_ZEROPAGE *a) // 0x46
{
    cpu_address_zero_page(a->imm8);
    lsr_common();
    return true;
}

static bool trans_LSR_ZEROPAGEX(DisasContext *ctx, arg_LSR_ZEROPAGEX *a) // 0x56
{
    cpu_address_zero_page_x(a->imm8);
    lsr_common();
    return true;
}

static bool trans_LSR_ABSOLUTE(DisasContext *ctx, arg_LSR_ABSOLUTE *a) // 0x4e
{
    cpu_address_absolute(a->addr1, a->addr2);
    lsr_common();
    return true;
}

static bool trans_LSR_ABSOLUTEX(DisasContext *ctx, arg_LSR_ABSOLUTEX *a) // 0x5e
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    lsr_common();
    return true;
}

// Clear Interrupt Disable Bit
static bool trans_CLI(DisasContext *ctx, arg_CLI *a)
{
    tcg_gen_movi_tl(cpu_interrupt_flag, 0);
    return true;
}

static bool trans_RORA(DisasContext *ctx, arg_RORA *a) // 0x6a
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_setcondi_i32(TCG_COND_EQ, tmp, cpu_carry_flag, 1);
    TCGv tmp_carry = tcg_temp_new_i32();
    tcg_gen_mov_tl(tmp_carry, tmp);

    TCGv val = tcg_temp_new_i32();
    tcg_gen_andi_tl(val, cpu_A, 0x01);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, val, 1);

    tcg_gen_shli_tl(tmp, tmp_carry, 7);
    tcg_gen_shri_tl(cpu_A, cpu_A, 1);
    tcg_gen_or_tl(cpu_A, cpu_A, tmp);

    tcg_gen_setcondi_i32(TCG_COND_EQ, cpu_zero_flag, cpu_A, 0);

    tcg_gen_not_tl(tmp_carry, tmp_carry);
    tcg_gen_not_tl(tmp_carry, tmp_carry);
    tcg_gen_setcondi_i32(TCG_COND_EQ, cpu_negative_flag, tmp_carry, 1);

    return true;
}

static void ror_common(void)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_setcondi_i32(TCG_COND_EQ, tmp, cpu_carry_flag, 1);
    TCGv tmp_carry = tcg_temp_new_i32();
    tcg_gen_mov_tl(tmp_carry, tmp);

    TCGv val = tcg_temp_new_i32();
    tcg_gen_andi_tl(val, op_value, 0x01);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, val, 1);

    tcg_gen_shli_tl(tmp, tmp_carry, 7);
    tcg_gen_shri_tl(op_value, op_value, 1);
    tcg_gen_or_tl(op_value, op_value, tmp);
    tcg_gen_andi_tl(op_value, op_value, 0x0FF);

    tcg_gen_setcondi_i32(TCG_COND_EQ, cpu_zero_flag, op_value, 0);
    tcg_gen_not_tl(tmp_carry, tmp_carry);
    tcg_gen_not_tl(tmp_carry, tmp_carry);
    tcg_gen_setcondi_i32(TCG_COND_EQ, cpu_negative_flag, tmp_carry, 1);

    tcg_gen_qemu_st_tl(op_value, op_address, 0, MO_UB);
}

static bool trans_ROR_ZEROPAGE(DisasContext *ctx, arg_ROR_ZEROPAGE *a) // 0x66
{
    cpu_address_zero_page(a->imm8);
    ror_common();
    return true;
}

static bool trans_ROR_ZEROPAGEX(DisasContext *ctx, arg_ROR_ZEROPAGEX *a) // 0x76
{
    cpu_address_zero_page_x(a->imm8);
    ror_common();
    return true;
}

static bool trans_ROR_ABSOLUTE(DisasContext *ctx, arg_ROR_ABSOLUTE *a) // 0x6e
{
    cpu_address_absolute(a->addr1, a->addr2);
    ror_common();
    return true;
}

static bool trans_ROR_ABSOLUTEX(DisasContext *ctx, arg_ROR_ABSOLUTEX *a) // 0x7e
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    ror_common();
    return true;
}

// Clear Overflow Flag
static bool trans_CLV(DisasContext *ctx, arg_CLV *a)
{
    tcg_gen_movi_tl(cpu_overflow_flag, 0);
    return true;
}

// Set Decimal Flag
static bool trans_SED(DisasContext *ctx, arg_SED *a)
{
    tcg_gen_movi_tl(cpu_decimal_flag, 1);
    return true;
}


/*
 * MCU Control Instructions
 */


/*
 *  This instruction performs a single cycle No Operation.
 */
static bool trans_NOP(DisasContext *ctx, arg_NOP *a)
{
    /* NOP */
    return true;
}

static bool trans_SEI(DisasContext *ctx, arg_SEI *a)
{
    tcg_gen_movi_tl(cpu_interrupt_flag, 1);
    return true;
}

static bool trans_CLD(DisasContext *ctx, arg_CLD *a)
{
    tcg_gen_movi_tl(cpu_decimal_flag, 0);
    return true;
}

static bool trans_TXS(DisasContext *ctx, arg_TXS *a)
{
    tcg_gen_mov_tl(cpu_stack_point, cpu_X);
    return true;
}

static bool trans_BRK(DisasContext *ctx, arg_BRK *a)
{
    cpu_stack_pushw(ctx->base.pc_next);
    cpu_stack_popw(cpu_pc);
    cpu_stack_popb(cpu_pc);
    cpu_stack_pushb(0); // TODO: how to push the flag in stack? 
    return true;
}

static void branch_on_res(DisasContext *ctx, int addr, TCGv var, TCGCond cond, int val)
{
    TCGLabel *t, *done;
    t = gen_new_label();
    done = gen_new_label();

    tcg_gen_brcondi_i32(cond, var, val, t);

    gen_goto_tb(ctx, 0, ctx->base.pc_next);
    tcg_gen_br(done);

    gen_set_label(t);
    gen_goto_tb(ctx, 1, addr);
    gen_set_label(done);
}

static bool trans_BPL(DisasContext *ctx, arg_BPL *a) // 0x10
{
    int addr = cpu_address_relative(a->imm8, ctx->base.pc_next);
    branch_on_res(ctx, addr, cpu_negative_flag, TCG_COND_EQ, 0);
    return true;
}

static bool trans_JSR(DisasContext *ctx, arg_JSR *a) // 0x20
{
    cpu_address_absolute(a->addr1, a->addr2);
    cpu_stack_pushw(ctx->base.pc_next - 1);
    // gen_goto_tb(ctx, 1, addr);
    tcg_gen_mov_tl(cpu_pc, op_address);
    ctx->base.is_jmp = DISAS_LOOKUP;
    return true;
}

// Branch on Result Minus
static bool trans_BMI(DisasContext *ctx, arg_BMI *a)
{
    int addr = cpu_address_relative(a->imm8, ctx->base.pc_next);
    branch_on_res(ctx, addr, cpu_negative_flag, TCG_COND_NE, 0);
    return true;
}

// Return from Interrupt
static bool trans_RTI(DisasContext *ctx, arg_RTI *a) // 0x40
{
    // gen_helper_print_sp(cpu_env, cpu_stack_point);
    cpu_stack_popb(cpu_P);
    cpu_stack_popw(cpu_pc);
    tcg_gen_ori_tl(cpu_P, cpu_P, 0x20);
    tcg_gen_movi_tl(cpu_unused_flag, 1);
    gen_helper_set_p(cpu_env, cpu_P);
    cpu_pack_p();
    ctx->base.is_jmp = DISAS_EXIT;
    return true;
}

static bool trans_JMP_ABSOLUTE(DisasContext *ctx, arg_JMP_ABSOLUTE *a) // 0x4c
{
    cpu_address_absolute(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_pc, op_address);
    ctx->base.is_jmp = DISAS_LOOKUP;
    return true;
}

static bool trans_JMP_INDIRECT(DisasContext *ctx, arg_JMP_INDIRECT *a) // 0x6c
{
    cpu_address_indirect(a->addr1, a->addr2);
    tcg_gen_mov_tl(cpu_pc, op_address);
    ctx->base.is_jmp = DISAS_LOOKUP;
    return true;
}

// Return from Subroutine
static bool trans_RTS(DisasContext *ctx, arg_RTS *a) // 0x60
{
    // gen_helper_print_sp(cpu_env, cpu_stack_point);
    cpu_stack_popw(cpu_pc);
    // gen_helper_print_sp(cpu_env, cpu_stack_point);
    tcg_gen_addi_tl(cpu_pc, cpu_pc, 1);
    ctx->base.is_jmp = DISAS_LOOKUP;
    // tcg_gen_movi_i32(cpu_pc, 0xc027);
    // ctx->base.is_jmp = DISAS_EXIT;
    // gen_goto_tb(ctx, 1, addr);
    return true;
}

static void branch_flag_common(DisasContext *ctx, int imm8, TCGCond cond, TCGv flag, int val)
{
    int addr = cpu_address_relative(imm8, ctx->base.pc_next);
    branch_on_res(ctx, addr, flag, cond, val);
}

// Branch on Overflow Clear
static bool trans_BVC(DisasContext *ctx, arg_BVC *a)
{
    branch_flag_common(ctx, a->imm8, TCG_COND_EQ, cpu_overflow_flag, 0);
    return true;
}

// Branch on Overflow Set
static bool trans_BVS(DisasContext *ctx, arg_BVS *a)
{
    branch_flag_common(ctx, a->imm8, TCG_COND_EQ, cpu_overflow_flag, 1);
    return true;
}

// Branch on Carry Clear
static bool trans_BCC(DisasContext *ctx, arg_BCC *a) // 0x90
{
    branch_flag_common(ctx, a->imm8, TCG_COND_EQ, cpu_carry_flag, 0);
    return true;
}

// Branch on Carry Set
static bool trans_BCS(DisasContext *ctx, arg_BCS *a) // 0xb0
{
    branch_flag_common(ctx, a->imm8, TCG_COND_EQ, cpu_carry_flag, 1);
    return true;
}

static bool trans_BNE(DisasContext *ctx, arg_BNE *a) // 0xd0
{
    int addr = cpu_address_relative(a->imm8, ctx->base.pc_next);
    // printf("trans_BNE addr 0x%x, pc_next 0x%x\n", addr, ctx->base.pc_next);
    branch_on_res(ctx, addr, cpu_zero_flag, TCG_COND_EQ, 0);
    return true;
}

// Branch on Result Zero
static bool trans_BEQ(DisasContext *ctx, arg_BEQ *a) // 0xf0
{
    branch_flag_common(ctx, a->imm8, TCG_COND_EQ, cpu_zero_flag, 1);
    return true;
}

/*
 * Extended Instruction Set
 */
static bool trans_ASOZEROPAGE(DisasContext *ctx, arg_ASOZEROPAGE *a)
{
    return true;
}

static bool trans_ASOZEROPAGEX(DisasContext *ctx, arg_ASOZEROPAGEX *a)
{
    return true;
}

static bool trans_ASOABSOLUTE(DisasContext *ctx, arg_ASOABSOLUTE *a)
{
    return true;
}

static bool trans_ASOABSOLUTEX(DisasContext *ctx, arg_ASOABSOLUTEX *a)
{
    return true;
}

static bool trans_ASOABSOLUTEY(DisasContext *ctx, arg_ASOABSOLUTEY *a)
{
    return true;
}

static bool trans_ASOINDIRECTX(DisasContext *ctx, arg_ASOINDIRECTX *a)
{
    return true;
}

static bool trans_ASOINDIRECTY(DisasContext *ctx, arg_ASOINDIRECTY *a)
{
    return true;
}

static bool trans_RLAZEROPAGE(DisasContext *ctx, arg_RLAZEROPAGE *a)
{
    return true;
}

static bool trans_RLAZEROPAGEX(DisasContext *ctx, arg_RLAZEROPAGEX *a)
{
    return true;
}

static bool trans_RLAABSOLUTE(DisasContext *ctx, arg_RLAABSOLUTE *a)
{
    return true;
}

static bool trans_RLAABSOLUTEX(DisasContext *ctx, arg_RLAABSOLUTEX *a)
{
    return true;
}

static bool trans_RLAABSOLUTEY(DisasContext *ctx, arg_RLAABSOLUTEY *a)
{
    return true;
}

static bool trans_RLAINDIRECTX(DisasContext *ctx, arg_RLAINDIRECTX *a)
{
    return true;
}

static bool trans_RLAINDIRECTY(DisasContext *ctx, arg_RLAINDIRECTY *a)
{
    return true;
}

static bool trans_LSEZEROPAGE(DisasContext *ctx, arg_LSEZEROPAGE *a)
{
    return true;
}

static bool trans_LSEZEROPAGEX(DisasContext *ctx, arg_LSEZEROPAGEX *a)
{
    return true;
}

static bool trans_LSEABSOLUTE(DisasContext *ctx, arg_LSEABSOLUTE *a)
{
    return true;
}

static bool trans_LSEABSOLUTEX(DisasContext *ctx, arg_LSEABSOLUTEX *a)
{
    return true;
}

static bool trans_LSEABSOLUTEY(DisasContext *ctx, arg_LSEABSOLUTEY *a)
{
    return true;
}

static bool trans_LSEINDIRECTX(DisasContext *ctx, arg_LSEINDIRECTX *a)
{
    return true;
}

static bool trans_LSEINDIRECTY(DisasContext *ctx, arg_LSEINDIRECTY *a)
{
    return true;
}

static bool trans_RRAZEROPAGE(DisasContext *ctx, arg_RRAZEROPAGE *a)
{
    return true;
}

static bool trans_RRAZEROPAGEX(DisasContext *ctx, arg_RRAZEROPAGEX *a)
{
    return true;
}

static bool trans_RRAABSOLUTE(DisasContext *ctx, arg_RRAABSOLUTE *a)
{
    return true;
}

static bool trans_RRAABSOLUTEX(DisasContext *ctx, arg_RRAABSOLUTEX *a)
{
    return true;
}

static bool trans_RRAABSOLUTEY(DisasContext *ctx, arg_RRAABSOLUTEY *a)
{
    return true;
}

static bool trans_INS_ABSOLUTE(DisasContext *ctx, arg_INS_ABSOLUTE *a)
{
    cpu_address_absolute_x(a->addr1, a->addr2);
    tcg_gen_addi_tl(op_value, op_value, 1);
    tcg_gen_andi_tl(op_value, op_value, 0xFF);
    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
    sbc_common(op_value);
    return true;
}

static bool trans_RRAINDIRECTX(DisasContext *ctx, arg_RRAINDIRECTX *a)
{
    return true;
}

static bool trans_RRAINDIRECTY(DisasContext *ctx, arg_RRAINDIRECTY *a)
{
    return true;
}

/*
 * Extended Instruction Set
 */
static bool trans_NOP_04_ZEROPAGE(DisasContext *ctx, arg_NOP_04_ZEROPAGE *a)
{
    return true;
}

static bool trans_NOP_44_ZEROPAGE(DisasContext *ctx, arg_NOP_44_ZEROPAGE *a)
{
    return true;
}

static bool trans_NOP_64_ZEROPAGE(DisasContext *ctx, arg_NOP_64_ZEROPAGE *a)
{
    return true;
}

static bool trans_NOP_0C_ABSOLUTE(DisasContext *ctx, arg_NOP_0C_ABSOLUTE *a)
{
    return true;
}

static bool trans_NOP_1C_ABSOLUTE(DisasContext *ctx, arg_NOP_1C_ABSOLUTE *a)
{
    return true;
}

static bool trans_NOP_3C_ABSOLUTE(DisasContext *ctx, arg_NOP_3C_ABSOLUTE *a)
{
    return true;
}

static bool trans_NOP_5C_ABSOLUTE(DisasContext *ctx, arg_NOP_5C_ABSOLUTE *a)
{
    return true;
}

static bool trans_NOP_7C_ABSOLUTE(DisasContext *ctx, arg_NOP_7C_ABSOLUTE *a)
{
    return true;
}

static bool trans_NOP_DC_ABSOLUTE(DisasContext *ctx, arg_NOP_DC_ABSOLUTE *a)
{
    return true;
}

static bool trans_NOP_FC_ABSOLUTE(DisasContext *ctx, arg_NOP_FC_ABSOLUTE *a)
{
    return true;
}

static bool trans_NOP_80_IM(DisasContext *ctx, arg_NOP_80_IM *a)
{
    return true;
}

static bool trans_NOP_82_IM(DisasContext *ctx, arg_NOP_82_IM *a)
{
    return true;
}

static bool trans_NOP_89_IM(DisasContext *ctx, arg_NOP_89_IM *a)
{
    return true;
}

static bool trans_NOP_C2_IM(DisasContext *ctx, arg_NOP_C2_IM *a)
{
    return true;
}

static bool trans_NOP_E2_IM(DisasContext *ctx, arg_NOP_E2_IM *a)
{
    return true;
}


/*
 *  Core translation mechanism functions:
 *
 *    - translate()
 *    - canonicalize_skip()
 *    - gen_intermediate_code()
 *    - restore_state_to_opc()
 *
 */
static void translate(DisasContext *ctx)
{


    // uint32_t opcode = next_word(ctx);
    int pc = ctx->base.pc_next;
    uint32_t opcode;
    opcode = decode_insn_load(ctx);
    // printf("nes6502 pc: 0x%x  opcode: 0x%x\n", pc, opcode>>24);
    // if (pc == 53211)
        // printf("a\n");
    fprintf(log_p, "nes6502 pc: 0x%x  opcode: 0x%x\n", pc, opcode>>24);

    // uint8_t val0,val00, val1,val2;
    // address_space_read(&address_space_memory, 4, MEMTXATTRS_UNSPECIFIED, &val0, 1);
    // address_space_read(&address_space_memory, 5, MEMTXATTRS_UNSPECIFIED, &val00, 1);
    // address_space_read(&address_space_memory, 6, MEMTXATTRS_UNSPECIFIED, &val1, 1);
    // address_space_read(&address_space_memory, 7, MEMTXATTRS_UNSPECIFIED, &val2, 1);
    // uint32_t data = address_space_ldl_le(&address_space_memory,
    //                     6,   
    //                     MEMTXATTRS_UNSPECIFIED, NULL);
    // printf("translate 4 val0 0x%x, 5 val00  0x%x, 6 val1 0x%x, 7 val2 0x%x \n", val0, val00, val1, val2);
    // address_space_read(&address_space_memory, 0x8218, MEMTXATTRS_UNSPECIFIED, &val, 2);
    // printf("translate addr 0x8218, val 0x%x \n\n", val);

    if (!decode_insn(ctx, opcode)) {
        printf("unknown opcode 0x%x\n", opcode);
        gen_helper_unsupported(cpu_env);
        ctx->base.is_jmp = DISAS_NORETURN;
    }
}

/* Standardize the cpu_skip condition to NE.  */
static bool canonicalize_skip(DisasContext *ctx)
{
    switch (ctx->skip_cond) {
    case TCG_COND_NEVER:
        /* Normal case: cpu_skip is known to be false.  */
        return false;

    case TCG_COND_ALWAYS:
        /*
         * Breakpoint case: cpu_skip is known to be true, via TB_FLAGS_SKIP.
         * The breakpoint is on the instruction being skipped, at the start
         * of the TranslationBlock.  No need to update.
         */
        return false;

    case TCG_COND_NE:
        if (ctx->skip_var1 == NULL) {
            tcg_gen_mov_tl(cpu_skip, ctx->skip_var0);
        } else {
            tcg_gen_xor_tl(cpu_skip, ctx->skip_var0, ctx->skip_var1);
            ctx->skip_var1 = NULL;
        }
        break;

    default:
        /* Convert to a NE condition vs 0. */
        if (ctx->skip_var1 == NULL) {
            tcg_gen_setcondi_tl(ctx->skip_cond, cpu_skip, ctx->skip_var0, 0);
        } else {
            tcg_gen_setcond_tl(ctx->skip_cond, cpu_skip,
                               ctx->skip_var0, ctx->skip_var1);
            ctx->skip_var1 = NULL;
        }
        ctx->skip_cond = TCG_COND_NE;
        break;
    }
    ctx->skip_var0 = cpu_skip;
    return true;
}

static void nes6502_tr_init_disas_context(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    CPUNES6502State *env = cs->env_ptr;

    ctx->cs = cs;
    ctx->env = env;
    ctx->npc = ctx->base.pc_first;
}

static void nes6502_tr_tb_start(DisasContextBase *db, CPUState *cs)
{
}

static void nes6502_tr_insn_start(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    tcg_gen_insn_start(ctx->base.pc_next);
}

static void nes6502_tr_translate_insn(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    ctx->npc = ctx->base.pc_next;
    translate(ctx);
}

static void nes6502_tr_tb_stop(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    bool nonconst_skip = canonicalize_skip(ctx);
    bool force_exit = ctx->base.tb->flags & TB_FLAGS_SKIP;
    cpu_dump_state(ctx->cs, stdout, 0);

    switch (ctx->base.is_jmp) {
    case DISAS_NORETURN:
        assert(!nonconst_skip);
        break;
    case DISAS_NEXT:
    case DISAS_TOO_MANY:
        gen_goto_tb(ctx, 0, dcbase->pc_next);
        break;
    case DISAS_CHAIN:
        if (!force_exit) {
            /* Note gen_goto_tb checks singlestep.  */
            gen_goto_tb(ctx, 0, ctx->base.pc_next);
            break;
        }
        tcg_gen_movi_tl(cpu_pc, ctx->base.pc_next);
        /* fall through */
    case DISAS_LOOKUP:
            tcg_gen_lookup_and_goto_ptr();
            break;
        /* fall through */
    case DISAS_EXIT:
        tcg_gen_exit_tb(NULL, 0);
        break;
    default:
        g_assert_not_reached();
    }
}

static void nes6502_tr_disas_log(const DisasContextBase *dcbase,
                             CPUState *cs, FILE *logfile)
{
    fprintf(logfile, "IN: %s\n", lookup_symbol(dcbase->pc_first));
    target_disas(logfile, cs, dcbase->pc_first, dcbase->tb->size);
}

static const TranslatorOps nes6502_tr_ops = {
    .init_disas_context = nes6502_tr_init_disas_context,
    .tb_start           = nes6502_tr_tb_start,
    .insn_start         = nes6502_tr_insn_start,
    .translate_insn     = nes6502_tr_translate_insn,
    .tb_stop            = nes6502_tr_tb_stop,
    .disas_log          = nes6502_tr_disas_log,
};

void gen_intermediate_code(CPUState *cs, TranslationBlock *tb, int *max_insns,
                           target_ulong pc, void *host_pc)
{
    DisasContext dc = { };
    translator_loop(cs, tb, max_insns, pc, host_pc, &nes6502_tr_ops, &dc.base);
}
