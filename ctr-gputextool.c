#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "utils.h"

#include "lodepng/lodepng.h"

//Build with: gcc -o ctr-gputextool ctr-gputextool.c utils.c lodepng/lodepng.c -DLODEPNG_NO_COMPILE_CPP

typedef struct {
	u8 magicnum[4];//0x4d494c43 "CLIM"
	u8 bom[2];
	u8 headerlen[2];
	u8 revision[4];
	u8 filesize[4];
	u8 total_sections[4];//Assuming that's what this is since this has same structure as bcylt/bclan.
} bclim_struct;

typedef struct {
	u8 magicnum[4];//0x67616d69 "imag"
	u8 headerlen[4];//Relative to this field, it seems.
	u8 width[2];
	u8 height[2];
	u8 format[4];
	u8 imagesize[4];//Total size of the data prior to these two structures.
} imag_struct;

// stolen shamelessly from 3ds_hb_menu
static const u8 tile_order[] =
{
	0, 1, 8, 9, 2, 3, 10, 11, 16, 17, 24, 25, 18, 19, 26, 27,
	4, 5, 12, 13, 6, 7, 14, 15, 20, 21, 28, 29, 22, 23, 30, 31,
	32, 33, 40, 41, 34, 35, 42, 43, 48, 49, 56, 57, 50, 51, 58, 59,
	36, 37, 44, 45, 38, 39, 46, 47, 52, 53, 60, 61, 54, 55, 62, 63
};

int main(int argc, char **argv)
{
	int ret=0;
	int container_type = 0;
	unsigned width = 0, height = 0, x, y, tilex, tiley, tmpx, tmpy, tilepos;
	unsigned char *pngimagebuf = NULL;
	unsigned char *gpuimagebuf = NULL;
	u32 gpubufsize;
	u32 pos, pos2;
	bclim_struct *bclim;
	imag_struct *imag;
	FILE *f;

	if(argc<3)
	{
		printf("ctr-gputextool by yellows8\n");
		printf("Convert a PNG to a raw 3DS GPU texture, with a (B)CLIM struct optionally added. Which form to use is determined by the file-extension. Only the A4 color-format with tiling is supported right now.\n");
		printf("Usage:\n");
		printf("ctr-gputextool <inputpath.png> <outputpath{.bclim}>\n");
		return 0;
	}

	pos = strlen(argv[2]);
	if(pos>6)
	{
		if(strcmp(&argv[2][pos-6], ".bclim")==0)container_type = 1;
	}

	ret = lodepng_decode32_file(&pngimagebuf, &width, &height, argv[1]);
	if(ret!=0)
	{
		printf("lodepng returned an error: %s\n", lodepng_error_text(ret));
		return ret;
	}

	if((width & 7) || (height & 7))
	{
		printf("The input image width/height must be multiples of 8.\n");
		free(pngimagebuf);
		return 1;
	}

	gpubufsize = ((width*height)/2) + (0x28*container_type);

	gpuimagebuf = malloc(gpubufsize);
	if(gpuimagebuf==NULL)
	{
		printf("Failed to allocate memory for gpuimagebuf.\n");
		free(pngimagebuf);
		return 2;
	}
	memset(gpuimagebuf, 0, gpubufsize);

	pos = 0;

	for(y=0; y<height; y+=8)
	{
		for(x=0; x<width; x+=8)
		{
			for(tilepos=0; tilepos<64; tilepos++)
			{
				tmpx = (tile_order[tilepos] & 0x7);
				tmpy = (tile_order[tilepos] >> 3);

				tmpx+= x;
				tmpy+= y;

				pos2 = ((tmpx + tmpy*width) * 4);
				gpuimagebuf[pos] |= (pngimagebuf[pos2 + 3] >> 4) << (4 * (tilepos & 1));
				if(tilepos & 1)pos++;
			}
		}
	}

	if(container_type)
	{
		bclim = (bclim_struct*)&gpuimagebuf[gpubufsize - 0x28];
		imag = (imag_struct*)&gpuimagebuf[gpubufsize - 0x14];

		putle32(bclim->magicnum, 0x4d494c43);
		putle16(bclim->bom, 0xfeff);
		putle16(bclim->headerlen, 0x14);
		putle32(bclim->revision, 0x2020000);
		putle32(bclim->filesize, gpubufsize);
		putle32(bclim->total_sections, 0x1);

		putle32(imag->magicnum, 0x67616d69);
		putle32(imag->headerlen, 0x10);
		putle16(imag->width, width);
		putle16(imag->height, height);
		putle32(imag->format, 0xd);//A4
		putle32(imag->imagesize, gpubufsize-0x28);
	}

	f = fopen(argv[2], "wb");
	if(f)
	{
		fwrite(gpuimagebuf, 1, gpubufsize, f);
		fclose(f);
	}
	else
	{
		printf("Failed to open the output file for writing.\n");
		ret = 3;
	}

	free(pngimagebuf);
	free(gpuimagebuf);

	return 0;
}

