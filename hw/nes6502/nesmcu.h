/*
 * QEMU ATmega MCU
 *
 * Copyright (c) 2019-2020 Philippe Mathieu-Daudé
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_AVR_ATMEGA_H
#define HW_AVR_ATMEGA_H

#include "hw/char/avr_usart.h"
#include "hw/timer/avr_timer16.h"
#include "hw/misc/avr_power.h"
#include "target/nes6502/cpu.h"
#include "qom/object.h"
#include "qemu/units.h"
#include "ui/console.h"
#include "hw/input/adb.h"

#define TYPE_NES6502_MCU     "nesmcu"
#define TYPE_ATMEGA168_MCU  "ATmega168"
#define TYPE_ATMEGA328_MCU  "ATmega328"
#define TYPE_ATMEGA1280_MCU "ATmega1280"
#define TYPE_ATMEGA2560_MCU "ATmega2560"

typedef struct NesMcuState NesMcuState;
DECLARE_INSTANCE_CHECKER(NesMcuState, NES6502_MCU,
                         TYPE_NES6502_MCU)

#define POWER_MAX 2
#define USART_MAX 4
#define TIMER_MAX 6
#define GPIO_MAX 12

#define WORK_RAM_SIZE 64 * KiB

struct NesMcuState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    NES6502CPU cpu;
    MemoryRegion flash;
    MemoryRegion eeprom;
    MemoryRegion cpu_ram;
    MemoryRegion memory;
    MemoryRegion ppu_ram;
    MemoryRegion mmc_prg_pages;
    MemoryRegion mmc_chr_pages;
    MemoryRegion ppu_sprram;
    DeviceState *io;
    DeviceState *ppu;
    DeviceState *fb;
    DeviceState *psg;
    AVRMaskState pwr[POWER_MAX];
    AVRUsartState usart[USART_MAX];
    AVRTimer16State timer[TIMER_MAX];
    uint64_t xtal_freq_hz;

    /* ADB */
    ADBBusState adb_bus;

    int mmc_prg_pages_number;
    int mmc_chr_pages_number;

    QemuConsole *con;
    MemoryRegionSection fbsection;
    unsigned int	fb_base_phys;
	unsigned int	fb_xres;
	unsigned int	fb_yres;
};

enum next_irqs {
    NEXT_FD_I,
    NEXT_KBD_I,
    NEXT_PWR_I,
    NEXT_ENRX_I,
    NEXT_ENTX_I,
    NEXT_SCSI_I,
    NEXT_CLK_I,
    NEXT_SCC_I,
    NEXT_ENTX_DMA_I,
    NEXT_ENRX_DMA_I,
    NEXT_SCSI_DMA_I,
    NEXT_SCC_DMA_I,
    NEXT_SND_I,
    NEXT_NUM_IRQS
};

#endif /* HW_AVR_ATMEGA_H */
