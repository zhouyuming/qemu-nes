#ifndef NES_PPU_H
#define NES_PPU_H

#include "qemu/osdep.h"
#include "hw/char/imx_serial.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

#define TYPE_NES_PPU "nes-ppu" 
OBJECT_DECLARE_SIMPLE_TYPE(PPUState, NES_PPU)
struct PPUState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;

    byte PPUCTRL;   // $2000 write only
    byte PPUMASK;   // $2001 write only
    byte PPUSTATUS; // $2002 read only
    byte OAMADDR;   // $2003 write only
    byte OAMDATA;   // $2004
	word PPUSCROLL;
    byte PPUSCROLL_X, PPUSCROLL_Y; // $2005 write only x2
    word PPUADDR;   // $2006 write only x2
    word PPUDATA;   // $2007

    QEMUTimer *ppu_cycle_timer;
    qemu_irq irq;

    bool scroll_received_x;
    bool addr_received_high_byte;
    bool ready;

    int mirroring, mirroring_xor;

    int x, scanline;
};
// PPU Constants

struct Pixel {
    int x, y; // (x, y) coordinate
    int c; // RGB value of colors can be found in fce.h
};
typedef struct Pixel Pixel;

/* A buffer of pixels */
struct PixelBuf {
	Pixel buf[264 * 264];
	int size;
};
typedef struct PixelBuf PixelBuf;


// clear a pixel buffer
#define pixbuf_clean(bf) \
	do { \
		(bf).size = 0; \
	} while (0)


#define FPS 60
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

// add a pending pixel into a buffer
#define pixbuf_add(bf, xa, ya, ca) \
	do { \
		if ((xa) < SCREEN_WIDTH && (ya) < SCREEN_HEIGHT) { \
			(bf).buf[(bf).size].x = (xa); \
			(bf).buf[(bf).size].y = (ya); \
			(bf).buf[(bf).size].c = (ca); \
			(bf).size++; \
		} \
	} while (0)

#endif
