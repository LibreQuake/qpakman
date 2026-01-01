//------------------------------------------------------------------------
//  Basic image storage
//------------------------------------------------------------------------
//
//  Copyright (c) 2008  Andrew J Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "headers.h"
#include "main.h"

#include "im_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"


rgb_image_c::rgb_image_c(int _w, int _h) : width(_w), height(_h), is_solid(false)
{
  pixels = new u32_t[width * height];
}

rgb_image_c::~rgb_image_c()
{
  delete[] pixels;

  pixels = NULL;
  width = height = 0;
}


void rgb_image_c::Clear()
{
  memset(pixels, 0, width * height * sizeof(u32_t));

  is_solid = false;
}


rgb_image_c * rgb_image_c::Duplicate() const
{
  rgb_image_c *copy = new rgb_image_c(width, height);

  memcpy(copy->pixels, pixels, width * height * sizeof(u32_t));

  return copy;
}


void rgb_image_c::SwapEndian()
{
  u32_t *pos = pixels;
  u32_t *p_end = pos + (width * height);

  for (; pos < p_end; pos++)
  {
    *pos = UT_Swap32(*pos);
  }
}


void rgb_image_c::Mirror()
{
  for (int y=0; y < height; y++)
  {
    for (int x=0; x < width/2; x++)
    {
      int x2 = width - 1 - x;

      u32_t tmp     = PixelAt(x, y);
      PixelAt(x, y) = PixelAt(x2, y);
      PixelAt(x2,y) = tmp;
    }
  }
}


void rgb_image_c::Invert()
{
  u32_t *buffer = new u32_t[width + 1];

  for (int y=0; y < height/2; y++)
  {
    int y2 = height - 1 - y;

    memcpy(buffer,          &PixelAt(0, y),  width * sizeof(u32_t));
    memcpy(&PixelAt(0, y),  &PixelAt(0, y2), width * sizeof(u32_t));
    memcpy(&PixelAt(0, y2), buffer,          width * sizeof(u32_t));
  }

  delete[] buffer;
}


void rgb_image_c::RemoveAlpha()
{
  u32_t *pos   = pixels;
  u32_t *p_end = pos + (width * height);

  while (pos < p_end)
  {
    int a = RGB_A(*pos);

    u8_t r = RGB_R(*pos) * a / 255;
    u8_t g = RGB_G(*pos) * a / 255;
    u8_t b = RGB_B(*pos) * a / 255;

    *pos++ = MAKE_RGB(r, g, b);
  }
}


void rgb_image_c::ThresholdAlpha(u8_t alpha)
{
  u32_t *pos   = pixels;
  u32_t *p_end = pos + (width * height);

  for (; pos < p_end; pos++)
  {
    if (RGB_A(*pos) < alpha)
      *pos &= 0x00FFFFFFU;
    else
      *pos |= 0xFF000000U;
  }
}


void rgb_image_c::UpdateSolid()
{
  is_solid = true;

  const u32_t *pos   = pixels;
  const u32_t *p_end = pos + (width * height);

  for (; pos < p_end; pos++)
  {
    if (RGB_A(*pos) != ALPHA_SOLID)
    {
      is_solid = false;
      break;
    }
  }
}


rgb_image_c * rgb_image_c::ScaledDup(int new_w, int new_h)
{
  rgb_image_c *copy = new rgb_image_c(new_w, new_h);

  for (int y = 0; y < new_h; y++)
  for (int x = 0; x < new_w; x++)
  {
    int old_x = x * width  / new_w;
    int old_y = y * height / new_h;

    copy->PixelAt(x, y) = PixelAt(old_x, old_y);
  }

  return copy;
}


rgb_image_c * rgb_image_c::NiceMip()
{
  SYS_ASSERT((width  & 1) == 0);
  SYS_ASSERT((height & 1) == 0);

  int new_w = width  / 2;
  int new_h = height / 2;

  rgb_image_c *copy = new rgb_image_c(new_w, new_h);

  const unsigned char* src = (unsigned char*) (this->pixels);
  unsigned char*       dst = (unsigned char*) (copy->pixels);

  stbir_resize(
    src, width, height, 0,
    dst, new_w, new_h, 0,
    STBIR_4CHANNEL,              // stbir_pixel_layout pixel_layout
    STBIR_TYPE_UINT8_SRGB_ALPHA, // stbir_datatype data_type
    STBIR_EDGE_WRAP,             // stbir_edge edge
    STBIR_FILTER_CATMULLROM      // stbir_filter filter
  );

  return copy;
}


