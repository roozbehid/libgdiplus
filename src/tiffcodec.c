/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*-
 *
 * tiffcodec.c : Contains function definitions for encoding decoding tiff images
 *
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
#include <byteswap.h>
#include "tiffcodec.h"

#ifdef HAVE_LIBTIFF

/* Thankfully, libtiff sucks far less than libjpeg */
#include <tiffio.h>


/* Codecinfo related data*/
static ImageCodecInfo tiff_codec;
static const WCHAR tiff_codecname[] = {'B', 'u', 'i','l', 't', '-','i', 'n', ' ', 'T', 'I', 'F', 'F',
        0}; /* Built-in TIFF */
static const WCHAR tiff_extension[] = {'*', '.', 'T', 'I', 'F',';', '*', '.', 'T', 'I', 'F','F', 0}; /* *.TIF;*.TIFF */
static const WCHAR tiff_mimetype[] = {'i', 'm', 'a','g', 'e', '/', 't', 'i', 'f', 'f', 0}; /* image/gif */
static const WCHAR tiff_format[] = {'T', 'I', 'F', 'F', 0}; /* TIFF */


ImageCodecInfo *
gdip_getcodecinfo_tiff ()
{
        tiff_codec.Clsid = (CLSID) { 0x557cf405, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x0, 0x0, 0xf8, 0x1e, 0xf3, 0x2e } };
        tiff_codec.FormatID = gdip_tif_image_format_guid;
        tiff_codec.CodecName = (const WCHAR*) tiff_codecname;
        tiff_codec.DllName = NULL;
        tiff_codec.FormatDescription = (const WCHAR*) tiff_format;
        tiff_codec.FilenameExtension = (const WCHAR*) tiff_extension;
        tiff_codec.MimeType = (const WCHAR*) tiff_mimetype;
        tiff_codec.Flags = Encoder | Decoder | SupportBitmap | Builtin;
        tiff_codec.Version = 1;
        tiff_codec.SigCount = 0;
        tiff_codec.SigSize = 0;
        tiff_codec.SigPattern = 0;
        tiff_codec.SigMask = 0;

        return &tiff_codec;
}


GpStatus 
gdip_load_tiff_image_from_file (FILE *fp, GpImage **image)
{
    GpBitmap *img = NULL;
    TIFF *tif = NULL;
    char *raster = NULL;

    tif = TIFFFdOpen(fileno (fp), "lose.tif", "r");
    if (tif) {
        TIFFRGBAImage tifimg;
        char emsg[1024];

        if (TIFFRGBAImageBegin(&tifimg, tif, 0, emsg)) {
            size_t npixels;

            img = gdip_bitmap_new ();
            img->image.type = imageBitmap;
            img->image.graphics = 0;
            img->image.width = tifimg.width;
            img->image.height = tifimg.height;
            /* libtiff expands stuff out to ARGB32 for us if we use this interface */
            img->image.pixFormat = Format32bppArgb;
            img->cairo_format = CAIRO_FORMAT_ARGB32;
            img->data.Stride = tifimg.width * 4;
            img->data.PixelFormat = img->image.pixFormat;
            img->data.Width = img->image.width;
            img->data.Height = img->image.height;

            npixels = tifimg.width * tifimg.height;
            /* Note that we don't use _TIFFmalloc */
            raster = GdipAlloc (npixels * sizeof (guint32));
            if (raster != NULL) {
                /* Problem: the raster data returned here has the origin at bottom left,
                 * not top left.  The TIFF guys must be in cahoots with the OpenGL folks.
                 *
                 * Then, to add insult to injury, it's in ARGB format, not ABGR.
                 */
                if (TIFFRGBAImageGet(&tifimg, (uint32*) raster, tifimg.width, tifimg.height)) { 
                    guchar *onerow = GdipAlloc (img->data.Stride);
                    guint32 *r32 = (guint32*)raster;
                    int i;

                    /* flip raster */
                    for (i = 0; i < tifimg.height / 2; i++) {
                        memcpy (onerow, raster + (img->data.Stride * i), img->data.Stride);
                        memcpy (raster + (img->data.Stride * i),
                                raster + (img->data.Stride * (tifimg.height - i - 1)),
                                img->data.Stride);
                        memcpy (raster + (img->data.Stride * (tifimg.height - i - 1)),
                                onerow,
                                img->data.Stride);
                    }
                    /* flip bytes */
                    for (i = 0; i < tifimg.width * tifimg.height; i++) {
                        *r32 =
                            (*r32 & 0xff000000) |
                            ((*r32 & 0x00ff0000) >> 16) |
                            (*r32 & 0x0000ff00) |
                            ((*r32 & 0x000000ff) << 16);
                        r32++;
                    }

                    img->data.Scan0 = raster;
                    GdipFree (onerow);
                    img->data.Reserved = GBD_OWN_SCAN0;

                    img->image.surface = cairo_surface_create_for_image (raster,
                                                                         img->cairo_format,
                                                                         img->image.width,
                                                                         img->image.height,
                                                                         img->data.Stride);
                    img->image.horizontalResolution = 0;
                    img->image.verticalResolution = 0;

                    img->image.imageFlags =
                        ImageFlagsReadOnly |
                        ImageFlagsHasRealPixelSize |
                        ImageFlagsColorSpaceRGB;

                    img->image.propItems = NULL;
                    img->image.palette = NULL;
                }
            } else {
                goto error;
            }

            TIFFRGBAImageEnd(&tifimg);
        } else {
            goto error;
        }

        TIFFClose(tif);
    } else {
        goto error;
    }

    *image = (GpImage *) img;
    return Ok;

  error:

    if (raster)
        GdipFree (raster);
    if (img)
        gdip_bitmap_dispose (img);
    if (tif)
        TIFFClose(tif);
    *image = NULL;
    return InvalidParameter;
}

GpStatus 
gdip_save_tiff_image_to_file (FILE *fp, GpImage *image)
{
    return NotImplemented;
}

#else

/* no libtiff */

ImageCodecInfo *
gdip_getcodecinfo_tiff ()
{
        return NULL;
}

GpStatus 
gdip_load_tiff_image_from_file (FILE *fp, GpImage **image)
{
    *image = NULL;
    return NotImplemented;
}

GpStatus 
gdip_save_tiff_image_to_file (FILE *fp, GpImage *image)
{
    return NotImplemented;
}

#endif
