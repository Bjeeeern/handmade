#if !defined(RENDER_GROUP_H)

#include "memory.h"
#include "primitive_shapes.h"
#include "math.h"
#include "font.h"

// NOTE(bjorn):
//
// All Color variables passed to the renderer are in NON-premultiplied alpha.
//

struct camera_parameters
{
  f32 LensChamberSize;
  f32 NearClipPoint;
};

struct output_target_screen_variables
{
    m22 MeterToPixel;
    m22 PixelToMeter;
    v2 Center;
};

inline v4 
sRGB255ToLinear1(v4 sRGB)
{
  v4 Result;

  f32 Inv255 = 1.0f/255.0f;
  Result.R = Square(sRGB.R * Inv255);
  Result.G = Square(sRGB.G * Inv255);
  Result.B = Square(sRGB.B * Inv255);
  Result.A = sRGB.A * Inv255;

  return Result;
}

inline v4
Linear1TosRGB255(v4 Linear)
{
  v4 Result;
  Result.R = SquareRoot(Linear.R) * 255.0f;
  Result.G = SquareRoot(Linear.G) * 255.0f;
  Result.B = SquareRoot(Linear.B) * 255.0f;
  Result.A = Linear.A * 255.0f;

  return Result;
}

	internal_function void
DrawLine(game_bitmap* Buffer, 
				 f32 AX, f32 AY, f32 BX, f32 BY, 
				 f32 R, f32 G, f32 B)
{
	b32 AIsInside = (((0 <= AX) && (AX < Buffer->Width)) &&
									 ((0 <= AY) && (AY < Buffer->Height)));
	b32 BIsInside = (((0 <= BX) && (BX < Buffer->Width)) &&
									 ((0 <= BY) && (BY < Buffer->Height)));

	v2 Start = {};
	v2 End = {};

	if(AIsInside)
	{
		Start.X = AX;
		Start.Y = AY;
		End.X = BX;
		End.Y = BY;
	}
	else if(BIsInside)
	{
		Start.X = BX;
		Start.Y = BY;
		End.X = AX;
		End.Y = AY;
	}
	else
	{
		for(int times = 2;
				times > 0;
				times--)
		{
			if(AIsInside)
			{
				break;
			}
			else
			{
				if(AX < 0)
				{
					if(Absolute(BX - AX) != 0)
					{
						f32 t = (0 - AX) / (BX - AX);
						if(0.0f <= t && t <= 1.0f)
						{
							f32 a = t * (BY - AY) + AY;
							Start.X = 0;
							Start.Y = a;
							AX = Start.X;
							AY = Start.Y;
							AIsInside = (((0 <= AX) && (AX < Buffer->Width)) &&
													 ((0 <= AY) && (AY < Buffer->Height)));
							continue;
						}
					}
					return;
				}
				else if(AX >= Buffer->Width)
				{
					if(Absolute(BX - AX) != 0)
					{
						f32 t = (Buffer->Width - AX) / (BX - AX);
						if(0.0f <= t && t <= 1.0f)
						{
							f32 a = t * (BY - AY) + AY;
							Start.X = Buffer->Width - 1.0f;
							Start.Y = a;
							AX = Start.X;
							AY = Start.Y;
							AIsInside = (((0 <= AX) && (AX < Buffer->Width)) &&
													 ((0 <= AY) && (AY < Buffer->Height)));
							continue;
						}
					}
					return;
				}
				else if(AY < 0)
				{
					if(Absolute(BY - AY) != 0)
					{
						f32 t = (0 - AY) / (BY - AY);
						if(0.0f <= t && t <= 1.0f)
						{
							f32 a = t * (BX - AX) + AX;
							Start.X = a;
							Start.Y = 0;
							AX = Start.X;
							AY = Start.Y;
							AIsInside = (((0 <= AX) && (AX < Buffer->Width)) &&
													 ((0 <= AY) && (AY < Buffer->Height)));
							continue;
						}
					}
					return;
				}
				else if(AY >= Buffer->Height)
				{
					if(Absolute(BY - AY) != 0)
					{
						f32 t = (Buffer->Height - AY) / (BY - AY);
						if(0.0f <= t && t <= 1.0f)
						{
							f32 a = t * (BX - AX) + AX;
							Start.X = a;
							Start.Y = Buffer->Height - 1.0f;
							AX = Start.X;
							AY = Start.Y;
							AIsInside = (((0 <= AX) && (AX < Buffer->Width)) &&
													 ((0 <= AY) && (AY < Buffer->Height)));
							continue;
						}
					}
					return;
				}
				else { return; }
			}
		}

		End.X = BX;
		End.Y = BY;
	}

	u32 Color = ((RoundF32ToS32(R * 255.0f) << 16) |
							 (RoundF32ToS32(G * 255.0f) << 8) |
							 (RoundF32ToS32(B * 255.0f) << 0));

	f32 Length = Distance(Start, End);
	if(Distance(Start, End) < 1.0f)
	{
		Length = 1.0f;
	}

	f32 InverseLength = 1.0f / Length;
	v2 Normal = (End - Start) * InverseLength;

	f32 StepLength = 0.0f;
	while(StepLength < Length)
	{
		v2 Point = Start + (Normal * StepLength);
		s32 X = RoundF32ToS32(Point.X);
		s32 Y = RoundF32ToS32(Point.Y);

		if(((0 <= X) && (X < Buffer->Width)) &&
			 ((0 <= Y) && (Y < Buffer->Height)))
		{
			u32 *Pixel = (u32 *)Buffer->Memory + Buffer->Pitch * Y + X;
			*Pixel = Color;
		}
		else
		{
			return;
		}

		StepLength += 1.0f;
	}
}

	internal_function void
DrawLine(game_bitmap* Buffer, v2 A, v2 B, v3 C)
{
	DrawLine(Buffer, A.X, A.Y, B.X, B.Y, C.R, C.G, C.B);
}

	internal_function void
