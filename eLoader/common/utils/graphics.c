#include <common/utils/graphics.h>
#include <common/utils.h>
#include <common/debug.h>
#include <common/lib.h>

#define IS_ALPHA(color) (((color)&0xff000000)==0xff000000?0:1)
#define FRAMEBUFFER_SIZE (PSP_LINE_SIZE*SCREEN_HEIGHT*4)
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

Color* g_vram_base = (Color*) (0x40000000 | 0x04000000);

typedef union
{
	int rgba;
	struct
	{
		char r;
		char g;
		char b;
		char a;
	} c;
} color_t;

extern u8 msx[];

Color* getVramDrawBuffer()
{
	Color* vram = (Color*) g_vram_base;
	return vram;
}

Color* getVramDisplayBuffer()
{
	Color* vram = (Color*) g_vram_base;
	return vram;
}

void putPixelScreen(Color color, int x, int y)
{
	Color* vram = getVramDrawBuffer();
	vram[PSP_LINE_SIZE * y + x] = color;
}

Color getPixelScreen(int x, int y)
{
	Color* vram = getVramDrawBuffer();
	return vram[PSP_LINE_SIZE * y + x];
}

Color getPixelImage(int x, int y, Image* image)
{
	return image->data[x + y * image->textureWidth];
}

int gY = 0;

void SetColor(int col)
{
    gY = 0;
	int i;
	color_t *pixel = (color_t *)0x44000000;
	for(i = 0; i < 512*272; i++) {
		pixel->rgba = col;
		pixel++;
	}
}

void cls()
{
    sceDisplaySetFrameBuf((void *)0x44000000, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, 1);
    SetColor(0);
	gY = 0;
}

void printTextScreen(int x, int y, const char * text, u32 color)
{
	int c, i, j, l;
	u8 *font;
	Color *vram_ptr;
	Color *vram;

	for (c = 0; c < strlen(text); c++) {
		if (x < 0 || x + 8 > SCREEN_WIDTH || y < 0 || y + 8 > SCREEN_HEIGHT) break;
		char ch = text[c];
		vram = getVramDrawBuffer() + x + y * PSP_LINE_SIZE;

		font = &msx[ (int)ch * 8];
		for (i = l = 0; i < 8; i++, l += 8, font++) {
			vram_ptr  = vram;
			for (j = 0; j < 8; j++) {
				if ((*font & (128 >> j))) *vram_ptr = color;
				vram_ptr++;
			}
			vram += PSP_LINE_SIZE;
		}
		x += 8;
	}
}

void puts_scr_color(const char * text, u32 color)
{
	if (gY > 272)
	{
    	SetColor(0);
  	}

  	printTextScreen(0, gY, text, color);
  	gY += 12;

    LOGSTR0(text);
	LOGSTR0("\n");
}


void puts_scr(const char * text)
{
  	puts_scr_color(text, 0x00FFFFFF);
}




void PRTSTR0(const char* A)
{
  puts_scr(A);
}

void PRTSTR1(const char* A, unsigned long B)
{
  char buff[512];
  mysprintf1(buff, A, (unsigned long)B);
  puts_scr(buff);
}

void PRTSTR2(const char* A, unsigned long B, unsigned long C)
{
  char buff[512];
  mysprintf2(buff, A, (unsigned long)B, (unsigned long)C);
  puts_scr(buff);
}

void PRTSTR3(const char* A, unsigned long B, unsigned long C, unsigned long D)
{
  char buff[512];
  mysprintf3(buff, A, (unsigned long)B, (unsigned long)C, (unsigned long)D);
  puts_scr(buff);
}

void PRTSTR4(const char* A, unsigned long B, unsigned long C, unsigned long D, unsigned long E)
{
  char buff[512];
  mysprintf4 (buff, A, (unsigned long)B, (unsigned long)C, (unsigned long)D,  (unsigned long)E);
  puts_scr(buff);
}

void PRTSTR8(const char* A, unsigned long B, unsigned long C, unsigned long D, unsigned long E, unsigned long F, unsigned long G, unsigned long H, unsigned long I)
{
  char buff[512];
  mysprintf8 (buff, A, (unsigned long)B, (unsigned long)C, (unsigned long)D,  (unsigned long)E, (unsigned long)F, (unsigned long)G, (unsigned long)H,  (unsigned long)I);
  puts_scr(buff);
}