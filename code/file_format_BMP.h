#if !defined(FILE_FORMAT_BMP_H)

#include "types_and_defines.h"
#include "memory.h"

#pragma pack(push, 1)
struct bitmap_header
{
	u16 FileType;
	u32 FileSize;
	u16 Reserved1;
	u16 Reserved2;
	u32 BitmapOffset;
	u32 Size;
	//TODO(bjorn): Negative values means that the order of pixels are reversed. A
	//solid BMP loader should be able to handle this.
	s32 Width;
	s32 Height;
	u16 Planes;
	u16 BitsPerPixel;
	//TODO(bjorn): Not all bitmaps have the following fields neccesarily!
	u32 Compression;
	u32 SizeOfBitmap;
	s32 HorzResolution;
	s32 VertResolution;
	u32 ColorsUsed;
	u32 ColorsImportant;

	u32 RedMask;
	u32 GreenMask;
	u32 BlueMask;
};
#pragma pack(pop)

internal_function v2s
PeekAtBMPResolution(u8* BMPFile, memi BMPFileSize)
{
  v2s Result = {};

	Assert(BMPFileSize != 0);
	if(BMPFileSize != 0)
	{
		bitmap_header *Header = (bitmap_header *)BMPFile;

    Result.Width = Header->Width;
    Result.Height = Header->Height;
  }

  return Result;
}

//NOTE(bjorn) Not complete BMP loading code!!
internal_function game_bitmap*
LoadBMPIntoBitmap(u8* BMPFile, memi BMPFileSize, game_bitmap* Bitmap)
{
	Assert(BMPFileSize != 0);
	if(BMPFileSize != 0)
	{
		bitmap_header *Header = (bitmap_header *)BMPFile;

    Assert(Bitmap->Width == (u32)Header->Width);
    Assert(Bitmap->Height == (u32)Header->Height);

		u32 *Pixels = (u32 *)(BMPFile + Header->BitmapOffset); 

		Assert(Header->Compression == 3);
		Assert(Header->Height >= 0);

		u32 RedMask = Header->RedMask;
		u32 GreenMask = Header->GreenMask;
		u32 BlueMask = Header->BlueMask;
		u32 AlphaMask = ~(Header->RedMask|Header->GreenMask|Header->BlueMask);

		bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);

		s32 AlphaShift = 24 - (s32)AlphaScan.Index;
		s32 RedShift   = 16 - (s32)RedScan.Index;
		s32 GreenShift = 8  - (s32)GreenScan.Index;
		s32 BlueShift  = 0  - (s32)BlueScan.Index;

    //TODO(bjorn): Larger than 4 Gb BMPs?
		s32 PixelCount = ((s32)BMPFileSize - Header->BitmapOffset) / 4;

    s32 DestPixelIndex = 0;
		for(s32 PixelIndex = 0;
				PixelIndex < PixelCount;
				++PixelIndex, ++DestPixelIndex)
		{
			u32 DA = RotateLeft(Pixels[PixelIndex] & AlphaMask, AlphaShift);
			u32 DR = RotateLeft(Pixels[PixelIndex] & RedMask,   RedShift);
			u32 DG = RotateLeft(Pixels[PixelIndex] & GreenMask, GreenShift);
			u32 DB = RotateLeft(Pixels[PixelIndex] & BlueMask,  BlueShift);

      v4 Texel = { (f32)((DR >> 16)&0xFF),
                   (f32)((DG >>  8)&0xFF),
                   (f32)((DB >>  0)&0xFF),
                   (f32)((DA >> 24)&0xFF)};

      Texel = sRGB255ToLinear1(Texel);

      Texel.RGB *= Texel.A;

      Texel = Linear1TosRGB255(Texel);

      Bitmap->Memory[DestPixelIndex] = (((u32)(Texel.A+0.5f) << 24) | 
                                         ((u32)(Texel.R+0.5f) << 16) | 
                                         ((u32)(Texel.G+0.5f) <<  8) | 
                                         ((u32)(Texel.B+0.5f) <<  0));

      if((PixelIndex+1)%(Bitmap->Width) == 0)
      {
        DestPixelIndex += (Bitmap->Pitch - Bitmap->Width);
      }
    }
	}

	return Bitmap;
}

#define FILE_FORMAT_BMP_H
#endif