DrawChar(game_bitmap *Buffer, font *Font, u32 UnicodeCodePoint, 
				 f32 GlyphPixelWidth, f32 GlyphPixelHeight, 
				 f32 GlyphOriginLeft, f32 GlyphOriginBottom,
				 f32 RealR, f32 RealG, f32 RealB)
{
	unicode_to_glyph_data *Entries = (unicode_to_glyph_data *)(Font + 1);

	s32 CharEntryIndex = 0;
	for(s32 EntryIndex = 0;
			EntryIndex < Font->UnicodeCodePointCount;
			EntryIndex++)
	{
		if(UnicodeCodePoint == Entries[EntryIndex].UnicodeCodePoint)
		{
			CharEntryIndex = EntryIndex;
			break;
		}
	}

	s32 Offset = Entries[CharEntryIndex].OffsetToGlyphData;
	s32 CurveCount = Entries[CharEntryIndex].QuadraticCurveCount;
	v2 GlyphDim = {GlyphPixelWidth, GlyphPixelHeight};
	v2 GlyphOrigin = {GlyphOriginLeft, GlyphOriginBottom};

	quadratic_curve *Curves = (quadratic_curve *)((u8 *)Font + Offset);
	for(s32 CurveIndex = 0;
			CurveIndex < CurveCount;
			CurveIndex++)
	{
		quadratic_curve Curve = Curves[CurveIndex];

		v2 Start = Hadamard(Curve.Srt, GlyphDim) + GlyphOrigin;
		v2 End = Hadamard(Curve.End, GlyphDim) + GlyphOrigin;

		DrawLine(Buffer, Start.X, Start.Y, End.X, End.Y, RealR, RealG, RealB);
	}
}

//NOTE(bjorn):
//
// Cycle count per pixel estimation
// 
// bitshift 24
// bitmask 24
// s32tof32 2
// f32tos32 2
// mul 61
// sub 29
// add 13
// dot 6
// cross 4
// div 9
// comp 3
// lerp 3
// sqrt 3
//
// total: greater than 181 cycles, measured 270 ish.
// target: 50 or so.
//
	internal_function void
