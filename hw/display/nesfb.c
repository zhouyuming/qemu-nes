/*
 * NeXT Cube/Station Framebuffer Emulation
 *
 * Copyright (c) 2011 Bryce Lanham
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "ui/console.h"
#include "hw/loader.h"
#include "framebuffer.h"
#include "ui/pixel_ops.h"
#include "hw/m68k/next-cube.h"
#include "qom/object.h"
#include "ui/input.h"
#include "hw/nes6502/nesppu.h"

#define TYPE_NES6502FB "nes6502-fb"
OBJECT_DECLARE_SIMPLE_TYPE(NESFbState, NES6502FB)

struct NESFbState {
    SysBusDevice parent_obj;

    MemoryRegion fb_mr;
    MemoryRegion mr;
    MemoryRegionSection fbsection;
    QemuConsole *con;

    uint32_t cols;
    uint32_t rows;
    int invalidate;
    uint16_t shift;

    int bg_color_index;
    uint64_t bg_addr;
    uint64_t buf_addr;
    PixelBuf bg;
    PixelBuf bbg;
    PixelBuf fg;

    int crt_ready;
};

// Palette

typedef struct __pal {
	int r;
	int g;
	int b;
} pal;
#808080
static const pal palette[64] = {
	{ 0x80, 0x80, 0x80 },
	{ 0x00, 0x00, 0xBB },
	{ 0x37, 0x00, 0xBF },
	{ 0x84, 0x00, 0xA6 },
	{ 0xBB, 0x00, 0x6A },
	{ 0xB7, 0x00, 0x1E },
	{ 0xB3, 0x00, 0x00 },
	{ 0x91, 0x26, 0x00 },
	{ 0x7B, 0x2B, 0x00 },
	{ 0x00, 0x3E, 0x00 },
	{ 0x00, 0x48, 0x0D },
	{ 0x00, 0x3C, 0x22 },
	{ 0x00, 0x2F, 0x66 },
	{ 0x00, 0x00, 0x00 },
	{ 0x05, 0x05, 0x05 },
	{ 0x05, 0x05, 0x05 },
	{ 0xC8, 0xC8, 0xC8 },
	{ 0x00, 0x59, 0xFF },
	{ 0x44, 0x3C, 0xFF },
	{ 0xB7, 0x33, 0xCC },
	{ 0xFF, 0x33, 0xAA },
	{ 0xFF, 0x37, 0x5E },
	{ 0xFF, 0x37, 0x1A },
	{ 0xD5, 0x4B, 0x00 },
	{ 0xC4, 0x62, 0x00 },
	{ 0x3C, 0x7B, 0x00 },
	{ 0x1E, 0x84, 0x15 },
	{ 0x00, 0x95, 0x66 },
	{ 0x00, 0x84, 0xC4 },
	{ 0x11, 0x11, 0x11 },
	{ 0x09, 0x09, 0x09 },
	{ 0x09, 0x09, 0x09 },
	{ 0xFF, 0xFF, 0xFF },
	{ 0x00, 0x95, 0xFF },
	{ 0x6F, 0x84, 0xFF },
	{ 0xD5, 0x6F, 0xFF },
	{ 0xFF, 0x77, 0xCC },
	{ 0xFF, 0x6F, 0x99 },
	{ 0xFF, 0x7B, 0x59 },
	{ 0xFF, 0x91, 0x5F },
	{ 0xFF, 0xA2, 0x33 },
	{ 0xA6, 0xBF, 0x00 },
	{ 0x51, 0xD9, 0x6A },
	{ 0x4D, 0xD5, 0xAE },
	{ 0x00, 0xD9, 0xFF },
	{ 0x66, 0x66, 0x66 },
	{ 0x0D, 0x0D, 0x0D },
	{ 0x0D, 0x0D, 0x0D },
	{ 0xFF, 0xFF, 0xFF },
	{ 0x84, 0xBF, 0xFF },
	{ 0xBB, 0xBB, 0xFF },
	{ 0xD0, 0xBB, 0xFF },
	{ 0xFF, 0xBF, 0xEA },
	{ 0xFF, 0xBF, 0xCC },
	{ 0xFF, 0xC4, 0xB7 },
	{ 0xFF, 0xCC, 0xAE },
	{ 0xFF, 0xD9, 0xA2 },
	{ 0xCC, 0xE1, 0x99 },
	{ 0xAE, 0xEE, 0xB7 },
	{ 0xAA, 0xF7, 0xEE },
	{ 0xB3, 0xEE, 0xFF },
	{ 0xDD, 0xDD, 0xDD },
	{ 0x11, 0x11, 0x11 },
	{ 0x11, 0x11, 0x11 }
};

typedef struct ALLEGRO_VERTEX ALLEGRO_VERTEX;

struct ALLEGRO_VERTEX {
    uint32_t x, y, z;
    uint32_t color;
};

static int color_idx_ = -1;
static int bg_size_ = -1;
static unsigned int rgb;
static void nextfb_draw_line(void *opaque, uint8_t *d, const uint8_t *s,
                             int width, int pitch)
{
    NESFbState *nfbstate = NES6502FB(opaque);
    uint32_t *e;
    e = (uint32_t *)d;

    // if (color_idx_ != nfbstate->bg_color_index || nfbstate->bg.size == 0) {
    // if (color_idx_ != nfbstate->bg_color_index && nfbstate->bg.size != bg_size_) {
        for (int row = 0; row < nfbstate->rows; row++) {
            for (int i = 0; i < nfbstate->cols; i++) {
                e[row * nfbstate->cols + i] = rgb;
            }
        }
        color_idx_ = nfbstate->bg_color_index;
        bg_size_ = nfbstate->bg.size;
    // }

    // if (nfbstate->crt_ready == 0) {
    //     return;
    // }

    for (int i = 0; i < nfbstate->bbg.size; i++) {
        Pixel *p = &(nfbstate->bbg.buf[i]);
        if (p->x >= 512 || p->x < 0 || p->y < 0|| p->y >= 480) {
            continue;
        } 
        // printf("i %d, p->x %d,  p->y %d, color 0x%x\n", i, p->x, p->y, p->c);
        // e[p->y * nfbstate->cols + p->x] = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2) * nfbstate->cols + (p->x * 2)]         = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2) * nfbstate->cols + (p->x * 2 + 1)]     = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2 + 1) * nfbstate->cols + (p->x * 2)]     = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2 + 1) * nfbstate->cols + (p->x * 2 + 1)] = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
    }

    for (int i = 0; i < nfbstate->bg.size; i++) {
        Pixel *p = &(nfbstate->bg.buf[i]);
        if (p->x >= 512 || p->x < 0 || p->y < 0|| p->y >= 480) {
            continue;
        } 
        // printf("i %d, p->x %d,  p->y %d, color 0x%x\n", i, p->x, p->y, p->c);
        // e[p->y * nfbstate->cols + p->x] = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2) * nfbstate->cols + (p->x * 2)]         = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2) * nfbstate->cols + (p->x * 2 + 1)]     = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2 + 1) * nfbstate->cols + (p->x * 2)]     = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2 + 1) * nfbstate->cols + (p->x * 2 + 1)] = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
    }

    for (int i = 0; i < nfbstate->fg.size; i++) {
        Pixel *p = &(nfbstate->fg.buf[i]);
        if (p->x >= 512 || p->x < 0 || p->y < 0|| p->y >= 480) {
            continue;
        } 
        // printf("i %d, p->x %d,  p->y %d, color 0x%x\n", i, p->x, p->y, p->c);
        // e[p->y * nfbstate->cols + p->x] = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2) * nfbstate->cols + (p->x * 2)]         = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2) * nfbstate->cols + (p->x * 2 + 1)]     = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2 + 1) * nfbstate->cols + (p->x * 2)]     = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
        e[(p->y * 2 + 1) * nfbstate->cols + (p->x * 2 + 1)] = rgb_to_pixel32(palette[p->c].r, palette[p->c].g, palette[p->c].b);
    }

    nfbstate->crt_ready = 0;
}

static void nextfb_update(void *opaque)
{
    NESFbState *s = NES6502FB(opaque);
    int dest_width = 4;
    int src_width;
    int first = 0;
    int last  = 0;
    DisplaySurface *surface = qemu_console_surface(s->con);
	// uint8_t *dest = surface_data(surface);
    src_width = s->cols ;
    dest_width = s->cols;
	static int inited = 0;
    if (s->invalidate) {
        framebuffer_update_memory_section(&s->fbsection, &s->fb_mr, 0,
                                          s->cols, src_width);
        s->invalidate = 0;
    }

	if (inited) {
        framebuffer_update_display_full_frame(surface, &s->fbsection, s->cols, s->rows,
                                src_width, dest_width, 0, 1, nextfb_draw_line,
                                s, &first, &last);
        dpy_gfx_update(s->con, 0, 0, s->cols, s->rows);
    } else {
        dpy_gfx_update(s->con, 0, 0, s->cols, s->rows);
        inited = 1;
    }

    pixbuf_clean(s->bbg);
    pixbuf_clean(s->bg);
    pixbuf_clean(s->fg);

}

static void nextfb_invalidate(void *opaque)
{
    NESFbState *s = NES6502FB(opaque);
    s->invalidate = 1;
}

static const GraphicHwOps nextfb_ops = {
    .invalidate  = nextfb_invalidate,
    // .gfx_update  = nextfb_update,
};

#define NESFB_BG_COLOR 0
#define NESFB_SHOW_BACKGROUND_L 1
#define NESFB_SHOW_BACKGROUND_H 2
#define NESFB_FLIP_DISPLAY 3
#define NESFB_SHOW_BBG 4
#define NESFB_SHOW_BG 5
#define NESFB_SHOW_FG 6

static uint64_t nesfb_read(void *opaque, hwaddr offset, unsigned size)
{
    // switch (offset) {
    // case 1:

    // default:
    //     qemu_log_mask(0, "%s: Bad offset %"HWADDR_PRIx"\n", __func__, offset);
    //     return 0;
    // }
    return 0;
}

static void nes_flush_buf(PixelBuf *buf)
{
}


static void nesfb_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    NESFbState *s = (NESFbState *)opaque;
    static int write_addr = 0;
    static PixelBuf *addr = NULL;
    switch (offset >> 2) {
        case NESFB_BG_COLOR:
            // assert(value <= 64);
            if (value >=64) {
                value = 63;
            }
            // printf("NESFB_BG_COLOR %ld\n", value);
            s->bg_color_index = value;
            rgb = rgb_to_pixel32(palette[value].r, palette[value].g, palette[value].b);
            // nes_set_bg_color(value);
            break;
        case NESFB_SHOW_BACKGROUND_L:
            s->buf_addr = value;
            break;
        case NESFB_SHOW_BACKGROUND_H:
            addr = (PixelBuf *)((value << 32) | s->buf_addr);
            write_addr = 1;
            break;
        case NESFB_SHOW_BBG:
            if (write_addr) {
                memcpy(&s->bbg, addr, sizeof(PixelBuf));
                nes_flush_buf(&s->bbg);
                write_addr = 0;
            }
            break;
        case NESFB_SHOW_BG:
            if (write_addr) {
                memcpy(&s->bg, addr, sizeof(PixelBuf));
                nes_flush_buf(&s->bg);
                write_addr = 0;
            }
            break;
        case NESFB_SHOW_FG:
            if (write_addr) {
                memcpy(&s->fg, addr, sizeof(PixelBuf));
                nes_flush_buf(&s->fg);
                write_addr = 0;
            }
            break;
        case NESFB_FLIP_DISPLAY:
            s->crt_ready = 1;
            nextfb_update(s);
            break;
        default:
            return;
    }
}

static const MemoryRegionOps nesfb_ops = {
    .read = nesfb_read,
    .write = nesfb_write,
};

static void nextfb_realize(DeviceState *dev, Error **errp)
{
    NESFbState *s = NES6502FB(dev);

    memory_region_init_ram(&s->fb_mr, OBJECT(dev), "nes-video", 0x1CB100,
                           &error_fatal);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->fb_mr);

    memory_region_init_io(&s->mr, OBJECT(dev), &nesfb_ops, s, "nesfb.io", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mr);

    s->invalidate = 1;
    s->cols = 256*2;
    s->rows = 240*2;
    s->bg_color_index = 15;
    s->crt_ready = 0;

    s->con = graphic_console_init(dev, 0, &nextfb_ops, s);
    // DisplaySurface *surface = qemu_console_surface(s->con);
    // surface->format = PIXMAN_r8g8b8;
    qemu_console_resize(s->con, s->cols, s->rows);
    // qemu_add_kbd_event_handler(nextkbd_event, s);
}

static void nextfb_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
    dc->realize = nextfb_realize;

    /* Note: This device does not have any state that we have to reset or migrate */
}

static const TypeInfo nextfb_info = {
    .name          = TYPE_NES6502FB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(NESFbState),
    .class_init    = nextfb_class_init,
};

static void nextfb_register_types(void)
{
    type_register_static(&nextfb_info);
}

type_init(nextfb_register_types)
