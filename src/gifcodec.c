/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*-
 *
 * gifcodec.c : Contains function definitions for encoding decoding gif images
 *
 * Copyright (C) Novell, Inc. 2003-2004.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
 * and associated documentation files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial 
 * portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT 
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *  	Sanjay Gupta (gsanjay@novell.com)
 *	Vladimir Vukicevic (vladimir@pobox.com)
 *
 * Copyright (C) Novell, Inc. 2003-2004.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "gdipImage.h"
#include "gifcodec.h"

#ifdef HAVE_LIBUNGIF

#include <gif_lib.h>


/* Codecinfo related data*/
static ImageCodecInfo gif_codec;
static const WCHAR gif_codecname[] = {'B', 'u', 'i','l', 't', '-','i', 'n', ' ', 'G', 'I', 'F', ' ', 'C', 
        'o', 'd', 'e', 'c',   0}; /* Built-in GIF Codec */
static const WCHAR gif_extension[] = {'*', '.', 'G', 'I', 'F',0}; /* *.GIF */
static const WCHAR gif_mimetype[] = {'i', 'm', 'a','g', 'e', '/', 'g', 'i', 'f', 0}; /* image/gif */
static const WCHAR gif_format[] = {'G', 'I', 'F', 0}; /* GIF */

ImageCodecInfo *
gdip_getcodecinfo_gif ()
{                        
        gif_codec.Clsid = (CLSID) { 0x557cf402, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x0, 0x0, 0xf8, 0x1e, 0xf3, 0x2e } };
        gif_codec.FormatID = gdip_gif_image_format_guid;
        gif_codec.CodecName = (const WCHAR*) gif_codecname;
        gif_codec.DllName = NULL;
        gif_codec.FormatDescription = (const WCHAR*) gif_format;
        gif_codec.FilenameExtension = (const WCHAR*) gif_extension;
        gif_codec.MimeType = (const WCHAR*) gif_mimetype;
        gif_codec.Flags = Encoder | Decoder | SupportBitmap | Builtin;
        gif_codec.Version = 1;
        gif_codec.SigCount = 0;
        gif_codec.SigSize = 0;
        gif_codec.SigPattern = 0;
        gif_codec.SigMask = 0;

        return &gif_codec;
}