DrawTriangleSlowly(game_bitmap *Buffer, 
                   camera_parameters* CamParam, output_target_screen_variables* ScreenVars,
                   v3 CameraSpacePoint0, v3 CameraSpacePoint1, v3 CameraSpacePoint2, 
                   v2 UV0, v2 UV1, v2 UV2, 
                   game_bitmap* Bitmap, v4 RGBA)
{
  BEGIN_TIMED_BLOCK(DrawTriangleSlowly);

  b32 AllPointsBehindClipPoint = false;
  {
    if(CameraSpacePoint0.Z > CamParam->NearClipPoint &&
       CameraSpacePoint1.Z > CamParam->NearClipPoint &&
       CameraSpacePoint2.Z > CamParam->NearClipPoint)
    {
      AllPointsBehindClipPoint = true;
    }
    else
    {
      //TODO(bjorn): Come up with a good clipping scheme.
      if(CameraSpacePoint0.Z > CamParam->NearClipPoint ||
         CameraSpacePoint1.Z > CamParam->NearClipPoint ||
         CameraSpacePoint2.Z > CamParam->NearClipPoint)
      {
        AllPointsBehindClipPoint = true;
      }
    }
  }
  if(AllPointsBehindClipPoint) { return; }

  v2 PixelSpacePoint0, PixelSpacePoint1, PixelSpacePoint2;
  {
    f32 Divisor = (CamParam->LensChamberSize - CameraSpacePoint0.Z);
    f32 PerspectiveCorrection = SafeRatio0(CamParam->LensChamberSize, Divisor);

    PixelSpacePoint0 = 
      (ScreenVars->Center + 
       (ScreenVars->MeterToPixel * CameraSpacePoint0.XY) * PerspectiveCorrection);
  }
  {
    f32 Divisor = (CamParam->LensChamberSize - CameraSpacePoint1.Z);
    f32 PerspectiveCorrection = SafeRatio0(CamParam->LensChamberSize, Divisor);

    PixelSpacePoint1 = 
      (ScreenVars->Center + 
       (ScreenVars->MeterToPixel * CameraSpacePoint1.XY) * PerspectiveCorrection);
  }
  {
    f32 Divisor = (CamParam->LensChamberSize - CameraSpacePoint2.Z);
    f32 PerspectiveCorrection = SafeRatio0(CamParam->LensChamberSize, Divisor);

    PixelSpacePoint2 = 
      (ScreenVars->Center + 
       (ScreenVars->MeterToPixel * CameraSpacePoint2.XY) * PerspectiveCorrection);
  }

  f32 Bary2D_V1YmV2Y = (PixelSpacePoint1.Y - PixelSpacePoint2.Y);
  f32 Bary2D_V0XmV2X = (PixelSpacePoint0.X - PixelSpacePoint2.X);
  f32 Bary2D_V2XmV1X = (PixelSpacePoint2.X - PixelSpacePoint1.X);
  f32 Bary2D_V0YmV2Y = (PixelSpacePoint0.Y - PixelSpacePoint2.Y);
  f32 Bary2D_V2YmV0Y = (PixelSpacePoint2.Y - PixelSpacePoint0.Y);
  f32 Bary2D_InvDenom = 1.0f / (Bary2D_V1YmV2Y*Bary2D_V0XmV2X + Bary2D_V2XmV1X*Bary2D_V0YmV2Y);

  v3 Bary3D_TotalAreaVector = Cross(CameraSpacePoint2-CameraSpacePoint0, 
                                    CameraSpacePoint1-CameraSpacePoint0);
  f32 Bary3D_TotalAreaVectorX = Bary3D_TotalAreaVector.X;
  f32 Bary3D_TotalAreaVectorY = Bary3D_TotalAreaVector.Y;
  f32 Bary3D_TotalAreaVectorZ = Bary3D_TotalAreaVector.Z;

  f32 CameraSpacePoint0X = CameraSpacePoint0.X;
  f32 CameraSpacePoint0Y = CameraSpacePoint0.Y;
  f32 CameraSpacePoint0Z = CameraSpacePoint0.Z;

  f32 CameraSpacePoint1X = CameraSpacePoint1.X;
  f32 CameraSpacePoint1Y = CameraSpacePoint1.Y;
  f32 CameraSpacePoint1Z = CameraSpacePoint1.Z;

  f32 CameraSpacePoint2X = CameraSpacePoint2.X;
  f32 CameraSpacePoint2Y = CameraSpacePoint2.Y;
  f32 CameraSpacePoint2Z = CameraSpacePoint2.Z;

  v3 FocalPoint = {0,0,CamParam->LensChamberSize};
  v3 TriangleNormal = 
    Cross(CameraSpacePoint1-CameraSpacePoint0, CameraSpacePoint2-CameraSpacePoint0);
  v3 TriangleCenter = (CameraSpacePoint0 + CameraSpacePoint1 + CameraSpacePoint2) * (1.0f/3.0f);

  //TODO(bjorn): Better name here.
  f32 LinePlaneIntersection_Numerator = Dot(TriangleCenter - FocalPoint, TriangleNormal);
  b32 CameraAndTriangleNormalsAreOrthogonal = LinePlaneIntersection_Numerator  == 0;
  if(CameraAndTriangleNormalsAreOrthogonal) { return; }

  s32 Left   = RoundF32ToS32(Min(Min(PixelSpacePoint0.X, PixelSpacePoint1.X),PixelSpacePoint2.X));
  s32 Right  = RoundF32ToS32(Max(Max(PixelSpacePoint0.X, PixelSpacePoint1.X),PixelSpacePoint2.X));
  s32 Bottom = RoundF32ToS32(Min(Min(PixelSpacePoint0.Y, PixelSpacePoint1.Y),PixelSpacePoint2.Y));
  s32 Top    = RoundF32ToS32(Max(Max(PixelSpacePoint0.Y, PixelSpacePoint1.Y),PixelSpacePoint2.Y));

  Left   = Clamp(0, Left,   Buffer->Width );
  Right  = Clamp(0, Right,  Buffer->Width );
  Bottom = Clamp(0, Bottom, Buffer->Height);
  Top    = Clamp(0, Top,    Buffer->Height);

  RGBA.RGB *= RGBA.A;
  f32 Inv255 = 1.0f/255.0f;

  b32 IsOrthogonal = CamParam->LensChamberSize == positive_infinity32;

  f32 PixelToMeter = ScreenVars->PixelToMeter.A;

  u32 *UpperLeftPixel = Buffer->Memory + Left + Bottom * Buffer->Pitch;
  BEGIN_TIMED_BLOCK(ProcessPixel);
  for(s32 Y = Bottom;
      Y < Top;
      ++Y)
	{
		u32 *Pixel = UpperLeftPixel;

		for(s32 IX = Left;
				IX < Right;
				IX += 4)
    {
      for(s32 I = 0;
          I < 4;
          I++)
      {
        s32 X = IX + I;

        f32 PixelPointX = (f32)X;
        f32 PixelPointY = (f32)Y;

        f32 BarycentricWeight0;
        f32 BarycentricWeight1;
        f32 BarycentricWeight2;
        if(IsOrthogonal)
        {
          f32 PYmV2Y = (PixelPointY - PixelSpacePoint2.Y);
          f32 PXmV2X = (PixelPointX - PixelSpacePoint2.X);

          BarycentricWeight0 = (Bary2D_V1YmV2Y*PXmV2X + Bary2D_V2XmV1X*PYmV2Y) * Bary2D_InvDenom; 
          BarycentricWeight1 = (Bary2D_V2YmV0Y*PXmV2X + Bary2D_V0XmV2X*PYmV2Y) * Bary2D_InvDenom; 
          BarycentricWeight2 = 1.0f - BarycentricWeight0 - BarycentricWeight1;
        }
        else
        {
          //NOTE(bjorn): Source: https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
          f32 PointX;
          f32 PointY;
          f32 PointZ;
          {
            f32 PointInCameraSpaceX = PixelToMeter * (PixelPointX - ScreenVars->Center.X);
            f32 PointInCameraSpaceY = PixelToMeter * (PixelPointY - ScreenVars->Center.Y);

            v3 LineDirection = { PointInCameraSpaceX,
                                 PointInCameraSpaceY,
                                 -FocalPoint.Z};

            f32 Denominator = (LineDirection.X * TriangleNormal.X +
                               LineDirection.Y * TriangleNormal.Y +
                               LineDirection.Z * TriangleNormal.Z);
            f32 d = LinePlaneIntersection_Numerator / Denominator;

            PointX = d * LineDirection.X;
            PointY = d * LineDirection.Y;
            PointZ = (1 - d) * FocalPoint.Z;
          }

          f32 V0PX = CameraSpacePoint0X-PointX;
          f32 V0PY = CameraSpacePoint0Y-PointY;
          f32 V0PZ = CameraSpacePoint0Z-PointZ;

          f32 V1PX = CameraSpacePoint1X-PointX;
          f32 V1PY = CameraSpacePoint1Y-PointY;
          f32 V1PZ = CameraSpacePoint1Z-PointZ;

          f32 V2PX = CameraSpacePoint2X-PointX;
          f32 V2PY = CameraSpacePoint2Y-PointY;
          f32 V2PZ = CameraSpacePoint2Z-PointZ;

          f32 AreaAVectorX = V2PY*V1PZ - V2PZ*V1PY;
          f32 AreaAVectorY = V2PZ*V1PX - V2PX*V1PZ;
          f32 AreaAVectorZ = V2PX*V1PY - V2PY*V1PX;

          f32 AreaBVectorX = V0PY*V2PZ - V0PZ*V2PY;
          f32 AreaBVectorY = V0PZ*V2PX - V0PX*V2PZ;
          f32 AreaBVectorZ = V0PX*V2PY - V0PY*V2PX;

          f32 DotAB = (AreaAVectorX*AreaBVectorX +
                       AreaAVectorY*AreaBVectorY +
                       AreaAVectorZ*AreaBVectorZ);
          f32 DotBB = (AreaBVectorX*AreaBVectorX +
                       AreaBVectorY*AreaBVectorY +
                       AreaBVectorZ*AreaBVectorZ);
          f32 InvDotTB  = 1.0f / (Bary3D_TotalAreaVectorX*AreaBVectorX +
                                  Bary3D_TotalAreaVectorY*AreaBVectorY +
                                  Bary3D_TotalAreaVectorZ*AreaBVectorZ);

          //TODO(bjorn): Deal with artefacts due to indirect calculation of the last weight.
          f32 Epsilon = 0.0001f;
          BarycentricWeight0 = DotAB * InvDotTB;
          BarycentricWeight1 = DotBB * InvDotTB;
          BarycentricWeight2 = 1.0f - BarycentricWeight0 - BarycentricWeight1;

          BarycentricWeight0 += Epsilon;
          BarycentricWeight1 += Epsilon;
          BarycentricWeight2 += Epsilon;
        }

        b32 InsideTriangle = (BarycentricWeight0 >= 0 &&
                              BarycentricWeight1 >= 0 &&
                              BarycentricWeight2 >= 0);
        if(InsideTriangle)
        {
          f32 TexelR;
          f32 TexelG;
          f32 TexelB;
          f32 TexelA;
          if(Bitmap)
          {
            f32 U = BarycentricWeight0*UV0.U + BarycentricWeight1*UV1.U + BarycentricWeight2*UV2.U;
            f32 V = BarycentricWeight0*UV0.V + BarycentricWeight1*UV1.V + BarycentricWeight2*UV2.V;

#if 0
            Assert(0.0f <= UV.U && UV.U <= 1.0f);
            Assert(0.0f <= UV.V && UV.V <= 1.0f);
#endif

            f32 tX = U * (f32)(Bitmap->Width -2);
            f32 tY = V * (f32)(Bitmap->Height-2);

            s32 wX = (s32)tX;
            s32 wY = (s32)tY;

            f32 fX = tX - wX;
            f32 fY = tY - wY;

            Assert(0 <= tX && tX < Bitmap->Width);
            Assert(0 <= tY && tY < Bitmap->Height);

            u32* TexelPtr = Bitmap->Memory + Bitmap->Pitch * wY + wX;
            u32* TexelPtr0 = TexelPtr;
            u32* TexelPtr1 = TexelPtr + 1;
            u32* TexelPtr2 = TexelPtr + Bitmap->Pitch;
            u32* TexelPtr3 = TexelPtr + Bitmap->Pitch + 1;

            f32 TexelSmp0R = (f32)((*TexelPtr0 >> 16)&0xFF);
            f32 TexelSmp0G = (f32)((*TexelPtr0 >>  8)&0xFF);
            f32 TexelSmp0B = (f32)((*TexelPtr0 >>  0)&0xFF);
            f32 TexelSmp0A = (f32)((*TexelPtr0 >> 24)&0xFF);

            f32 TexelSmp1R = (f32)((*TexelPtr1 >> 16)&0xFF);
            f32 TexelSmp1G = (f32)((*TexelPtr1 >>  8)&0xFF);
            f32 TexelSmp1B = (f32)((*TexelPtr1 >>  0)&0xFF);
            f32 TexelSmp1A = (f32)((*TexelPtr1 >> 24)&0xFF);

            f32 TexelSmp2R = (f32)((*TexelPtr2 >> 16)&0xFF);
            f32 TexelSmp2G = (f32)((*TexelPtr2 >>  8)&0xFF);
            f32 TexelSmp2B = (f32)((*TexelPtr2 >>  0)&0xFF);
            f32 TexelSmp2A = (f32)((*TexelPtr2 >> 24)&0xFF);

            f32 TexelSmp3R = (f32)((*TexelPtr3 >> 16)&0xFF);
            f32 TexelSmp3G = (f32)((*TexelPtr3 >>  8)&0xFF);
            f32 TexelSmp3B = (f32)((*TexelPtr3 >>  0)&0xFF);
            f32 TexelSmp3A = (f32)((*TexelPtr3 >> 24)&0xFF);

            TexelSmp0R = Square(TexelSmp0R * Inv255);
            TexelSmp0G = Square(TexelSmp0G * Inv255);
            TexelSmp0B = Square(TexelSmp0B * Inv255);
            TexelSmp0A =        TexelSmp0A * Inv255;

            TexelSmp1R = Square(TexelSmp1R * Inv255);
            TexelSmp1G = Square(TexelSmp1G * Inv255);
            TexelSmp1B = Square(TexelSmp1B * Inv255);
            TexelSmp1A =        TexelSmp1A * Inv255;

            TexelSmp2R = Square(TexelSmp2R * Inv255);
            TexelSmp2G = Square(TexelSmp2G * Inv255);
            TexelSmp2B = Square(TexelSmp2B * Inv255);
            TexelSmp2A =        TexelSmp2A * Inv255;

            TexelSmp3R = Square(TexelSmp3R * Inv255);
            TexelSmp3G = Square(TexelSmp3G * Inv255);
            TexelSmp3B = Square(TexelSmp3B * Inv255);
            TexelSmp3A =        TexelSmp3A * Inv255;

            f32 ifY = (1.0f-fY);
            f32 ifX = (1.0f-fX);
            f32 ifYmifX = ifY*ifX;
            f32 ifYmfX  = ifY*fX;
            f32 ifXmfY  = ifX*fY;
            f32 fXmfY   = fX*fY;
            TexelR = ifYmifX*TexelSmp0R + ifYmfX*TexelSmp1R + ifXmfY*TexelSmp2R + fXmfY*TexelSmp3R;
            TexelG = ifYmifX*TexelSmp0G + ifYmfX*TexelSmp1G + ifXmfY*TexelSmp2G + fXmfY*TexelSmp3G;
            TexelB = ifYmifX*TexelSmp0B + ifYmfX*TexelSmp1B + ifXmfY*TexelSmp2B + fXmfY*TexelSmp3B;
            TexelA = ifYmifX*TexelSmp0A + ifYmfX*TexelSmp1A + ifXmfY*TexelSmp2A + fXmfY*TexelSmp3A;

            TexelR = TexelR * RGBA.R;
            TexelG = TexelG * RGBA.G;
            TexelB = TexelB * RGBA.B;
            TexelA = TexelA * RGBA.A;
          }
          else
          {
            TexelR = RGBA.R;
            TexelG = RGBA.G;
            TexelB = RGBA.B;
            TexelA = RGBA.A;
          }

          f32 DestR = (f32)((*Pixel >> 16)&0xFF);
          f32 DestG = (f32)((*Pixel >>  8)&0xFF);
          f32 DestB = (f32)((*Pixel >>  0)&0xFF);
          f32 DestA = (f32)((*Pixel >> 24)&0xFF);

          DestR = Square(DestR * Inv255);
          DestG = Square(DestG * Inv255);
          DestB = Square(DestB * Inv255);
          DestA = DestA * Inv255;

          f32 InvTexelA = (1.0f - TexelA);
          f32 BlendedR = DestR*InvTexelA + TexelR;
          f32 BlendedG = DestG*InvTexelA + TexelG;
          f32 BlendedB = DestB*InvTexelA + TexelB;
          f32 BlendedA = DestA*InvTexelA + TexelA;

          BlendedR = SquareRoot(BlendedR) * 255.0f;
          BlendedG = SquareRoot(BlendedG) * 255.0f;
          BlendedB = SquareRoot(BlendedB) * 255.0f;
          BlendedA = BlendedA * 255.0f;

          u32 Color = (((u32)(BlendedA+0.5f) << 24) | 
                       ((u32)(BlendedR+0.5f) << 16) | 
                       ((u32)(BlendedG+0.5f) <<  8) | 
                       ((u32)(BlendedB+0.5f) <<  0));

          *Pixel = Color;
        }
#if 0
        else
        {
          *Pixel = 0xFF00FF00;
        }
#endif

        Pixel += 1;
      }
    }

    UpperLeftPixel += Buffer->Pitch;
  }

  END_TIMED_BLOCK_COUNTED(ProcessPixel, (Right-Left) * (Top-Bottom));
  END_TIMED_BLOCK(DrawTriangleSlowly);
}

  internal_function void
