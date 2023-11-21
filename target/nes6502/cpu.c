/*
 * QEMU AVR CPU
 *
 * Copyright (c) 2019-2020 Michael Rolnik
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
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "exec/exec-all.h"
#include "cpu.h"
#include "disas/dis-asm.h"
#include "exec/address-spaces.h"

static void nes6502_cpu_set_pc(CPUState *cs, vaddr value)
{
    NES6502CPU *cpu = NES6502_CPU(cs);

    cpu->env.pc_w = value; /* internally PC points to words */
}

static vaddr nes6502_cpu_get_pc(CPUState *cs)
{
    NES6502CPU *cpu = NES6502_CPU(cs);

    return cpu->env.pc_w;
}

static bool nes6502_cpu_has_work(CPUState *cs)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    return (cs->interrupt_request & (CPU_INTERRUPT_HARD | CPU_INTERRUPT_RESET))
            && cpu_interrupts_enabled(env);
}

static void nes6502_cpu_synchronize_from_tb(CPUState *cs,
                                        const TranslationBlock *tb)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    tcg_debug_assert(!(cs->tcg_cflags & CF_PCREL));
    env->pc_w = tb->pc; /* internally PC points to words */
}

static void nes6502_cpu_exec_exit(CPUState *cs)
{
    // NES6502CPU *cpu = NES6502_CPU(cs);
    // CPUNES6502State *env = &cpu->env;

    // int val;
    // address_space_read(&address_space_memory, 6, MEMTXATTRS_UNSPECIFIED, &val, 2);

    // uint32_t data = address_space_ldl_le(&address_space_memory,
    //                     6,
    //                     MEMTXATTRS_UNSPECIFIED, NULL);

    // printf("nes6502_cpu_exec_exit addr 6, val 0x%x data 0x%x\n", val, data);
    // cpu_dump_state(cs, stdout, 0);
}

static void nes6502_restore_state_to_opc(CPUState *cs,
                                     const TranslationBlock *tb,
                                     const uint64_t *data)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    env->pc_w = data[0];
}

static void nes6502_cpu_reset_hold(Object *obj)
{
    CPUState *cs = CPU(obj);
    NES6502CPU *cpu = NES6502_CPU(cs);
    NES6502CPUClass *mcc = NES6502_CPU_GET_CLASS(cpu);
    CPUNES6502State *env = &cpu->env;

    if (mcc->parent_phases.hold) {
        mcc->parent_phases.hold(obj);
    }

    env->pc_w = 0;
    env->sregI = 1;
    env->sregC = 0;
    env->sregZ = 0;
    env->sregN = 0;
    env->sregV = 0;
    env->sregS = 0;
    env->sregH = 0;
    env->sregT = 0;

    env->rampD = 0;
    env->rampX = 0;
    env->rampY = 0;
    env->rampZ = 0;
    env->eind = 0;

    env->skip = 0;

    memset(env->r, 0, sizeof(env->r));

    env->reg_A = 0;
    env->reg_X = 0;
    env->reg_Y = 0;
    env->stack_point = 0xfd;
    env->carry_flag = 0;
    env->zero_flag = 0;
    env->interrupt_flag = 0;
    env->decimal_flag = 0;
    env->break_flag = 0;
    env->unused_flag = 0;
    env->overflow_flag = 0;
    env->negative_flag = 0;
}

static void nes6502_cpu_disas_set_info(CPUState *cpu, disassemble_info *info)
{
    info->mach = bfd_arch_avr;
    info->print_insn = nes6502_print_insn;
}

static void nes6502_cpu_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    NES6502CPUClass *mcc = NES6502_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }
    qemu_init_vcpu(cs);
    cpu_reset(cs);

    mcc->parent_realize(dev, errp);
}

static void nes6502_cpu_set_int(void *opaque, int irq, int level)
{
    NES6502CPU *cpu = opaque;
    CPUNES6502State *env = &cpu->env;
    CPUState *cs = CPU(cpu);
    uint64_t mask = (1ull << irq);

    if (level) {
        env->intsrc |= mask;
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        env->intsrc &= ~mask;
        if (env->intsrc == 0) {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
        }
    }
}

static void nes6502_cpu_initfn(Object *obj)
{
    NES6502CPU *cpu = NES6502_CPU(obj);

    cpu_set_cpustate_pointers(cpu);

    /* Set the number of interrupts supported by the CPU. */
    qdev_init_gpio_in(DEVICE(cpu), nes6502_cpu_set_int,
                      sizeof(cpu->env.intsrc) * 8);
}

static ObjectClass *nes6502_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;

    oc = object_class_by_name(cpu_model);
    if (object_class_dynamic_cast(oc, TYPE_NES6502_CPU) == NULL ||
        object_class_is_abstract(oc)) {
        oc = NULL;
    }
    return oc;
}

