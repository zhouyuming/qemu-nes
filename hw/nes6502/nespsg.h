#ifndef NES_PSG_H
#define NES_PSG_H

#include "qemu/osdep.h"
#include "hw/char/imx_serial.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define TYPE_NES_PSG "nes-psg" 
OBJECT_DECLARE_SIMPLE_TYPE(PSGState, NES_PSG)
struct PPUState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
};

#endif