DrawRectangle(game_bitmap *Buffer, rectangle2 Rect, v3 RGB)
{
  s32 Left   = RoundF32ToS32(Rect.Min.X);
  s32 Right  = RoundF32ToS32(Rect.Max.X);
  s32 Bottom = RoundF32ToS32(Rect.Min.Y);
  s32 Top    = RoundF32ToS32(Rect.Max.Y);

  Left = Left < 0 ? 0 : Left;
  Bottom = Bottom < 0 ? 0 : Bottom;

  Right = Right > Buffer->Width ? Buffer->Width : Right;
  Top = Top > Buffer->Height ? Buffer->Height : Top;

  u32 Color = ((RoundF32ToS32(RGB.R * 255.0f) << 16) |
               (RoundF32ToS32(RGB.G * 255.0f) << 8) |
               (RoundF32ToS32(RGB.B * 255.0f) << 0));

  s32 PixelPitch = Buffer->Width;

	u32 *UpperLeftPixel = (u32 *)Buffer->Memory + Left + Bottom * PixelPitch;

	for(s32 Y = Bottom;
			Y < Top;
			++Y)
	{
		u32 *Pixel = UpperLeftPixel;

		for(s32 X = Left;
				X < Right;
				++X)
		{
			*Pixel++ = Color;
		}

		UpperLeftPixel += PixelPitch;
	}
}