static void nes6502_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    // NES6502CPU *cpu = NES6502_CPU(cs);
    // CPUNES6502State *env = &cpu->env;

    //uint32_t reg_A;
    //uint32_t reg_X;
    //uint32_t reg_Y;
    //uint32_t carry_flag; /* 0x00000001 1 bit */
    //uint32_t zero_flag; /* 0x00000001 1 bit */
    //uint32_t interrupt_flag; /* 0x00000001 1 bit */
    //uint32_t decimal_flag; /* 0x00000001 1 bit */
    //uint32_t break_flag; /* 0x00000001 1 bit */
    //uint32_t unused_flag; /* 0x00000001 1 bit */
    //uint32_t overflow_flag; /* 0x00000001 1 bit */
    //uint32_t negative_flag; /* 0x00000001 1 bit */
    //uint32_t stack_point; /* 16 bits */
    //uint8_t p;
    //uint32_t op_address;  
    //uint32_t op_value;
    
    // qemu_fprintf(f, "\n");
    // qemu_fprintf(f, "PC:    %06x\n", env->pc_w); /* PC points to words */
    // qemu_fprintf(f, "SP:      %04x\n", env->stack_point);
    // qemu_fprintf(f, "reg_A:     %02x\n", env->reg_A);
    // qemu_fprintf(f, "reg_X:     %02x\n", env->reg_X);
    // qemu_fprintf(f, "reg_Y:     %02x\n", env->reg_Y);
    // qemu_fprintf(f, "op_address:     %02x\n", env->op_address);
    // qemu_fprintf(f, "op_value:      %02x\n", env->op_value);
    // qemu_fprintf(f, "p:       %02x\n", env->p);
    // qemu_fprintf(f, "P:    [ %c %c %c %c %c %c %c %c ]\n",
    //              env->carry_flag ? 'C' : '-',
    //              env->zero_flag ? 'Z' : '-',
    //              env->interrupt_flag ? 'I' : '-',
    //              env->decimal_flag ? 'D' : '-',
    //              env->break_flag ? 'B' : '-',
    //              env->unused_flag ? 'U' : '-', 
    //              env->overflow_flag ? 'O' : '-',
    //              env->negative_flag ? 'N' : '-');

    // qemu_fprintf(f, "\n");
}

#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps avr_sysemu_ops = {
    .get_phys_page_debug = nes6502_cpu_get_phys_page_debug,
};

#include "hw/core/tcg-cpu-ops.h"

static const struct TCGCPUOps avr_tcg_ops = {
    .initialize = nes6502_cpu_tcg_init,
    .synchronize_from_tb = nes6502_cpu_synchronize_from_tb,
    .restore_state_to_opc = nes6502_restore_state_to_opc,
    .cpu_exec_interrupt = nes6502_cpu_exec_interrupt,
    .tlb_fill = nes6502_cpu_tlb_fill,
    .do_interrupt = nes6502_cpu_do_interrupt,
    .cpu_exec_exit = nes6502_cpu_exec_exit,
};

static void nes6502_cpu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);
    NES6502CPUClass *mcc = NES6502_CPU_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    device_class_set_parent_realize(dc, nes6502_cpu_realizefn, &mcc->parent_realize);

    resettable_class_set_parent_phases(rc, NULL, nes6502_cpu_reset_hold, NULL,
                                       &mcc->parent_phases);

    cc->class_by_name = nes6502_cpu_class_by_name;

    cc->has_work = nes6502_cpu_has_work;
    cc->dump_state = nes6502_cpu_dump_state;
    cc->set_pc = nes6502_cpu_set_pc;
    cc->get_pc = nes6502_cpu_get_pc;
    dc->vmsd = &vms_nes6502_cpu;
    cc->sysemu_ops = &avr_sysemu_ops;
    cc->disas_set_info = nes6502_cpu_disas_set_info;
    cc->gdb_read_register = nes6502_cpu_gdb_read_register;
    cc->gdb_write_register = nes6502_cpu_gdb_write_register;
    cc->gdb_adjust_breakpoint = nes6502_cpu_gdb_adjust_breakpoint;
    cc->gdb_num_core_regs = 35;
    cc->gdb_core_xml_file = "avr-cpu.xml";
    cc->tcg_ops = &avr_tcg_ops;
}

