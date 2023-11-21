/*
 * QEMU AVR CPU helpers
 *
 * Copyright (c) 2016-2020 Michael Rolnik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "hw/core/tcg-cpu-ops.h"
#include "exec/exec-all.h"
#include "exec/address-spaces.h"
#include "exec/helper-proto.h"

bool nes6502_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    if (interrupt_request == CPU_INTERRUPT_NMI) {
        assert(env->carry_flag <= 1);
        assert(env->zero_flag <= 1);
        assert(env->interrupt_flag <= 1);
        assert(env->decimal_flag <= 1);
        assert(env->break_flag <= 1);
        assert(env->unused_flag <= 1);
        assert(env->overflow_flag <= 1);
        assert(env->negative_flag <= 1);

        env->interrupt_flag = 1;
        env->unused_flag = 0;
        env->p = 0;
        env->p = env->carry_flag | env->zero_flag << 1 | env->interrupt_flag << 2 | \
                env->decimal_flag << 3 | env->break_flag << 4 | env->unused_flag << 5 | \
                env->overflow_flag << 6 | env->negative_flag << 7;

        // printf("nes6502_cpu_exec_interrupt env->stack_point 0x%x\n", env->stack_point);
        cpu_stw_data(env, env->stack_point + 0xFF, env->pc_w);
        env->stack_point -= 2;
        env->stack_point &= 0xFF;
        // printf("---------------------------------------Interruptw: sp: %d, pc_w: %d\n", env->stack_point, env->pc_w);

        cpu_stb_data(env, env->stack_point + 0x100, env->p);
        env->stack_point -= 1;
        env->stack_point &= 0xFF;
        // printf("---------------------------------------Interruptb: sp: %d\n", env->stack_point);

        env->pc_w = cpu_lduw_data(env, 0xFFFA);
        cs->interrupt_request &= ~CPU_INTERRUPT_NMI;
        return true;
    }
    return false;
}

void nes6502_cpu_do_interrupt(CPUState *cs)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    int vector = 0;
    int size = avr_feature(env, AVR_FEATURE_JMP_CALL) ? 2 : 1;
    int base = 0;

    if (cs->exception_index == EXCP_RESET) {
        vector = 0;
    } else if (env->intsrc != 0) {
        vector = ctz32(env->intsrc) + 1;
    }

    env->pc_w = base + vector * size;
    env->sregI = 0; /* clear Global Interrupt Flag */

    cs->exception_index = -1;
}

hwaddr nes6502_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    return addr; /* I assume 1:1 address correspondence */
}

bool nes6502_cpu_tlb_fill(CPUState *cs, vaddr addr, int size,
                      MMUAccessType access_type, int mmu_idx,
                      bool probe, uintptr_t retaddr)
{
    uint32_t address, physical, prot;

    /* Linear mapping */
    address = physical = addr & TARGET_PAGE_MASK;
    prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    tlb_set_page(cs, address, physical, prot, mmu_idx, TARGET_PAGE_SIZE);
    return true;
}

/*
 *  helpers
 */

void helper_sleep(CPUNES6502State *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_HLT;
    cpu_loop_exit(cs);
}

void helper_unsupported(CPUNES6502State *env)
{
    CPUState *cs = env_cpu(env);

    /*
     *  I count not find what happens on the real platform, so
     *  it's EXCP_DEBUG for meanwhile
     */
    cs->exception_index = EXCP_DEBUG;
    if (qemu_loglevel_mask(LOG_UNIMP)) {
        qemu_log("UNSUPPORTED\n");
        cpu_dump_state(cs, stderr, 0);
    }
    cpu_loop_exit(cs);
}

void helper_debug(CPUNES6502State *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_DEBUG;
    cpu_loop_exit(cs);
}

void helper_break(CPUNES6502State *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_DEBUG;
    cpu_loop_exit(cs);
}

void helper_wdr(CPUNES6502State *env)
{
    qemu_log_mask(LOG_UNIMP, "WDG reset (not implemented)\n");
}

/*
 * This function implements IN instruction
 *
 * It does the following
 * a.  if an IO register belongs to CPU, its value is read and returned
 * b.  otherwise io address is translated to mem address and physical memory
 *     is read.
 * c.  it caches the value for sake of SBI, SBIC, SBIS & CBI implementation
 *
 */
target_ulong helper_inb(CPUNES6502State *env, uint32_t port)
{
    target_ulong data = 0;
    return data;
}