// TODO(bjorn): There is a visual bug when drawn from BottomRight to TopLeft.
	internal_function void
DrawFrame(game_bitmap *Buffer, rectangle2 R, v2 WorldDir, v3 Color)
{
	v2 ScreenSpaceDir = v2{WorldDir.X, WorldDir.Y};
	Assert(LengthSquared(WorldDir) <= 1.001f);
	Assert(LengthSquared(WorldDir) >= 0.999f);

	m22 Rot90CCW = {0,-1,
								 1, 0};
	v2 Origo = (R.Min + R.Max)*0.5f;
	v2 YAxis = ScreenSpaceDir * Absolute(R.Max.Y - R.Min.Y)*0.5f;
	v2 XAxis = (Rot90CCW * ScreenSpaceDir) * Absolute(R.Max.X - R.Min.X)*0.5f;

	v2 TopLeft     = Origo - XAxis + YAxis;
	v2 TopRight    = Origo + XAxis + YAxis;
	v2 BottomLeft  = Origo - XAxis - YAxis;
	v2 BottomRight = Origo + XAxis - YAxis;

	DrawLine(Buffer, TopLeft,     TopRight,    Color);
	DrawLine(Buffer, TopRight,    BottomRight, Color);
	DrawLine(Buffer, BottomRight, BottomLeft,  Color);
	DrawLine(Buffer, BottomLeft,  TopLeft,     Color);
}

#if 0
	internal_function void
DrawBitmap(game_bitmap* Buffer, game_bitmap* Bitmap, 
					 v2 TopLeft, v2 RealDim, f32 Alpha = 1.0f)
{
	v2 BottomRight = TopLeft + RealDim;

	f32 BitmapPixelPerScreenPixelX = (f32)Bitmap->Width / Absolute(BottomRight.X - TopLeft.X); 
	f32 BitmapPixelPerScreenPixelY = (f32)Bitmap->Height / Absolute(BottomRight.Y - TopLeft.Y);

	s32 DestLeft   = RoundF32ToS32(TopLeft.X);
	s32 DestTop    = RoundF32ToS32(TopLeft.Y);
	s32 DestRight  = RoundF32ToS32(BottomRight.X);
	s32 DestBottom = RoundF32ToS32(BottomRight.Y);

	DestLeft = DestLeft < 0 ? 0 : DestLeft;
	DestTop = DestTop < 0 ? 0 : DestTop;
	DestRight = DestRight > Buffer->Width ? Buffer->Width : DestRight;
	DestBottom = DestBottom > Buffer->Height ? Buffer->Height : DestBottom;

	Assert(DestLeft >= 0);
	Assert(DestRight <= Buffer->Width);
	Assert(DestTop >= 0);
	Assert(DestBottom <= Buffer->Height);

	u32 *DestBufferRow = Buffer->Memory + (Buffer->Pitch * DestTop + DestLeft);

	for(s32 DestY = DestTop;
			DestY < DestBottom;
			++DestY)
	{
		u32 *DestPixel = DestBufferRow;

		for(s32 DestX = DestLeft;
				DestX < DestRight;
				++DestX)
		{

			f32 BitmapPixelX = (DestX - TopLeft.X) * BitmapPixelPerScreenPixelX;
			f32 BitmapPixelY = (DestY - TopLeft.Y) * BitmapPixelPerScreenPixelY;

			s32 SrcX = RoundF32ToS32(BitmapPixelX);
			s32 SrcY = RoundF32ToS32(BitmapPixelY);

			if(0 <= SrcX && SrcX < Bitmap->Width &&
				 0 <= SrcY && SrcY < Bitmap->Height)
			{
				u32 SourceColor = Bitmap->Memory[Bitmap->Pitch * SrcY + SrcX];
				u32 DestColor = *DestPixel;

				f32 SA = (f32)((SourceColor >> 24)&0xFF)*Alpha;
				f32 SR = (f32)((SourceColor >> 16)&0xFF)*Alpha;
				f32 SG = (f32)((SourceColor >>  8)&0xFF)*Alpha;
				f32 SB = (f32)((SourceColor >>  0)&0xFF)*Alpha;
        f32 RSA = SA / 255.0f;

        f32 DA = (f32)((DestColor   >> 24)&0xFF);
				f32 DR = (f32)((DestColor   >> 16)&0xFF);
				f32 DG = (f32)((DestColor   >>  8)&0xFF);
				f32 DB = (f32)((DestColor   >>  0)&0xFF);
        f32 RDA = DA / 255.0f;

				f32 InvRSA = (1.0f - RSA);
				f32 A = 255.0f*(RSA + RDA - RSA*RDA);
 				f32 R = InvRSA*DR + SR;
				f32 G = InvRSA*DG + SG;
				f32 B = InvRSA*DB + SB;

        *DestPixel = (((u32)(A+0.5f) << 24) | 
                      ((u32)(R+0.5f) << 16) | 
                      ((u32)(G+0.5f) <<  8) | 
                      ((u32)(B+0.5f) <<  0));
      }

			DestPixel++;
		}

		DestBufferRow += Buffer->Pitch;
	}
}
#endif

	internal_function void
