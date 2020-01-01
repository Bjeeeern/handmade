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

#if COMPILER_MSVC
#pragma warning(disable:4701)
#endif
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

  __m128 ColorModifierR = _mm_set1_ps(RGBA.R);
  __m128 ColorModifierG = _mm_set1_ps(RGBA.G);
  __m128 ColorModifierB = _mm_set1_ps(RGBA.B);
  __m128 ColorModifierA = _mm_set1_ps(RGBA.A);

  b32 IsOrthogonal = CamParam->LensChamberSize == positive_infinity32;

  //NOTE(bjorn): Ortographic barycentric calculation
  __m128 Const1 = _mm_set1_ps(1.0f);
  __m128 Const0 = _mm_set1_ps(0.0f);
  __m128 Const255 = _mm_set1_ps(255.0f);

  __m128 PixelSpacePoint0X = _mm_set1_ps(PixelSpacePoint0.X);
  __m128 PixelSpacePoint0Y = _mm_set1_ps(PixelSpacePoint0.Y);

  __m128 PixelSpacePoint1X = _mm_set1_ps(PixelSpacePoint1.X);
  __m128 PixelSpacePoint1Y = _mm_set1_ps(PixelSpacePoint1.Y);

  __m128 PixelSpacePoint2X = _mm_set1_ps(PixelSpacePoint2.X);
  __m128 PixelSpacePoint2Y = _mm_set1_ps(PixelSpacePoint2.Y);

  __m128 Bary2D_V1YmV2Y = _mm_sub_ps(PixelSpacePoint1Y, PixelSpacePoint2Y);
  __m128 Bary2D_V0XmV2X = _mm_sub_ps(PixelSpacePoint0X, PixelSpacePoint2X);
  __m128 Bary2D_V2XmV1X = _mm_sub_ps(PixelSpacePoint2X, PixelSpacePoint1X);
  __m128 Bary2D_V0YmV2Y = _mm_sub_ps(PixelSpacePoint0Y, PixelSpacePoint2Y);
  __m128 Bary2D_V2YmV0Y = _mm_sub_ps(PixelSpacePoint2Y, PixelSpacePoint0Y);
  __m128 Bary2D_InvDenom = 
    _mm_div_ps(Const1, 
               _mm_add_ps( _mm_mul_ps(Bary2D_V1YmV2Y, Bary2D_V0XmV2X), 
                           _mm_mul_ps(Bary2D_V2XmV1X, Bary2D_V0YmV2Y)));

  //NOTE(bjorn): Perspective barycentric calculation
  __m128 PixelToMeter = _mm_set1_ps(ScreenVars->PixelToMeter.A);
  __m128 ScreenCenterX = _mm_set1_ps(ScreenVars->Center.X);
  __m128 ScreenCenterY = _mm_set1_ps(ScreenVars->Center.Y);

  __m128 FocalPointZ = _mm_set1_ps(FocalPoint.Z);
  __m128 Bary3D_Numerator = _mm_set1_ps(LinePlaneIntersection_Numerator);

  //TODO(bjorn): Deal with artefacts due to indirect calculation of the last weight.
  __m128 Bary3D_Epsilon = _mm_set1_ps(0.0001f);

  v3 Bary3D_TotalAreaVector = Cross(CameraSpacePoint2-CameraSpacePoint0, 
                                    CameraSpacePoint1-CameraSpacePoint0);
  __m128 Bary3D_TotalAreaVectorX = _mm_set1_ps(Bary3D_TotalAreaVector.X);
  __m128 Bary3D_TotalAreaVectorY = _mm_set1_ps(Bary3D_TotalAreaVector.Y);
  __m128 Bary3D_TotalAreaVectorZ = _mm_set1_ps(Bary3D_TotalAreaVector.Z);

  __m128 Bary3D_TriangleNormalX = _mm_set1_ps(TriangleNormal.X);
  __m128 Bary3D_TriangleNormalY = _mm_set1_ps(TriangleNormal.Y);

  __m128 nFocalPointZ_mul_TriangleNormalZ = _mm_set1_ps(-FocalPoint.Z * TriangleNormal.Z);

  __m128 CameraSpacePoint0X = _mm_set1_ps(CameraSpacePoint0.X);
  __m128 CameraSpacePoint0Y = _mm_set1_ps(CameraSpacePoint0.Y);
  __m128 CameraSpacePoint0Z = _mm_set1_ps(CameraSpacePoint0.Z);

  __m128 CameraSpacePoint1X = _mm_set1_ps(CameraSpacePoint1.X);
  __m128 CameraSpacePoint1Y = _mm_set1_ps(CameraSpacePoint1.Y);
  __m128 CameraSpacePoint1Z = _mm_set1_ps(CameraSpacePoint1.Z);

  __m128 CameraSpacePoint2X = _mm_set1_ps(CameraSpacePoint2.X);
  __m128 CameraSpacePoint2Y = _mm_set1_ps(CameraSpacePoint2.Y);
  __m128 CameraSpacePoint2Z = _mm_set1_ps(CameraSpacePoint2.Z);

  __m128 UV0U = _mm_set1_ps(UV0.U);
  __m128 UV0V = _mm_set1_ps(UV0.V);
                                 
  __m128 UV1U = _mm_set1_ps(UV1.U);
  __m128 UV1V = _mm_set1_ps(UV1.V);
                                 
  __m128 UV2U = _mm_set1_ps(UV2.U);
  __m128 UV2V = _mm_set1_ps(UV2.V);

  __m128 UVBitmapWidth  = _mm_set1_ps((f32)(Bitmap->Width -2));
  __m128 UVBitmapHeight = _mm_set1_ps((f32)(Bitmap->Height-2));

  u32 DefaultSSERoundingMode = _MM_GET_ROUNDING_MODE();
  _MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);

  __m128 Inv255 = _mm_set1_ps( 1.0f/255.0f);
  __m128 SquareInv255 = _mm_set1_ps(Square(1.0f/255.0f));

  __m128i Mask0xFF = _mm_set1_epi32(0x000000FF);

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
      __m128 PixelPointX = _mm_cvtepi32_ps(_mm_set_epi32(IX+3, IX+2, IX+1, IX));
      __m128 PixelPointY = _mm_cvtepi32_ps(_mm_set1_epi32(Y));

      __m128 BarycentricWeight0;
      __m128 BarycentricWeight1;
      __m128 BarycentricWeight2;

      if(IsOrthogonal)
      {
        __m128 PXmV2X = _mm_sub_ps(PixelPointX, PixelSpacePoint2X);
        __m128 PYmV2Y = _mm_sub_ps(PixelPointY, PixelSpacePoint2Y);

        BarycentricWeight0 = 
          _mm_mul_ps(_mm_add_ps(_mm_mul_ps(Bary2D_V1YmV2Y, PXmV2X), 
                                _mm_mul_ps(Bary2D_V2XmV1X, PYmV2Y)), 
                     Bary2D_InvDenom); 
        BarycentricWeight1 = 
          _mm_mul_ps(_mm_add_ps(_mm_mul_ps(Bary2D_V2YmV0Y, PXmV2X), 
                                _mm_mul_ps(Bary2D_V0XmV2X, PYmV2Y)), 
                     Bary2D_InvDenom); 
        BarycentricWeight2 = _mm_sub_ps(Const1, _mm_add_ps(BarycentricWeight0, BarycentricWeight1));
      }
      else
      {
        //NOTE(bjorn): Source: https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
        __m128 PointX;
        __m128 PointY;
        __m128 PointZ;
        {
          __m128 PointInCameraSpaceX = 
            _mm_mul_ps(PixelToMeter, _mm_sub_ps(PixelPointX, ScreenCenterX));
          __m128 PointInCameraSpaceY = 
            _mm_mul_ps(PixelToMeter, _mm_sub_ps(PixelPointY, ScreenCenterY));

          __m128 Denominator = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(PointInCameraSpaceX, Bary3D_TriangleNormalX),
                                  _mm_mul_ps(PointInCameraSpaceY, Bary3D_TriangleNormalY)),
                       nFocalPointZ_mul_TriangleNormalZ);
          __m128 d = _mm_div_ps(Bary3D_Numerator, Denominator);

          PointX = _mm_mul_ps(d, PointInCameraSpaceX);
          PointY = _mm_mul_ps(d, PointInCameraSpaceY);
          PointZ = _mm_mul_ps(_mm_sub_ps(Const1, d), FocalPointZ);
        }

        __m128 V0PX = _mm_sub_ps(CameraSpacePoint0X, PointX);
        __m128 V0PY = _mm_sub_ps(CameraSpacePoint0Y, PointY);
        __m128 V0PZ = _mm_sub_ps(CameraSpacePoint0Z, PointZ);

        __m128 V1PX = _mm_sub_ps(CameraSpacePoint1X, PointX);
        __m128 V1PY = _mm_sub_ps(CameraSpacePoint1Y, PointY);
        __m128 V1PZ = _mm_sub_ps(CameraSpacePoint1Z, PointZ);

        __m128 V2PX = _mm_sub_ps(CameraSpacePoint2X, PointX);
        __m128 V2PY = _mm_sub_ps(CameraSpacePoint2Y, PointY);
        __m128 V2PZ = _mm_sub_ps(CameraSpacePoint2Z, PointZ);

        __m128 AreaAVectorX = _mm_sub_ps(_mm_mul_ps(V2PY, V1PZ), _mm_mul_ps(V2PZ, V1PY));
        __m128 AreaAVectorY = _mm_sub_ps(_mm_mul_ps(V2PZ, V1PX), _mm_mul_ps(V2PX, V1PZ));
        __m128 AreaAVectorZ = _mm_sub_ps(_mm_mul_ps(V2PX, V1PY), _mm_mul_ps(V2PY, V1PX));

        __m128 AreaBVectorX = _mm_sub_ps(_mm_mul_ps(V0PY, V2PZ), _mm_mul_ps(V0PZ, V2PY));
        __m128 AreaBVectorY = _mm_sub_ps(_mm_mul_ps(V0PZ, V2PX), _mm_mul_ps(V0PX, V2PZ));
        __m128 AreaBVectorZ = _mm_sub_ps(_mm_mul_ps(V0PX, V2PY), _mm_mul_ps(V0PY, V2PX));

        __m128 DotAB = _mm_add_ps(_mm_add_ps(_mm_mul_ps(AreaAVectorX, AreaBVectorX),
                                             _mm_mul_ps(AreaAVectorY, AreaBVectorY)),
                                  _mm_mul_ps(AreaAVectorZ, AreaBVectorZ));
        __m128 DotBB = _mm_add_ps(_mm_add_ps(_mm_mul_ps(AreaBVectorX, AreaBVectorX),
                                             _mm_mul_ps(AreaBVectorY, AreaBVectorY)),
                                  _mm_mul_ps(AreaBVectorZ, AreaBVectorZ));
        __m128 InvDotTB  = 
          _mm_div_ps(Const1, 
                     _mm_add_ps(_mm_add_ps(_mm_mul_ps(Bary3D_TotalAreaVectorX, AreaBVectorX),
                                           _mm_mul_ps(Bary3D_TotalAreaVectorY, AreaBVectorY)),
                                _mm_mul_ps(Bary3D_TotalAreaVectorZ, AreaBVectorZ)));

        BarycentricWeight0 = _mm_mul_ps(DotAB, InvDotTB);
        BarycentricWeight1 = _mm_mul_ps(DotBB, InvDotTB);
        BarycentricWeight2 = _mm_sub_ps(Const1, _mm_add_ps(BarycentricWeight0, BarycentricWeight1));

        BarycentricWeight0 = _mm_add_ps(BarycentricWeight0, Bary3D_Epsilon);
        BarycentricWeight1 = _mm_add_ps(BarycentricWeight1, Bary3D_Epsilon);
        BarycentricWeight2 = _mm_add_ps(BarycentricWeight2, Bary3D_Epsilon);
      }
      __m128i DrawMask = 
        _mm_castps_si128(_mm_and_ps(_mm_and_ps(_mm_cmpge_ps(BarycentricWeight0, Const0), 
                                               _mm_cmpge_ps(BarycentricWeight1, Const0)),
                                    _mm_cmpge_ps(BarycentricWeight2, Const0)));
      if(_mm_movemask_epi8(DrawMask))
      {
        __m128i Dest = _mm_loadu_si128((__m128i*)Pixel);

        __m128 DestR = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Dest, 16), Mask0xFF));
        __m128 DestG = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Dest,  8), Mask0xFF));
        __m128 DestB = _mm_cvtepi32_ps(_mm_and_si128(               Dest,      Mask0xFF));
        __m128 DestA = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Dest, 24), Mask0xFF));

        __m128 TexelR;
        __m128 TexelG;
        __m128 TexelB;
        __m128 TexelA;
        if(Bitmap)
        {
          __m128 U = _mm_add_ps(_mm_add_ps(_mm_mul_ps(BarycentricWeight0, UV0U), 
                                           _mm_mul_ps(BarycentricWeight1, UV1U)), 
                                _mm_mul_ps(BarycentricWeight2, UV2U));
          __m128 V = _mm_add_ps(_mm_add_ps(_mm_mul_ps(BarycentricWeight0, UV0V), 
                                           _mm_mul_ps(BarycentricWeight1, UV1V)), 
                                _mm_mul_ps(BarycentricWeight2, UV2V));

          U = _mm_max_ps(Const0, _mm_min_ps(U, Const1));
          V = _mm_max_ps(Const0, _mm_min_ps(V, Const1));

          __m128 tX = _mm_mul_ps(U, UVBitmapWidth); 
          __m128 tY = _mm_mul_ps(V, UVBitmapHeight);

          __m128i wX = _mm_cvttps_epi32(tX);
          __m128i wY = _mm_cvttps_epi32(tY);

          __m128 fX = _mm_sub_ps(tX, _mm_cvtepi32_ps(wX));
          __m128 fY = _mm_sub_ps(tY, _mm_cvtepi32_ps(wY));

          __m128i TexSmp0;
          __m128i TexSmp1;
          __m128i TexSmp2;
          __m128i TexSmp3;
          for(s32 I = 0;
              I < 4;
              I++)
          {
            u32* TexelPtr = Bitmap->Memory + Bitmap->Pitch * wY.m128i_i32[I] + wX.m128i_i32[I];
            u32* TexelPtr0 = TexelPtr;
            u32* TexelPtr1 = TexelPtr + 1;
            u32* TexelPtr2 = TexelPtr + Bitmap->Pitch;
            u32* TexelPtr3 = TexelPtr + Bitmap->Pitch + 1;

            TexSmp0.m128i_i32[I] = *TexelPtr0;
            TexSmp1.m128i_i32[I] = *TexelPtr1;
            TexSmp2.m128i_i32[I] = *TexelPtr2;
            TexSmp3.m128i_i32[I] = *TexelPtr3;
          }

          __m128 TexSmp0R = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp0, 16), Mask0xFF));
          __m128 TexSmp0G = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp0,  8), Mask0xFF));
          __m128 TexSmp0B = _mm_cvtepi32_ps(_mm_and_si128(               TexSmp0,      Mask0xFF));
          __m128 TexSmp0A = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp0, 24), Mask0xFF));

          __m128 TexSmp1R = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp1, 16), Mask0xFF));
          __m128 TexSmp1G = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp1,  8), Mask0xFF));
          __m128 TexSmp1B = _mm_cvtepi32_ps(_mm_and_si128(               TexSmp1,      Mask0xFF));
          __m128 TexSmp1A = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp1, 24), Mask0xFF));

          __m128 TexSmp2R = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp2, 16), Mask0xFF));
          __m128 TexSmp2G = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp2,  8), Mask0xFF));
          __m128 TexSmp2B = _mm_cvtepi32_ps(_mm_and_si128(               TexSmp2,      Mask0xFF));
          __m128 TexSmp2A = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp2, 24), Mask0xFF));

          __m128 TexSmp3R = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp3, 16), Mask0xFF));
          __m128 TexSmp3G = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp3,  8), Mask0xFF));
          __m128 TexSmp3B = _mm_cvtepi32_ps(_mm_and_si128(               TexSmp3,      Mask0xFF));
          __m128 TexSmp3A = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(TexSmp3, 24), Mask0xFF));

          TexSmp0R = _mm_mul_ps(TexSmp0R, _mm_mul_ps(TexSmp0R, SquareInv255));
          TexSmp0G = _mm_mul_ps(TexSmp0G, _mm_mul_ps(TexSmp0G, SquareInv255));
          TexSmp0B = _mm_mul_ps(TexSmp0B, _mm_mul_ps(TexSmp0B, SquareInv255));
          TexSmp0A = _mm_mul_ps(TexSmp0A, Inv255);

          TexSmp1R = _mm_mul_ps(TexSmp1R, _mm_mul_ps(TexSmp1R, SquareInv255));
          TexSmp1G = _mm_mul_ps(TexSmp1G, _mm_mul_ps(TexSmp1G, SquareInv255));
          TexSmp1B = _mm_mul_ps(TexSmp1B, _mm_mul_ps(TexSmp1B, SquareInv255));
          TexSmp1A = _mm_mul_ps(TexSmp1A, Inv255);

          TexSmp2R = _mm_mul_ps(TexSmp2R, _mm_mul_ps(TexSmp2R, SquareInv255));
          TexSmp2G = _mm_mul_ps(TexSmp2G, _mm_mul_ps(TexSmp2G, SquareInv255));
          TexSmp2B = _mm_mul_ps(TexSmp2B, _mm_mul_ps(TexSmp2B, SquareInv255));
          TexSmp2A = _mm_mul_ps(TexSmp2A, Inv255);

          TexSmp3R = _mm_mul_ps(TexSmp3R, _mm_mul_ps(TexSmp3R, SquareInv255));
          TexSmp3G = _mm_mul_ps(TexSmp3G, _mm_mul_ps(TexSmp3G, SquareInv255));
          TexSmp3B = _mm_mul_ps(TexSmp3B, _mm_mul_ps(TexSmp3B, SquareInv255));
          TexSmp3A = _mm_mul_ps(TexSmp3A, Inv255);

          __m128 ifY = _mm_sub_ps(Const1, fY);
          __m128 ifX = _mm_sub_ps(Const1, fX);
          __m128 ifYmifX = _mm_mul_ps(ifY, ifX);
          __m128 ifYmfX  = _mm_mul_ps(ifY,  fX);
          __m128 ifXmfY  = _mm_mul_ps(ifX,  fY);
          __m128 fXmfY   = _mm_mul_ps( fX,  fY);

          TexelR = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(ifYmifX, TexSmp0R), _mm_mul_ps(ifYmfX, TexSmp1R)), 
                       _mm_add_ps(_mm_mul_ps(ifXmfY,  TexSmp2R), _mm_mul_ps(fXmfY,  TexSmp3R)));
          TexelG = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(ifYmifX, TexSmp0G), _mm_mul_ps(ifYmfX, TexSmp1G)), 
                       _mm_add_ps(_mm_mul_ps(ifXmfY,  TexSmp2G), _mm_mul_ps(fXmfY,  TexSmp3G)));
          TexelB = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(ifYmifX, TexSmp0B), _mm_mul_ps(ifYmfX, TexSmp1B)), 
                       _mm_add_ps(_mm_mul_ps(ifXmfY,  TexSmp2B), _mm_mul_ps(fXmfY,  TexSmp3B)));
          TexelA = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(ifYmifX, TexSmp0A), _mm_mul_ps(ifYmfX, TexSmp1A)), 
                       _mm_add_ps(_mm_mul_ps(ifXmfY,  TexSmp2A), _mm_mul_ps(fXmfY,  TexSmp3A)));

          TexelR = _mm_mul_ps(TexelR, ColorModifierR);
          TexelG = _mm_mul_ps(TexelG, ColorModifierG);
          TexelB = _mm_mul_ps(TexelB, ColorModifierB);
          TexelA = _mm_mul_ps(TexelA, ColorModifierA);
        }
        else
        {
          TexelR = ColorModifierR;
          TexelG = ColorModifierG;
          TexelB = ColorModifierB;
          TexelA = ColorModifierA;
        }

        DestR = _mm_mul_ps(DestR, _mm_mul_ps(DestR, SquareInv255));
        DestG = _mm_mul_ps(DestG, _mm_mul_ps(DestG, SquareInv255));
        DestB = _mm_mul_ps(DestB, _mm_mul_ps(DestB, SquareInv255));
        DestA = _mm_mul_ps(DestA, Inv255);

        __m128 InvTexelA = _mm_sub_ps(Const1, TexelA);
        __m128 BlendedR = _mm_add_ps(_mm_mul_ps(DestR, InvTexelA), TexelR);
        __m128 BlendedG = _mm_add_ps(_mm_mul_ps(DestG, InvTexelA), TexelG);
        __m128 BlendedB = _mm_add_ps(_mm_mul_ps(DestB, InvTexelA), TexelB);
        __m128 BlendedA = _mm_add_ps(_mm_mul_ps(DestA, InvTexelA), TexelA);

        BlendedR = _mm_mul_ps(_mm_sqrt_ps(BlendedR), Const255);
        BlendedG = _mm_mul_ps(_mm_sqrt_ps(BlendedG), Const255);
        BlendedB = _mm_mul_ps(_mm_sqrt_ps(BlendedB), Const255);
        BlendedA = _mm_mul_ps(BlendedA, Const255);

        BlendedR = _mm_max_ps(Const0, _mm_min_ps(BlendedR, Const255));
        BlendedG = _mm_max_ps(Const0, _mm_min_ps(BlendedG, Const255));
        BlendedB = _mm_max_ps(Const0, _mm_min_ps(BlendedB, Const255));
        BlendedA = _mm_max_ps(Const0, _mm_min_ps(BlendedA, Const255));

        __m128i IntR = _mm_cvtps_epi32(BlendedR);
        __m128i IntG = _mm_cvtps_epi32(BlendedG);
        __m128i IntB = _mm_cvtps_epi32(BlendedB);
        __m128i IntA = _mm_cvtps_epi32(BlendedA);

        IntR = _mm_slli_epi32(IntR, 16);
        IntG = _mm_slli_epi32(IntG,  8);
        IntA = _mm_slli_epi32(IntA, 24);

        __m128i Out = _mm_or_si128(_mm_or_si128(IntR,
                                                IntG), 
                                   _mm_or_si128(IntB,
                                                IntA));
        __m128i MaskedOut = _mm_or_si128(_mm_and_si128(DrawMask, Out),
                                         _mm_andnot_si128(DrawMask, Dest));
        _mm_storeu_si128((__m128i*)Pixel, MaskedOut);
      }

      Pixel += 4;
    }

    UpperLeftPixel += Buffer->Pitch;
  }

  END_TIMED_BLOCK_COUNTED(ProcessPixel, (Right-Left) * (Top-Bottom));

  _MM_SET_ROUNDING_MODE(DefaultSSERoundingMode);

  END_TIMED_BLOCK(DrawTriangleSlowly);
}
#if COMPILER_MSVC
#pragma warning(default:4701)
#endif

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
