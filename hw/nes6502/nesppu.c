#include "qemu/osdep.h"
#include "hw/char/imx_serial.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "nesppu.h"
#include "exec/address-spaces.h"
#include "exec/cpu-defs.h"

PPUState *ppu_;

#define DPRINTF(fmt, args...) \
    do { \
        if (DEBUG_IMX_UART) { \
            fprintf(stderr, "[%s]%s: " fmt , TYPE_NES_PPU, \
                                             __func__, ##args); \
        } \
    } while (0)

byte ppu_sprite_palette[4][4];
bool ppu_2007_first_read;
byte ppu_addr_latch;

byte ppu_latch;
bool ppu_sprite_hit_occured = false;

// static const word ppu_base_nametable_addresses[4] = { 0x2000, 0x2400, 0x2800, 0x2C00 };

// Screen State and Rendering

// For sprite-0-hit checks
byte ppu_screen_background[264][248];

// Precalculated tile high and low bytes addition for pattern tables
byte ppu_l_h_addition_table[256][256][8];
byte ppu_l_h_addition_flip_table[256][256][8];

static byte read_ppu_sprram(byte addr)
{
    byte val;
    address_space_read(&address_space_memory, 0x61A000 + addr, MEMTXATTRS_UNSPECIFIED, &val, 1);
    return val;
}

static void write_ppu_sprram(byte addr, byte val)
{
    address_space_write(&address_space_memory, 0x61A000 + addr, MEMTXATTRS_UNSPECIFIED, &val, 1);
}

static inline word ppu_get_real_ram_address(word address)
{
    if (address < 0x2000) {
        return address;
    }
    else if (address < 0x3F00) {
        if (address < 0x3000) {
            return address;
        }
        else {
            return address;// - 0x1000;
        }
    }
    else if (address < 0x4000) {
        address = 0x3F00 | (address & 0x1F);
        if (address == 0x3F10 || address == 0x3F14 || address == 0x3F18 || address == 0x3F1C)
            return address - 0x10;
        else
            return address;
    }
    assert(address < 0x4000);
    return 0xFFFF;
}

static byte ppu_ram_read(word addr)
{
    byte val;
    address_space_read(&address_space_memory, 0x16000 + ppu_get_real_ram_address(addr), MEMTXATTRS_UNSPECIFIED, &val, 1);
    return val;
}

static void ppu_ram_write(word addr, byte val)
{
     address_space_write(&address_space_memory, 0x16000 + ppu_get_real_ram_address(addr), MEMTXATTRS_UNSPECIFIED, &val, 1);
}

static bool common_bit_set(long long value, byte position) { return value & (1L << position); }
#define M_common(SUFFIX, TYPE) \
    static void common_set_bit##SUFFIX(TYPE *variable, byte position)    { *variable |= 1L << position;    } \
    static void common_unset_bit##SUFFIX(TYPE *variable, byte position)  { *variable &= ~(1L << position); } \
    static void common_toggle_bit##SUFFIX(TYPE *variable, byte position) { *variable ^= 1L << position;    } \
    static void common_modify_bit##SUFFIX(TYPE *variable, byte position, bool set) \
        { set ? common_set_bit##SUFFIX(variable, position) : common_unset_bit##SUFFIX(variable, position); }

// M_common(b, byte)
// M_common(w, word)
// M_common(d, dword)
// M_common(q, qword)

static void common_unset_bitb(byte *variable, byte position)  { *variable &= ~(1L << position); }

static void common_set_bitb(byte *variable, byte position)    
{ 
    *variable |= 1L << position;    
}

static void common_modify_bitb(byte *variable, byte position, bool set) 
{ 
    set ? common_set_bitb(variable, position) : common_unset_bitb(variable, position); 
}

static byte ppu_vram_address_increment(PPUState *ppu)
{ 
    return common_bit_set(ppu->PPUCTRL, 2) ? 32 : 1;
}

static void ppu_set_sprite_0_hit(bool yesno, PPUState *ppu)
{ 
    common_modify_bitb(&ppu->PPUSTATUS, 6, yesno); 
}

static bool ppu_shows_background(PPUState *ppu)                                  
{ 
    return common_bit_set(ppu->PPUMASK, 3); 
}

static void ppu_set_in_vblank(bool yesno)                           
{ 
    common_modify_bitb(&ppu_->PPUSTATUS, 7, yesno); 
}

static bool ppu_generates_nmi(PPUState *ppu)                                     
{ 
    return common_bit_set(ppu->PPUCTRL, 7);                   
}

static uint64_t ppu_read(void *opaque, hwaddr offset,
                                unsigned size)
{
    PPUState *ppu = (PPUState *)opaque;
    ppu->PPUADDR &= 0x3FFF;
    offset &= 7;
    switch (offset) {
        case 2:
        {
            byte value = ppu->PPUSTATUS;
            ppu_set_in_vblank(false);
            ppu_set_sprite_0_hit(false, ppu);
            ppu->scroll_received_x = 0;
            ppu->PPUSCROLL = 0;
            ppu->addr_received_high_byte = 0;
            ppu_latch = value;
            ppu_addr_latch = 0;
            ppu_2007_first_read = true;
            return value;
        }
        case 4: return ppu_latch = read_ppu_sprram(ppu->OAMADDR);
        case 7:
        {
            byte data;
            
            if (ppu->PPUADDR < 0x3F00) {
                data = ppu_latch = ppu_ram_read(ppu->PPUADDR);
            }
            else {
                data = ppu_ram_read(ppu->PPUADDR);
                ppu_latch = 0;
            }
            
            if (ppu_2007_first_read) {
                ppu_2007_first_read = false;
            }
            else {
                ppu->PPUADDR += ppu_vram_address_increment(ppu);
            }
            return data;
        }
        default:
            return 0xFF;
    }
    return 0;
}

static void ppu_write(void *opaque, hwaddr offset,
                             uint64_t data, unsigned size)
{
    PPUState *ppu = (PPUState *)opaque;
    offset &= 15;
    ppu_latch = data;
    ppu->PPUADDR &= 0x3FFF;
    switch(offset) {
        case 0: if (ppu->ready) ppu->PPUCTRL = data; break;
        case 1: if (ppu->ready) ppu->PPUMASK = data; break;
        case 3: ppu->OAMADDR = data; break;
        case 4: write_ppu_sprram(ppu->OAMADDR++, data); break;
        case 5:
        {
            if (ppu->scroll_received_x)
                ppu->PPUSCROLL_Y = data;
            else
                ppu->PPUSCROLL_X = data;

            ppu->scroll_received_x ^= 1;
            break;
        }
        case 6:
        {
            if (!ppu->ready)
                return;

            if (ppu->addr_received_high_byte)
                ppu->PPUADDR = (ppu_addr_latch << 8) + data;
            else
                ppu_addr_latch = data;

            ppu->addr_received_high_byte ^= 1;
            ppu_2007_first_read = true;
            break;
        }
        case 7:
        {
            if (ppu->PPUADDR > 0x1FFF || ppu->PPUADDR < 0x4000) {
                ppu_ram_write(ppu->PPUADDR ^ ppu->mirroring_xor, data);
                ppu_ram_write(ppu->PPUADDR, data);
            }
            else {
                ppu_ram_write(ppu->PPUADDR, data);
            }
            break;
        }
        case 8:
        {
            data &= 0x00FF;
            ppu->mirroring = data;
            ppu->mirroring_xor = 0x400 << data;
        }
    }
    ppu_latch = data;
}



// Rendering

PixelBuf bg, bbg, fg;

static bool ppu_shows_background_in_leftmost_8px(void)                  
{ 
    return common_bit_set(ppu_->PPUMASK, 1); 
}

static const word ppu_base_nametable_addresses[4] = { 0x2000, 0x2400, 0x2800, 0x2C00 };

static word ppu_base_nametable_address(void)                            
{ 
    return ppu_base_nametable_addresses[ppu_->PPUCTRL & 0x3];  
}

static word ppu_background_pattern_table_address(void)                  
{ 
    return common_bit_set(ppu_->PPUCTRL, 4) ? 0x1000 : 0x0000; 
}

static bool ppu_shows_sprites(void)                                     
{ 
    return common_bit_set(ppu_->PPUMASK, 4); 
}

static byte ppu_sprite_height(void)                                     
{ 
    return common_bit_set(ppu_->PPUCTRL, 5) ? 16 : 8;          
}

static void ppu_set_sprite_overflow(bool yesno)                     
{ 
    common_modify_bitb(&ppu_->PPUSTATUS, 5, yesno); 
}

static word ppu_sprite_pattern_table_address(void)                      
{ 
    return common_bit_set(ppu_->PPUCTRL, 3) ? 0x1000 : 0x0000; 
}

static    FILE *log_p;
static void ppu_draw_background_scanline(bool mirror)
{
    int tile_x;
    int pixel_x, pixel_y;
    for (tile_x = ppu_shows_background_in_leftmost_8px() ? 0 : 1; tile_x < 32; tile_x++) {
        // Skipping off-screen pixels
        if (((tile_x << 3) - ppu_->PPUSCROLL_X + (mirror ? 256 : 0)) > 256)
            continue;

        int tile_y = ppu_->scanline >> 3;
        int tile_index = ppu_ram_read(ppu_base_nametable_address() + tile_x + (tile_y << 5) + (mirror ? 0x400 : 0));
        word tile_address = ppu_background_pattern_table_address() + 16 * tile_index;

        int y_in_tile = ppu_->scanline & 0x7;
        byte l = ppu_ram_read(tile_address + y_in_tile);
        byte h = ppu_ram_read(tile_address + y_in_tile + 8);

        int x;
        for (x = 0; x < 8; x++) {
            byte color = ppu_l_h_addition_table[l][h][x];

            // Color 0 is transparent
            if (color != 0) {
                
                word attribute_address = (ppu_base_nametable_address() + (mirror ? 0x400 : 0) + 0x3C0 + (tile_x >> 2) + (ppu_->scanline >> 5) * 8);
                bool top = (ppu_->scanline % 32) < 16;
                bool left = (tile_x % 4 < 2);

                byte palette_attribute = ppu_ram_read(attribute_address);

                if (!top) {
                    palette_attribute >>= 4;
                }
                if (!left) {
                    palette_attribute >>= 2;
                }
                palette_attribute &= 3;

                word palette_address = 0x3F00 + (palette_attribute << 2);
                int idx = ppu_ram_read(palette_address + color);

                ppu_screen_background[(tile_x << 3) + x][ppu_->scanline] = color;

                pixel_x = (tile_x << 3) + x - ppu_->PPUSCROLL_X + (mirror ? 256 : 0);
                pixel_y = ppu_->scanline + 1;
                if (pixel_x >= 512 || pixel_x < 0 || pixel_y < 0|| pixel_y >= 480) {
                    continue;
                } 

                if (x==0) {
    // printf("tile_x %d, tile_y %d, tile_address 0x%x, l %d, h %d, attribute_address 0x%x, pixel_x %d, pixel_y %d, idx %d\n", 
    //             tile_x, tile_y, tile_address, l, h, attribute_address, pixel_x, pixel_y, idx);
    // printf("ppu state PPUCTRL 0x%x, PPUMASK 0x%x, PPUSTATUS 0x%x, PPUADDR 0x%x\n", ppu_->PPUCTRL, ppu_->PPUMASK, ppu_->PPUSTATUS, ppu_->PPUADDR);
                
                // fprintf(log_p, "tile_x %d, tile_y %d, tile_address 0x%x, l %d, h %d, attribute_address 0x%x, pixel_x %d, pixel_y %d, idx %d\n", 
                // tile_x, tile_y, tile_address, l, h, attribute_address, pixel_x, pixel_y, idx);
                // fprintf(log_p, "ppu state PPUCTRL 0x%x, PPUMASK 0x%x, PPUSTATUS 0x%x, PPUADDR 0x%x\n", ppu_->PPUCTRL, ppu_->PPUMASK, ppu_->PPUSTATUS, ppu_->PPUADDR);
                }
                
                pixbuf_add(bg, pixel_x, pixel_y, idx);
            }
        }
    }
}

static void ppu_draw_sprite_scanline(PPUState *ppu)
{
    int scanline_sprite_count = 0;
    int n;
    for (n = 0; n < 0x100; n += 4) {
        byte sprite_x = read_ppu_sprram(n + 3);
        byte sprite_y = read_ppu_sprram(n);

        // Skip if sprite not on scanline
        if (sprite_y > ppu->scanline || sprite_y + ppu_sprite_height() < ppu->scanline)
           continue;

        scanline_sprite_count++;

        // PPU can't render > 8 sprites
        if (scanline_sprite_count > 8) {
            ppu_set_sprite_overflow(true);
            // break;
        }

        bool vflip = read_ppu_sprram(n + 2) & 0x80;
        bool hflip = read_ppu_sprram(n + 2) & 0x40;

        word tile_address = ppu_sprite_pattern_table_address() + 16 * read_ppu_sprram(n + 1);
        int y_in_tile = ppu->scanline & 0x7;
        byte l = ppu_ram_read(tile_address + (vflip ? (7 - y_in_tile) : y_in_tile));
        byte h = ppu_ram_read(tile_address + (vflip ? (7 - y_in_tile) : y_in_tile) + 8);

        byte palette_attribute = read_ppu_sprram(n + 2) & 0x3;
        word palette_address = 0x3F10 + (palette_attribute << 2);
        int x;
        for (x = 0; x < 8; x++) {
            int color = hflip ? ppu_l_h_addition_flip_table[l][h][x] : ppu_l_h_addition_table[l][h][x];

            // Color 0 is transparent
            if (color != 0) {
                int screen_x = sprite_x + x;
                int idx = ppu_ram_read(palette_address + color);
                
                if (read_ppu_sprram(n + 2) & 0x20) {
                    pixbuf_add(bbg, screen_x, sprite_y + y_in_tile + 1, idx);
                }
                else {
                    pixbuf_add(fg, screen_x, sprite_y + y_in_tile + 1, idx);
                }

                // Checking sprite 0 hit
                if (ppu_shows_background(ppu) && !ppu_sprite_hit_occured && n == 0 && ppu_screen_background[screen_x][sprite_y + y_in_tile] == color) {
                    ppu_set_sprite_0_hit(true, ppu);
                    ppu_sprite_hit_occured = true;
                }
            }
        }
    }
}

/* Set background color. RGB value of c is defined in fce.h */
static void nes_set_bg_color(int c)
{
    address_space_write(&address_space_memory, 0x0C000000 + 0, MEMTXATTRS_UNSPECIFIED, &c, 4);
}

static void nes_flush_buf(PixelBuf *bg, uint32_t offset)
{
    int flush = 1;
    uint64_t addr = (uint64_t)bg;
    uint32_t bg_addr[2];
    bg_addr[0] = addr  & 0x0000ffffffff;
    bg_addr[1] = (addr & 0xffff00000000) >> 32;
    address_space_write(&address_space_memory, 0x0C000000 + 1 * 4, MEMTXATTRS_UNSPECIFIED, &bg_addr[0], 4);
    address_space_write(&address_space_memory, 0x0C000000 + 2 * 4, MEMTXATTRS_UNSPECIFIED, &bg_addr[1], 4);
    address_space_write(&address_space_memory, 0x0C000000 + offset * 4, MEMTXATTRS_UNSPECIFIED, &flush, 4);
}

static void nes_flush_bg_buf(PixelBuf *buf)
{
    nes_flush_buf(buf, 5);
}

static void nes_flush_fg_buf(PixelBuf *buf)
{
    nes_flush_buf(buf, 6);
}

static void nes_flush_bbg_buf(PixelBuf *buf)
{
    nes_flush_buf(buf, 4);
}

static void nes_flip_display(int c)
{
    address_space_write(&address_space_memory, 0x0C000000 + 3 * 4, MEMTXATTRS_UNSPECIFIED, &c, 4);
}

static void fce_update_screen(PPUState *ppu)
{
    // printf("bg %d\n", bg.size);

    int idx = ppu_ram_read(0x3F00);
    nes_set_bg_color(idx);

    if (ppu_shows_sprites())
        nes_flush_bbg_buf(&bbg);

    if (ppu_shows_background(ppu)) {
        // unsigned int bg_p = (unsigned int)(&bg);
        // printf("bg addr %p\n", &bg);
        nes_flush_bg_buf(&bg);
    }

    if (ppu_shows_sprites()) {
        nes_flush_fg_buf(&fg);
    }

    nes_flip_display(1);

    pixbuf_clean(bbg);
    pixbuf_clean(bg);
    pixbuf_clean(fg);

}

static void ppu_cycle(void *opaque)
{
    PPUState *ppu = (PPUState *)opaque;
    if (!ppu->ready)
        ppu->ready = true;

    ppu->scanline++;
    if (ppu_shows_background(ppu)) {
        ppu_draw_background_scanline(false);
        ppu_draw_background_scanline(true);
    }
    
    if (ppu_shows_sprites()) ppu_draw_sprite_scanline(ppu);

    if (ppu->scanline == 241) {
        ppu_set_in_vblank(true);
        ppu_set_sprite_0_hit(false, ppu);
        if (ppu_generates_nmi(ppu)) {
            qemu_irq_pulse(ppu->irq);
        }
    }
    else if (ppu->scanline == 262) {
        ppu->scanline = -1;
        ppu_sprite_hit_occured = false;
        ppu_set_in_vblank(false);
        fce_update_screen(ppu);
    }
    timer_mod_ns(ppu->ppu_cycle_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 30000);
    // timer_mod_ns(ppu->ppu_cycle_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME) + 1000);

    // timer_mod(ppu->ppu_cycle_timer, qemu_clock_get_ms(QEMU_CLOCK_REALTIME) + 1);

}

static const struct MemoryRegionOps ppu_ops = {
    .read = ppu_read,
    .write = ppu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ppu_realize(DeviceState *dev, Error **errp)
{
    PPUState *ppu = NES_PPU(dev);
    ppu->PPUCTRL = ppu->PPUMASK = ppu->PPUSTATUS = ppu->OAMADDR = ppu->PPUSCROLL_X = ppu->PPUSCROLL_Y = ppu->PPUADDR = 0;
    ppu->PPUSTATUS |= 0xA0;
    ppu->PPUDATA = 0;
    ppu_2007_first_read = true;
    log_p = fopen("ppu_log.txt", "w+");
    if (log_p == NULL)
    {
        fprintf(stderr, "Open log file failed.\n");
        exit(1);
    }
    // Initializing low-high byte-pairs for pattern tables
    int h, l, x;
    for (h = 0; h < 0x100; h++) {
        for (l = 0; l < 0x100; l++) {
            for (x = 0; x < 8; x++) {
                ppu_l_h_addition_table[l][h][x] = (((h >> (7 - x)) & 1) << 1) | ((l >> (7 - x)) & 1);
                ppu_l_h_addition_flip_table[l][h][x] = (((h >> x) & 1) << 1) | ((l >> x) & 1);
            }
        }
    }

    ppu->ppu_cycle_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, ppu_cycle, ppu);
    // int64_t    now        = qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL);
    // timer_mod(ppu->ppu_cycle_timer, now + 30);
    timer_mod_ns(ppu->ppu_cycle_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5000);

    // ppu->ppu_cycle_timer = timer_new_ns(QEMU_CLOCK_REALTIME, ppu_cycle, ppu);
    // timer_mod(ppu->ppu_cycle_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME));

    // ppu->ppu_cycle_timer = timer_new_ns(QEMU_CLOCK_REALTIME, ppu_cycle, ppu);
    // timer_mod_ns(ppu->ppu_cycle_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME));

    ppu_ = ppu;
}

static void ppu_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PPUState *s = NES_PPU(obj);

    memory_region_init_io(&s->iomem, obj, &ppu_ops, s,
                          TYPE_NES_PPU, 0x2000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void ppu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = ppu_realize;
    dc->desc = "xiaobawang nes ppu";
}
static const TypeInfo nes_ppu_info = {
    .name           = TYPE_NES_PPU,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(PPUState),
    .instance_init  = ppu_init,
    .class_init     = ppu_class_init,
};

static void nes_ppu_register_types(void)
{
    type_register_static(&nes_ppu_info);
}

type_init(nes_ppu_register_types)