DrawCircle(game_bitmap *Buffer, 
					 f32 RealX, f32 RealY, f32 RealRadius,
					 f32 R, f32 G, f32 B, f32 A)
{
	s32 CenterX = RoundF32ToS32(RealX);
	s32 CenterY = RoundF32ToS32(RealY);
	s32 Left    = RoundF32ToS32(RealX - RealRadius);
	s32 Right   = RoundF32ToS32(RealX + RealRadius);
	s32 Bottom  = RoundF32ToS32(RealY - RealRadius);
	s32 Top     = RoundF32ToS32(RealY + RealRadius);

	s32 RadiusSquared = RoundF32ToS32(Square(RealRadius));

	Left = Left < 0 ? 0 : Left;
	Bottom = Bottom < 0 ? 0 : Bottom;
	Right = Right > Buffer->Width ? Buffer->Width : Right;
	Top = Top > Buffer->Height ? Buffer->Height : Top;

	u32 *UpperLeftPixel = (u32 *)Buffer->Memory + Left + Bottom * Buffer->Pitch;

	for(s32 Y = Bottom;
			Y < Top;
			++Y)
	{
		u32 *Pixel = UpperLeftPixel;

		for(s32 X = Left;
				X < Right;
				++X)
		{
			s32 dX = X-CenterX;
			s32 dY = Y-CenterY;
			if((Square(dX)+Square(dY)) < RadiusSquared)
			{
				f32 OldR = ((*Pixel & 0x00FF0000) >> 16) / 255.0f;
				f32 OldG = ((*Pixel & 0x0000FF00) >> 8 ) / 255.0f;
				f32 OldB = ((*Pixel & 0x000000FF) >> 0 ) / 255.0f;

				u32 Color = ((RoundF32ToS32(Lerp(A, OldR, R) * 255.0f) << 16) |
										 (RoundF32ToS32(Lerp(A, OldG, G) * 255.0f) << 8) |
										 (RoundF32ToS32(Lerp(A, OldB, B) * 255.0f) << 0));

				*Pixel = Color;
			}
			Pixel++;
		}

		UpperLeftPixel += Buffer->Pitch;
	}
}

	internal_function void
DrawCircle(game_bitmap *Buffer, v2 P, f32 R, v4 C)
{
	DrawCircle(Buffer, P.X, P.Y, R, C.R, C.G, C.B, C.A);
}

enum render_group_entry_type
{
	RenderGroupEntryType_render_entry_vector,
	RenderGroupEntryType_render_entry_coordinate_system,
	RenderGroupEntryType_render_entry_wire_cube,
	RenderGroupEntryType_render_entry_blank_quad,
	RenderGroupEntryType_render_entry_quad,
	RenderGroupEntryType_render_entry_sphere,
};

struct render_entry_header
{
  render_group_entry_type Type;
};

struct render_entry_vector
{
	v4 Color;
	v3 A;
	v3 B;
};
struct render_entry_coordinate_system
{
	m44 Tran;
};
struct render_entry_wire_cube
{
	v4 Color;
	m44 Tran;
};
struct render_entry_quad
{
	game_bitmap* Bitmap;
	v4 Color;
	m44 Tran;
};
struct render_entry_blank_quad
{
	v4 Color;
	m44 Tran;
};
struct render_entry_sphere
{
	v4 Color;
	m44 Tran;
};

struct render_group
{
  m44 WorldToCamera;

  camera_parameters CamParam;

  b32 ClearScreen;
  v4 ClearScreenColor;

  u32 MaxPushBufferSize;
  u32 PushBufferSize;
  u8* PushBufferBase;
};

internal_function render_group*
AllocateRenderGroup(memory_arena* Arena, u32 MaxPushBufferSize)
{
	render_group* Result = PushStruct(Arena, render_group);

	Result->PushBufferBase = PushArray(Arena, MaxPushBufferSize, u8);
  Result->PushBufferSize = 0;
  Result->MaxPushBufferSize = MaxPushBufferSize;

	return Result;
}

#define PushRenderElement(Group, type) (type*)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type)
internal_function void*
PushRenderElement_(render_group* RenderGroup, u32 Size, render_group_entry_type Type)
{
  void* Result = 0;

  Size += sizeof(render_entry_header);

  if(RenderGroup->PushBufferSize + Size < RenderGroup->MaxPushBufferSize)
  {
    render_entry_header* Header = 
      (render_entry_header*)(RenderGroup->PushBufferBase + RenderGroup->PushBufferSize);
    Header->Type = Type;

    Result = RenderGroup->PushBufferBase + RenderGroup->PushBufferSize + sizeof(render_entry_header);

    RenderGroup->PushBufferSize += Size;
  }
  else
  {
    InvalidCodePath;
  }

  return Result;
}

internal_function void
PushBlankQuad(render_group* RenderGroup, m44 T, v4 Color)
{
	render_entry_blank_quad* Entry = PushRenderElement(RenderGroup, render_entry_blank_quad);
	Entry->Tran = T;
	Entry->Color = Color;
}

internal_function void
PushQuad(render_group* RenderGroup, m44 T, game_bitmap* Bitmap, v4 Color = {1,1,1,1})
{
	render_entry_quad* Entry = PushRenderElement(RenderGroup, render_entry_quad);
	Entry->Tran = T;
	Entry->Bitmap = Bitmap;
	Entry->Color = Color;
}

internal_function void
PushWireCube(render_group* RenderGroup, m44 T, v4 Color = {0,0.4f,0.8f,1})
{
	render_entry_wire_cube* Entry = PushRenderElement(RenderGroup, render_entry_wire_cube);
	Entry->Tran = T;
	Entry->Color = Color;
}
internal_function void
PushWireCube(render_group* RenderGroup, rectangle3 Rect, v4 Color = {0,0,1,1})
{
	center_dim_v3_result CenterDim = RectToCenterDim(Rect);
	m44 T = ConstructTransform(CenterDim.Center, QuaternionIdentity(), CenterDim.Dim);

	PushWireCube(RenderGroup, T, Color);
}