/*
 *  This function implements OUT instruction
 *
 *  It does the following
 *  a.  if an IO register belongs to CPU, its value is written into the register
 *  b.  otherwise io address is translated to mem address and physical memory
 *      is written.
 *  c.  it caches the value for sake of SBI, SBIC, SBIS & CBI implementation
 *
 */
void helper_outb(CPUNES6502State *env, uint32_t port, uint32_t data)
{
    data &= 0x000000ff;
}

/*
 *  this function implements LD instruction when there is a possibility to read
 *  from a CPU register
 */
target_ulong helper_fullrd(CPUNES6502State *env, uint32_t addr)
{
    uint8_t data;

    env->fullacc = false;

    if (addr < NUMBER_OF_CPU_REGISTERS) {
        /* CPU registers */
        data = env->r[addr];
    } else if (addr < NUMBER_OF_CPU_REGISTERS + NUMBER_OF_IO_REGISTERS) {
        /* IO registers */
        data = helper_inb(env, addr - NUMBER_OF_CPU_REGISTERS);
    } else {
        /* memory */
        data = address_space_ldub(&address_space_memory, OFFSET_DATA + addr,
                                  MEMTXATTRS_UNSPECIFIED, NULL);
    }
    return data;
}

/*
 *  this function implements ST instruction when there is a possibility to write
 *  into a CPU register
 */
void helper_fullwr(CPUNES6502State *env, uint32_t data, uint32_t addr)
{
    env->fullacc = false;

    /* Following logic assumes this: */
    assert(OFFSET_CPU_REGISTERS == OFFSET_DATA);
    assert(OFFSET_IO_REGISTERS == OFFSET_CPU_REGISTERS +
                                  NUMBER_OF_CPU_REGISTERS);

    if (addr < NUMBER_OF_CPU_REGISTERS) {
        /* CPU registers */
        env->r[addr] = data;
    } else if (addr < NUMBER_OF_CPU_REGISTERS + NUMBER_OF_IO_REGISTERS) {
        /* IO registers */
        helper_outb(env, addr - NUMBER_OF_CPU_REGISTERS, data);
    } else {
        /* memory */
        address_space_stb(&address_space_memory, OFFSET_DATA + addr, data,
                          MEMTXATTRS_UNSPECIFIED, NULL);
    }
}

void helper_set_p(CPUNES6502State *env, uint32_t p)
{
    assert(p <= 255);
    env->carry_flag = (p >> 0) & 0x01;
    env->zero_flag = (p >> 1) & 0x01;
    env->interrupt_flag = (p >> 2) & 0x01;
    env->decimal_flag = (p >> 3) & 0x01;
    env->break_flag = (p >> 4) & 0x01;
    env->unused_flag = (p >> 5) & 0x01;
    env->overflow_flag = (p >> 6) & 0x01;
    env->negative_flag = (p >> 7) & 0x01;
}

void helper_print_p(CPUNES6502State *env, uint32_t p)
{
    assert(p <= 255);
    printf("print_p carry_flag %d, zero_flag %d, negative_flag %d\n", (p >> 0) & 0x01, (p >> 1) & 0x01, (p >> 7) & 0x01);
    // env->zero_flag = (p >> 1) & 0x01;
    // env->interrupt_flag = (p >> 2) & 0x01;
    // env->decimal_flag = (p >> 3) & 0x01;
    // env->break_flag = (p >> 4) & 0x01;
    // env->unused_flag = (p >> 5) & 0x01;
    // env->overflow_flag = (p >> 6) & 0x01;
    // env->overflow_flag = (p >> 7) & 0x01;
}

void helper_print_carry_flag(CPUNES6502State *env, uint32_t p)
{
    assert(p <= 255);
    printf("print_carry_flag  %d\n", p);
}

void helper_print_zero_flag(CPUNES6502State *env, uint32_t p)
{
    assert(p <= 255);
    printf("print_zero_flag  %d\n", p);
}

void helper_print_negative_flag(CPUNES6502State *env, uint32_t p)
{
    assert(p <= 255);
    printf("print_negative_flag  %d\n", p);
}

void helper_print_A(CPUNES6502State *env, uint32_t p)
{
    printf("print_A %d\n", p);
}

void helper_print_op_value(CPUNES6502State *env, uint32_t p)
{
    printf("print_op_value %d\n", p);
}

void helper_print_sp(CPUNES6502State *env, uint32_t p)
{
    printf("print_sp %d, pc %x\n", p, env->pc_w);
}

void helper_print_x(CPUNES6502State *env, uint32_t p)
{
    printf("print_x %d\n", p);
}