static void avr_avr5_initfn(Object *obj)
{
    NES6502CPU *cpu = NES6502_CPU(obj);
    CPUNES6502State *env = &cpu->env;

    set_avr_feature(env, AVR_FEATURE_LPM);
    set_avr_feature(env, AVR_FEATURE_IJMP_ICALL);
    set_avr_feature(env, AVR_FEATURE_ADIW_SBIW);
    set_avr_feature(env, AVR_FEATURE_SRAM);
    set_avr_feature(env, AVR_FEATURE_BREAK);

    set_avr_feature(env, AVR_FEATURE_2_BYTE_PC);
    set_avr_feature(env, AVR_FEATURE_2_BYTE_SP);
    set_avr_feature(env, AVR_FEATURE_JMP_CALL);
    set_avr_feature(env, AVR_FEATURE_LPMX);
    set_avr_feature(env, AVR_FEATURE_MOVW);
    set_avr_feature(env, AVR_FEATURE_MUL);
}

static void avr_avr51_initfn(Object *obj)
{
    NES6502CPU *cpu = NES6502_CPU(obj);
    CPUNES6502State *env = &cpu->env;

    set_avr_feature(env, AVR_FEATURE_LPM);
    set_avr_feature(env, AVR_FEATURE_IJMP_ICALL);
    set_avr_feature(env, AVR_FEATURE_ADIW_SBIW);
    set_avr_feature(env, AVR_FEATURE_SRAM);
    set_avr_feature(env, AVR_FEATURE_BREAK);

    set_avr_feature(env, AVR_FEATURE_2_BYTE_PC);
    set_avr_feature(env, AVR_FEATURE_2_BYTE_SP);
    set_avr_feature(env, AVR_FEATURE_RAMPZ);
    set_avr_feature(env, AVR_FEATURE_ELPMX);
    set_avr_feature(env, AVR_FEATURE_ELPM);
    set_avr_feature(env, AVR_FEATURE_JMP_CALL);
    set_avr_feature(env, AVR_FEATURE_LPMX);
    set_avr_feature(env, AVR_FEATURE_MOVW);
    set_avr_feature(env, AVR_FEATURE_MUL);
}

/*
 * Setting features of AVR core type avr6
 * --------------------------------------
 *
 * This type of AVR core is present in the following AVR MCUs:
 *
 * atmega2560, atmega2561, atmega256rfr2, atmega2564rfr2
 */
static void avr_avr6_initfn(Object *obj)
{
    NES6502CPU *cpu = NES6502_CPU(obj);
    CPUNES6502State *env = &cpu->env;

    set_avr_feature(env, AVR_FEATURE_LPM);
    set_avr_feature(env, AVR_FEATURE_IJMP_ICALL);
    set_avr_feature(env, AVR_FEATURE_ADIW_SBIW);
    set_avr_feature(env, AVR_FEATURE_SRAM);
    set_avr_feature(env, AVR_FEATURE_BREAK);

    set_avr_feature(env, AVR_FEATURE_3_BYTE_PC);
    set_avr_feature(env, AVR_FEATURE_2_BYTE_SP);
    set_avr_feature(env, AVR_FEATURE_RAMPZ);
    set_avr_feature(env, AVR_FEATURE_EIJMP_EICALL);
    set_avr_feature(env, AVR_FEATURE_ELPMX);
    set_avr_feature(env, AVR_FEATURE_ELPM);
    set_avr_feature(env, AVR_FEATURE_JMP_CALL);
    set_avr_feature(env, AVR_FEATURE_LPMX);
    set_avr_feature(env, AVR_FEATURE_MOVW);
    set_avr_feature(env, AVR_FEATURE_MUL);
}

typedef struct NES6502CPUInfo {
    const char *name;
    void (*initfn)(Object *obj);
} NES6502CPUInfo;


static void nes6502_cpu_list_entry(gpointer data, gpointer user_data)
{
    const char *typename = object_class_get_name(OBJECT_CLASS(data));

    qemu_printf("%s\n", typename);
}

void nes6502_cpu_list(void)
{
    GSList *list;
    list = object_class_get_list_sorted(TYPE_NES6502_CPU, false);
    g_slist_foreach(list, nes6502_cpu_list_entry, NULL);
    g_slist_free(list);
}

#define DEFINE_nes6502_cpu_TYPE(model, initfn) \
    { \
        .parent = TYPE_NES6502_CPU, \
        .instance_init = initfn, \
        .name = NES6502_CPU_TYPE_NAME(model), \
    }

static const TypeInfo nes6502_cpu_type_info[] = {
    {
        .name = TYPE_NES6502_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(NES6502CPU),
        .instance_init = nes6502_cpu_initfn,
        .class_size = sizeof(NES6502CPUClass),
        .class_init = nes6502_cpu_class_init,
        .abstract = true,
    },
    DEFINE_nes6502_cpu_TYPE("avr5", avr_avr5_initfn),
    DEFINE_nes6502_cpu_TYPE("avr51", avr_avr51_initfn),
    DEFINE_nes6502_cpu_TYPE("avr6", avr_avr6_initfn),
};

DEFINE_TYPES(nes6502_cpu_type_info)
