#include "qemu/osdep.h"
#include "hw/char/imx_serial.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "exec/address-spaces.h"
#include "ui/input.h"
#include "ui/console.h"

#ifndef DEBUG_IMX_UART
#define DEBUG_IMX_UART 0
#endif

#define TYPE_NES_PSG "nes-psg" 
OBJECT_DECLARE_SIMPLE_TYPE(PSGState, NES_PSG)

#define DPRINTF(fmt, args...) \
    do { \
        if (DEBUG_IMX_UART) { \
            fprintf(stderr, "[%s]%s: " fmt , TYPE_NES_PSG, \
                                             __func__, ##args); \
        } \
    } while (0)

struct PSGState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;

    int key[10];

    int flags[10];
};

static uint8_t prev_write;
static int p = 10;
int count[10] = {0};
#define LASTING_CYCLES 30

static uint64_t psg_read(void *opaque, hwaddr offset,
                                unsigned size)
{
    PSGState *s = (PSGState *)opaque;
    if (offset == 0x16) 
    {
        if (p++ < 9) 
        {
            // printf("nes_key_state(p): %d, p: %d, s->key: %d\n", s->key[p] == p, p, s->key[p]);
            if (s->key[p] == p) 
            {
                if (count[p] < LASTING_CYCLES)
                {
                    count[p]++;
                    return 1;
                } else {
                    count[p] = 0;
                    s->key[p] = -1;
                }
                
                // s->key[p] = -1;
                // return 1;
            }
        }
    }
    return 0;
}

static uint8_t cpu_ram_read(uint64_t addr)
{
    uint8_t val;
    address_space_read(&address_space_memory, addr & 0x7FF, MEMTXATTRS_UNSPECIFIED, &val, 1);
    return val;
}

static void ppu_sprram_write(uint8_t val)
{
    address_space_write(&address_space_memory, 0x2000 + 4, MEMTXATTRS_UNSPECIFIED, &val, 1);
}

static void psg_write(void *opaque, hwaddr offset,
                             uint64_t data, unsigned size)
{
    if (offset == 0x14) {
        for (int i = 0; i < 256; i++) {
            ppu_sprram_write(cpu_ram_read((0x100 * data) + i));
        }
        return;
    }
    if (offset == 0x16) {
        if ((data & 1) == 0 && prev_write == 1) {
            // strobe
            p = 0;
        }
    }
    prev_write = data & 1;
}

static const struct MemoryRegionOps psg_ops = {
    .read = psg_read,
    .write = psg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


// int last_case = 0;
// static void set_flags(int flags[], int current_case)
// {
//     if (current_case == last_case)
//         flags[current_case] = 1;
//     else
//         flags[current_case] = 0;
//     last_case = current_case;
// }



static void queue_code(void *opaque, int code)
{
    PSGState *s = (PSGState *)opaque;
    switch(code) {
        case 37: // A - K
            s->key[1] = 1;
            // set_flags(s->flags, 1);
            break;
        case 36: // B - J
            s->key[2] = 2;
            // set_flags(s->flags, 2);
            break;
        case 22: // Select - U
            s->key[3] = 3;
            // set_flags(s->flags, 3);
            break;
        case 23: // Start - I
            s->key[4] = 4;
            // set_flags(s->flags, 4);
            break;
        case 17: // Up - W
            s->key[5] = 5;
            // set_flags(s->flags, 5);
            break;
        case 31: // Down - s
            s->key[6] = 6;
            // set_flags(s->flags, 6);
            break;
        case 30: // Left - A
            s->key[7] = 7;
            // set_flags(s->flags, 7); 
            break;
        case 32: // Right - D
            s->key[8] = 8;
            // set_flags(s->flags, 8);
            break;
        case 165:
        case 164:
        case 150:
        case 151:
        case 145:
        case 159:
        case 158:
        case 160:
            break;
        default:
            s->key[0] = -1;
            printf("nespsg unbind key\n");
            break;
    }
}

static void nextkbd_event(void *opaque, int ch)
{

    queue_code(opaque, ch);
}

static void psg_realize(DeviceState *dev, Error **errp)
{
    PSGState *s = NES_PSG(dev);
    for (int i = 0; i < 10; i++)
        s->key[i] = -1;
    qemu_add_kbd_event_handler(nextkbd_event, s);
}

static void psg_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PSGState *s = NES_PSG(obj);

    memory_region_init_io(&s->iomem, obj, &psg_ops, s,
                          TYPE_NES_PSG, 0x2000);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void psg_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = psg_realize;
    dc->desc = "xiaobawang nes psg";
}
static const TypeInfo NES_PSG_info = {
    .name           = TYPE_NES_PSG,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(PSGState),
    .instance_init  = psg_init,
    .class_init     = psg_class_init,
};

static void NES_PSG_register_types(void)
{
    type_register_static(&NES_PSG_info);
}

type_init(NES_PSG_register_types)