rgb_image_c * rgb_image_c::AvgSelectMip()
{
  SYS_ASSERT((width  & 1) == 0);
  SYS_ASSERT((height & 1) == 0);

  int new_w = width  / 2;
  int new_h = height / 2;

  u32_t pixels_original[4];

  rgb_image_c *copy = new rgb_image_c(new_w, new_h);

  for (int y = 0; y < new_h; y++)
  for (int x = 0; x < new_w; x++)
  {
    int R = 0, G = 0, B = 0, A = 0;
    int idx_original = 0;

    for (int dy = 0; dy < 2; dy++)
    for (int dx = 0; dx < 2; dx++)
    {
      u32_t cur = PixelAt(x*2+dx, y*2+dy);
      
      // save original pixel values, we'll select best match later
      pixels_original[idx_original++] = cur;

      R += RGB_R(cur); G += RGB_G(cur);
      B += RGB_B(cur); A += RGB_A(cur);
    }

    R /= 4; G /= 4;
    B /= 4; A /= 4;

    // select original pixel that matches average color best
    int32_t best_error = 2000000000;
    u32_t pixel_selected = 0;
    for(int i = 0; i < 4; i++) {
      u32_t cur = pixels_original[i];
      int32_t error = 0;

      int diff = RGB_R(cur) - R;
      error += diff * diff;
      diff = RGB_G(cur) - G;
      error += diff * diff;
      diff = RGB_B(cur) - B;
      error += diff * diff;
      diff = RGB_A(cur) - A;
      error += diff * diff;

      if(error < best_error)
      {
        best_error = error;
        pixel_selected = cur;
      }
    }

    copy->PixelAt(x, y) = pixel_selected;
  }


  return copy;
}

rgb_image_c * rgb_image_c::NiceSelectMip()
{
  SYS_ASSERT((width  & 1) == 0);
  SYS_ASSERT((height & 1) == 0);

  int new_w = width  / 2;
  int new_h = height / 2;

  u32_t pixels_original[4];

  // create a high-quality scaled down version
  rgb_image_c *copy = NiceMip();

  for (int y = 0; y < new_h; y++)
  for (int x = 0; x < new_w; x++)
  {
    int idx_original = 0;

    for (int dy = 0; dy < 2; dy++)
    for (int dx = 0; dx < 2; dx++)
    {
      // save original pixel values, we'll select best match later
      pixels_original[idx_original++] = PixelAt(x*2+dx, y*2+dy);
    }

    u32_t pixel_scaled = copy->PixelAt(x, y);

    // select original pixel that matches average color best
    int32_t best_error = 2000000000;
    u32_t pixel_selected = 0;
    for(int i = 0; i < 4; i++) {
      u32_t cur = pixels_original[i];
      int32_t error = 0;

      int diff = RGB_R(cur) - RGB_R(pixel_scaled);
      error += diff * diff;
      diff = RGB_G(cur) - RGB_G(pixel_scaled);
      error += diff * diff;
      diff = RGB_B(cur) - RGB_B(pixel_scaled);
      error += diff * diff;
      diff = RGB_A(cur) - RGB_A(pixel_scaled);
      error += diff * diff;

      if(error < best_error)
      {
        best_error = error;
        pixel_selected = cur;
      }
    }

    copy->PixelAt(x, y) = pixel_selected;
  }

  return copy;
}

void rgb_image_c::QuakeSkyFix()
{
  for (int y = 0; y < height; y++)
  {
    u32_t *pix = &PixelAt(0, y);
    u32_t *pix_end = &PixelAt(width / 2, y);

    for (; pix < pix_end; pix++)
      if (*pix == MAKE_RGB(0,0,0))
        *pix = MAKE_RGBA(0,0,0,ALPHA_TRANS);
  }

  // we assume some parts were transparent
  is_solid = false;
}


void rgb_image_c::BlackToTrans()
{
  for (int y = 0; y < height; y++)
  {
    u32_t *pix = &PixelAt(0, y);
    u32_t *pix_end = &PixelAt(width, y);

    for (; pix < pix_end; pix++)
      if (*pix == MAKE_RGB(0,0,0))
        *pix = MAKE_RGBA(0,0,0,ALPHA_TRANS);
  }

  // we assume some parts were transparent
  is_solid = false;
}


//--- editor settings ---
// vi:ts=2:sw=2:expandtab