internal_function void
PushVector(render_group* RenderGroup, m44 T, v3 A, v3 B, v4 Color = {0,0.4f,0.8f,1})
{
	render_entry_vector* Entry = PushRenderElement(RenderGroup, render_entry_vector);
	Entry->A = T * A;
	Entry->B = T * B;
	Entry->Color = Color;
}

internal_function void
PushCoordinateSystem(render_group* RenderGroup, m44 T)
{
	render_entry_coordinate_system* Entry = 
    PushRenderElement(RenderGroup, render_entry_coordinate_system);
	Entry->Tran = T;
}

internal_function void
PushSphere(render_group* RenderGroup, m44 T, v4 Color = {0,0.4f,0.8f,1})
{
	render_entry_sphere* Entry = PushRenderElement(RenderGroup, render_entry_sphere);
	Entry->Tran = T;
	Entry->Color = Color;
}

//TODO(Bjorn): SetCamera(f32 HeightOfScreenInGameMeters, f32 YFoV)
internal_function void
SetCamera(render_group* RenderGroup, m44 WorldToCamera, f32 LensChamberSize, f32 NearClipPoint)
{
  RenderGroup->WorldToCamera = WorldToCamera;

  camera_parameters* CamParam = &RenderGroup->CamParam;

  Assert(NearClipPoint <= LensChamberSize);
  CamParam->LensChamberSize = LensChamberSize;
  CamParam->NearClipPoint = NearClipPoint;
}

internal_function void
ClearScreen(render_group* RenderGroup, v4 Color)
{
  RenderGroup->ClearScreen = true;
  RenderGroup->ClearScreenColor = Color;
}

struct pixel_pos_result
{
  b32 PointIsInView;
  f32 PerspectiveCorrection;
  v2 P;
};

internal_function pixel_pos_result
ProjectPointToScreen(m44 WorldToCamera, camera_parameters* CamParam, 
                     output_target_screen_variables* ScreenVars, v3 Pos)
{
	pixel_pos_result Result = {};

  v3 PosRelativeCameraLens = WorldToCamera * Pos;
  //NOTE(bjorn): The untransformed camera has the lens located at the origin
  //and is looking along the negative z-axis in a right-hand coordinate system. 
  f32 DistanceFromCameraLens = -PosRelativeCameraLens.Z;

  f32 PerspectiveCorrection = 
    CamParam->LensChamberSize / (DistanceFromCameraLens + CamParam->LensChamberSize);

  //TODO(bjorn): Add clipping plane parameters.
  b32 PointIsInView = (DistanceFromCameraLens + CamParam->LensChamberSize) > 0;
  if(PointIsInView)
  {
    Result.PointIsInView = PointIsInView;
    Result.PerspectiveCorrection = PerspectiveCorrection;
    Result.P = (ScreenVars->Center + 
                (ScreenVars->MeterToPixel * PosRelativeCameraLens.XY) * PerspectiveCorrection);
  }

  return Result;
}

struct pixel_line_segment_result
{
  b32 PartOfSegmentInView;
  v2 A;
  v2 B;
};
//TODO(bjorn): Do a unique solution to this that interpolates the points when nessecary.
//             Using the last term in a m33? Would that be inversible?
  internal_function pixel_line_segment_result
ProjectSegmentToScreen(m44 WorldToCamera, camera_parameters* CamParam, 
                       output_target_screen_variables* ScreenVars, v3 A, v3 B)
{
  pixel_line_segment_result Result = {};

  pixel_pos_result PixPosA = 
    ProjectPointToScreen(WorldToCamera, CamParam, ScreenVars, A);
  pixel_pos_result PixPosB = 
    ProjectPointToScreen(WorldToCamera, CamParam, ScreenVars, B);

  if(PixPosA.PointIsInView && PixPosB.PointIsInView)
  {
    Result.PartOfSegmentInView = true;
    Result.A = PixPosA.P;
    Result.B = PixPosB.P;
  }
  else if (PixPosA.PointIsInView || PixPosB.PointIsInView)
  {
    Result.PartOfSegmentInView = false;

    pixel_pos_result InView = PixPosA.PointIsInView ? PixPosA : PixPosB;
    pixel_pos_result OutOfView = PixPosB.PointIsInView ? PixPosA : PixPosB;

    Result.A = InView.P;
    //Result.B = 
  }

  return Result;
}

internal_function void
DrawVector(render_group* RenderGroup, output_target_screen_variables* ScreenVars, 
           game_bitmap* OutputTarget, v3 V0, v3 V1, v3 Color)
{
  pixel_line_segment_result LineSegment = 
    ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                           ScreenVars, V0, V1);
  if(LineSegment.PartOfSegmentInView)
  {
    DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Color);
  }

  v3 n = Normalize(V1-V0);

  //TODO(bjorn): Implement a good GetComplimentaryAxes() fuction.
  v3 t0 = {};
  v3 t1 = {};
  {
    f32 e = 0.01f;
    if(IsWithin(n.X, -e, e) && 
       IsWithin(n.Y, -e, e))
    {
      t0 = {1,0,0};
    }
    else
    {
      t0 = n + v3{0,0,100};
    }
    t1 = Normalize(Cross(n, t0));
    t0 = Cross(n, t1);
  }

  f32 Scale = 0.2f;
  v3 Verts[5];
  Verts[0] = V1 - n*Scale + t0 * 0.25f * Scale + t1 * 0.25f * Scale;
  Verts[1] = V1 - n*Scale - t0 * 0.25f * Scale + t1 * 0.25f * Scale;
  Verts[2] = V1 - n*Scale - t0 * 0.25f * Scale - t1 * 0.25f * Scale;
  Verts[3] = V1 - n*Scale + t0 * 0.25f * Scale - t1 * 0.25f * Scale;
  Verts[4] = V1;
  for(int VertIndex = 0; 
      VertIndex < 4; 
      VertIndex++)
  {
    V0 = Verts[(VertIndex+0)%4];
    V1 = Verts[(VertIndex+1)%4];
    LineSegment = 
      ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                             ScreenVars, V0, V1);
    if(LineSegment.PartOfSegmentInView)
    {
      DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Color);
    }
  }
  for(int VertIndex = 0; 
      VertIndex < 4; 
      VertIndex++)
  {
    V0 = Verts[VertIndex];
    V1 = Verts[4];
    LineSegment = 
      ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                             ScreenVars, V0, V1);
    if(LineSegment.PartOfSegmentInView)
    {
      DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Color);
    }
  }
}