GpStatus 
gdip_load_gif_image_from_file (FILE *fp, GpImage **image)
{
    GifFileType *gif = NULL;
    GifRecordType *rectype = NULL;
    int res;

    GpBitmap *img = NULL;
    ColorMapObject *pal = NULL;
    guchar *pixels = NULL;
    guchar *readptr, *writeptr;
    int i, j, l;
	int imgCount;
	int extBlockCount;
	bool pageDimensionFound = FALSE;
	bool timeDimensionFound = FALSE;
	int pageDimensionCount = 0;
	int timeDimensionCount = 0;

    gif = DGifOpenFileHandle (fileno (fp));
    if (gif == NULL)
        goto error;

    res = DGifSlurp (gif);
    if (res != GIF_OK)
        goto error;

    img = gdip_bitmap_new ();
    img->image.type = imageBitmap;
    img->image.graphics = 0;
    img->image.width = gif->SWidth;
    img->image.height = gif->SHeight;

	/* ImageCount member of GifFileType structure gives info about the total no of frames
	 * present in the image. 
	 * I am assuming that if there is an ExtensionBlock strucutre present in SavedImage structure 
	 * which has its Function member set to value 249 (Graphic Control Block) 
	 * then only its an animated gif.
	 */
	imgCount = gif->ImageCount;
	for (i=0; i<imgCount; i++) {
		SavedImage si = gif->SavedImages[i];
		extBlockCount = si.ExtensionBlockCount;
		for (l =0; l<extBlockCount; l++) {
			ExtensionBlock eb = si.ExtensionBlocks[l];
			if (eb.Function == 249){
				if (!timeDimensionFound)
					timeDimensionFound = TRUE;
				timeDimensionCount++;
				break; /*there can be only one Graphic Control Extension before ImageData*/
			}			
		}
	}
	
	if (timeDimensionCount < imgCount){
		pageDimensionFound = TRUE;
		pageDimensionCount = imgCount - timeDimensionCount;
	}
		
	if (pageDimensionFound && timeDimensionFound){
		img->image.frameDimensionCount = 2;
		img->image.frameDimensionList = (FrameDimensionInfo *) GdipAlloc (sizeof (FrameDimensionInfo)*2);
		img->image.frameDimensionList[0].count = pageDimensionCount;
		memcpy (&(img->image.frameDimensionList[0].frameDimension), &gdip_image_frameDimension_page_guid, sizeof (CLSID));
		img->image.frameDimensionList[1].count = timeDimensionCount;
		memcpy (&(img->image.frameDimensionList[1].frameDimension), &gdip_image_frameDimension_time_guid, sizeof (CLSID));
	} else if (pageDimensionFound) {
		img->image.frameDimensionCount = 1;
		img->image.frameDimensionList = (FrameDimensionInfo *) GdipAlloc (sizeof (FrameDimensionInfo));
		img->image.frameDimensionList[0].count = pageDimensionCount;
		memcpy (&(img->image.frameDimensionList[0].frameDimension), &gdip_image_frameDimension_page_guid, sizeof (CLSID));
	} else if (timeDimensionFound) {
		img->image.frameDimensionCount = 1;
		img->image.frameDimensionList = (FrameDimensionInfo *) GdipAlloc (sizeof (FrameDimensionInfo));
		img->image.frameDimensionList[0].count = timeDimensionCount;
		memcpy (&(img->image.frameDimensionList[0].frameDimension), &gdip_image_frameDimension_time_guid, sizeof (CLSID));
	}

    /* Note that Cairo/libpixman does not have support for indexed
     * images, so we expand gifs out to 32bpp argb.
     * This is unfortunate.
     */
#if 0
    /* Copy the palette over, if there is one */
    if (gif->SColorMap != NULL) {
        ColorPalette *pal = g_malloc (sizeof(ColorPalette) + sizeof(ARGB) * gif->SColorMap->ColorCount);
        pal->Flags = 0;
        pal->Count = gif->SColorMap->ColorCount;
        for (i = 0; i < gif->SColorMap->ColorCount; i++) {
            pal->Entries[i] = MAKE_ARGB_RGB(gif->SColorMap->Colors[i].Red,
                                            gif->SColorMap->Colors[i].Green,
                                            gif->SColorMap->Colors[i].Blue);
        }

        img->image.palette = pal;
    }
    img->image.pixFormat = Format8bppIndexed;
#endif

    pal = gif->SColorMap;

    img->image.pixFormat = Format32bppArgb;

    img->cairo_format = CAIRO_FORMAT_ARGB32;
    img->data.PixelFormat = img->image.pixFormat;
    img->data.Width = img->image.width;
    img->data.Height = img->image.height;
    img->data.Stride = img->data.Width * 4;

    pixels = GdipAlloc (img->data.Stride * img->data.Height);

    readptr = gif->SavedImages->RasterBits;
    writeptr = pixels;
    for (i = 0; i < img->image.width * img->image.height; i++) {
        guchar pix = *readptr++;
        if (pal) {
            *writeptr++ = pal->Colors[pix].Blue;
            *writeptr++ = pal->Colors[pix].Green;
            *writeptr++ = pal->Colors[pix].Red;
        } else {
            *writeptr++ = pix;
            *writeptr++ = pix;
            *writeptr++ = pix;
        }
        *writeptr++ = 255;  /* A */
    }

    img->data.Scan0 = pixels;
    img->data.Reserved = GBD_OWN_SCAN0;

    img->image.surface = cairo_surface_create_for_image (pixels, img->cairo_format,
                                                   img->image.width, img->image.height,
                                                   img->data.Stride);
    img->image.imageFlags =
        ImageFlagsReadOnly |
        ImageFlagsHasRealPixelSize |
        ImageFlagsColorSpaceRGB;
    img->image.horizontalResolution = 0;
    img->image.verticalResolution = 0;
    img->image.propItems = NULL;
    img->image.palette = NULL;

    *image = (GpImage *) img;
    return Ok;
  error:	
    if (pixels)
        GdipFree (pixels);
    if (img)
        gdip_bitmap_dispose (img);
    if (gif)
        DGifCloseFile (gif);
    *image = NULL;
    return InvalidParameter;
}

GpStatus 
gdip_save_gif_image_to_file (FILE *fp, GpImage *image)
{
    return NotImplemented;
}

#else

/* No libgif */

ImageCodecInfo *
gdip_getcodecinfo_gif ()
{
        return NULL;
}

GpStatus 
gdip_load_gif_image_from_file (FILE *fp, GpImage **image)
{
    *image = NULL;
    return NotImplemented;
}

GpStatus 
gdip_save_gif_image_to_file (FILE *fp, GpImage *image)
{
    return NotImplemented;
}


#endif