#define RenderGroup_DefineEntryAndAdvanceByteOffset(type) \
  type * Entry = (type *)UnidentifiedEntry; \
  PushBufferByteOffset += sizeof(render_entry_header) + sizeof(type)

  internal_function void
RenderGroupToOutput(render_group* RenderGroup, game_bitmap* OutputTarget, f32 ScreenHeightInMeters)
{
  BEGIN_TIMED_BLOCK(RenderGroupToOutput);

  output_target_screen_variables ScreenVars = {};
  {
    Assert(ScreenHeightInMeters > 0);
    f32 PixelsPerMeter = (f32)OutputTarget->Height / ScreenHeightInMeters;
    ScreenVars.MeterToPixel = 
    { PixelsPerMeter,              0,
      0,              PixelsPerMeter};
    ScreenVars.PixelToMeter = 
    { 1.0f/PixelsPerMeter,                   0,
      0,                  1.0f/PixelsPerMeter};
    ScreenVars.Center = OutputTarget->Dim * 0.5f;
  }

  if(RenderGroup->ClearScreen)
  {
    DrawRectangle(OutputTarget, RectMinMax(v2{0.0f, 0.0f}, OutputTarget->Dim), 
                  RenderGroup->ClearScreenColor.RGB);
  }

  for(u32 PushBufferByteOffset = 0;
      PushBufferByteOffset < RenderGroup->PushBufferSize;
      )
  {
    render_entry_header* Header = 
      (render_entry_header*)(RenderGroup->PushBufferBase + PushBufferByteOffset);
    u8* UnidentifiedEntry = 
      RenderGroup->PushBufferBase + PushBufferByteOffset + sizeof(render_entry_header);

    switch(Header->Type)
    {
      case RenderGroupEntryType_render_entry_vector
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_vector);

          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->A, Entry->B, Entry->Color.RGB);
        } break;
      case RenderGroupEntryType_render_entry_coordinate_system
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_coordinate_system);

          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{1,0,0}, v3{1,0,0});
          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{0,1,0}, v3{0,1,0});
          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{0,0,1}, v3{0,0,1});
        } break;
      case RenderGroupEntryType_render_entry_wire_cube
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_wire_cube);

          aabb_verts_result AABB = GetAABBVertices(&Entry->Tran);

          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[(VertIndex+0)%4];
            v3 V1 = AABB.Verts[(VertIndex+1)%4];
            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                     &ScreenVars, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB);
            }
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[(VertIndex+0)%4 + 4];
            v3 V1 = AABB.Verts[(VertIndex+1)%4 + 4];
            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                     &ScreenVars, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB);
            }
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[VertIndex];
            v3 V1 = AABB.Verts[VertIndex + 4];
            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                     &ScreenVars, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB);
            }
          }
        } break;
      case RenderGroupEntryType_render_entry_blank_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_blank_quad);

          //TODO(bjorn): Think about how and when in the pipeline to render the hit-points.
#if 0
          v2 PixelDim = Entry->Dim.XY * (PixelsPerMeter * f);
          rectangle2 Rect = RectCenterDim(PixelPos, PixelDim);

          DrawRectangle(OutputTarget, Rect, Entry->Color.RGB);
#endif
        } break;
      case RenderGroupEntryType_render_entry_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad);

          quad_verts_result Quad = GetQuadVertices(&Entry->Tran);

          v2 PixVerts[4] = {};
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = Quad.Verts[VertIndex];
            v3 V1 = Quad.Verts[(VertIndex + 1)%4];

            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                     &ScreenVars, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              PixVerts[VertIndex] = LineSegment.A;
            }
          }

          DrawTriangleSlowly(OutputTarget, 
                             &RenderGroup->CamParam, &ScreenVars, 
                             RenderGroup->WorldToCamera * Quad.Verts[0], 
                             RenderGroup->WorldToCamera * Quad.Verts[1], 
                             RenderGroup->WorldToCamera * Quad.Verts[2], 
                             {0,0},
                             {0,1},
                             {1,1},
                             Entry->Bitmap,
                             Entry->Color);
          DrawTriangleSlowly(OutputTarget, 
                             &RenderGroup->CamParam, &ScreenVars,
                             RenderGroup->WorldToCamera * Quad.Verts[0], 
                             RenderGroup->WorldToCamera * Quad.Verts[2], 
                             RenderGroup->WorldToCamera * Quad.Verts[3], 
                             {0,0},
                             {1,1},
                             {1,0},
                             Entry->Bitmap,
                             Entry->Color);

#if 0
          DrawLine(OutputTarget, PixVerts[0], PixVerts[2], {1.0f, 0.25f, 1.0f});
          DrawCircle(OutputTarget, (PixVerts[0] + PixVerts[2]) * 0.5f, 3.0f, {1.0f, 1.0f, 0.0f, 1.0f});

          pixel_pos_result PixPos = 
            ProjectPointToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                 &ScreenVars, (Quad.Verts[0]+Quad.Verts[2])*0.5f);
          if(PixPos.PointIsInView)
          {
            DrawCircle(OutputTarget, PixPos.P, 3.0f, {0.0f, 1.0f, 1.0f, 1.0f});
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            DrawLine(OutputTarget, PixVerts[VertIndex], PixVerts[(VertIndex + 1)%4], 
                     {1.0f, VertIndex*0.25f, 1.0f});
          }
#endif
        } break;
      case RenderGroupEntryType_render_entry_sphere
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_sphere);

          v3 P0 = Entry->Tran * v3{0,0,0};
          v3 P1 = Entry->Tran * v3{0.5f,0,0};

          pixel_pos_result PixPos = 
            ProjectPointToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                 &ScreenVars, P0);
          if(PixPos.PointIsInView)
          {
            f32 PixR = (ScreenVars.MeterToPixel.A * 
                        Magnitude(P1 - P0) * 
                        PixPos.PerspectiveCorrection);

            DrawCircle(OutputTarget, PixPos.P, PixR, Entry->Color);
          }
        } break;
      InvalidDefaultCase;
    }
  }

  END_TIMED_BLOCK(RenderGroupToOutput);
}


#define RENDER_GROUP_H
#endif
