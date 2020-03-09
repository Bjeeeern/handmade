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
DrawRectangle(game_bitmap *Buffer, rectangle2s Rect, v3 RGB, rectangle2s ClipRect)
{
  rectangle2s FillRect = Intersect(Rect, ClipRect);
  if(HasNoArea(FillRect)) { return; }

  s32 Left   = FillRect.Min.X;
  s32 Bottom = FillRect.Min.Y;
  s32 Right  = FillRect.Max.X;
  s32 Top    = FillRect.Max.Y;

  u32 Color = ((RoundF32ToS32(RGB.R * 255.0f) << 16) |
               (RoundF32ToS32(RGB.G * 255.0f) << 8) |
               (RoundF32ToS32(RGB.B * 255.0f) << 0));

  s32 PixelPitch = Buffer->Pitch;

  u32 *UpperLeftPixel = (u32 *)Buffer->Memory + Left + Bottom * PixelPitch;

  for(s32 Y = Bottom;
      Y < Top;
      Y++)
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

	internal_function void
DrawLine(game_bitmap* Buffer, 
         v2 A, v2 B, v3 RGB,
         rectangle2s ClipRect)
{
  //TODO(bjorn): Implement.
  v2s Min = { Min(FloorF32ToS32(A.X), FloorF32ToS32(B.X)), 
              Min(FloorF32ToS32(A.Y), FloorF32ToS32(B.Y))};
  v2s Max = { Max(RoofF32ToS32(A.X), RoofF32ToS32(B.X)), 
              Max(RoofF32ToS32(A.Y), RoofF32ToS32(B.Y))};
  rectangle2s FillRect = RectMinMax(Min, Max);

  FillRect = Intersect(FillRect, ClipRect);
  //TODO(bjorn): if(HasNoArea(FillRect)) { return; }

  rect_corner_v2_result Corner = GetRectCorners(Rect2sToRect2(FillRect));
  {
    b32 AOutside = (A.X < FillRect.Min.X);
    b32 BOutside = (B.X < FillRect.Min.X);
    if(AOutside && BOutside) { return; }

    if(AOutside || BOutside)
    {
      line_line_intersect Insect = LineLineIntersection(A, B, Corner.A, Corner.B);
      if(Insect.Hit)
      {
        if(AOutside) { A = A + (B-A) * Insect.t; }
        if(BOutside) { B = A + (B-A) * Insect.t; }
      }
    }
  }
  {
    b32 AOutside = (A.Y >= FillRect.Max.Y);
    b32 BOutside = (B.Y >= FillRect.Max.Y);
    if(AOutside && BOutside) { return; }

    if(AOutside || BOutside)
    {
      line_line_intersect Insect = LineLineIntersection(A, B, Corner.B, Corner.C);
      if(Insect.Hit)
      {
        if(AOutside) { A = A + (B-A) * Insect.t; }
        if(BOutside) { B = A + (B-A) * Insect.t; }
      }
    }
  }
  {
    b32 AOutside = (A.X >= FillRect.Max.X);
    b32 BOutside = (B.X >= FillRect.Max.X);
    if(AOutside && BOutside) { return; }

    if(AOutside || BOutside)
    {
      line_line_intersect Insect = LineLineIntersection(A, B, Corner.C, Corner.D);
      if(Insect.Hit)
      {
        if(AOutside) { A = A + (B-A) * Insect.t; }
        if(BOutside) { B = A + (B-A) * Insect.t; }
      }
    }
  }
  {
    b32 AOutside = (A.Y < FillRect.Min.Y);
    b32 BOutside = (B.Y < FillRect.Min.Y);
    if(AOutside && BOutside) { return; }

    if(AOutside || BOutside)
    {
      line_line_intersect Insect = LineLineIntersection(A, B, Corner.D, Corner.A);
      if(Insect.Hit)
      {
        if(AOutside) { A = A + (B-A) * Insect.t; }
        if(BOutside) { B = A + (B-A) * Insect.t; }
      }
    }
  }

  //AddMarginToRect(FillRect, 2);

  u32 Color = ((RoundF32ToS32(RGB.R * 255.0f) << 16) |
               (RoundF32ToS32(RGB.G * 255.0f) << 8) |
               (RoundF32ToS32(RGB.B * 255.0f) << 0));

  f32 Length = Distance(A, B);
  if(Distance(A, B) < 1.0f)
  {
    Length = 1.0f;
  }

  f32 InverseLength = 1.0f / Length;
  v2 Normal = (B - A) * InverseLength;

  b32 FirstRound = true;
  b32 SkipUntilInside = false;

  f32 HalfLength = Length * 0.5f;
  v2 MidPoint = (A+B)*0.5f;

  f32 StepLength = 0.0f;
  while(StepLength < HalfLength)
  {
    v2 Point = MidPoint + (Normal * StepLength);
    s32 X = RoundF32ToS32(Point.X);
    s32 Y = RoundF32ToS32(Point.Y);

    b32 PointHitTest = ((FillRect.Min.X <= X) && (X < FillRect.Max.X) &&
                        (FillRect.Min.Y <= Y) && (Y < FillRect.Max.Y));
    if(PointHitTest)
    {
      u32 *Pixel = (u32 *)Buffer->Memory + Buffer->Pitch * Y + X;
      *Pixel = Color;
    }

    StepLength += 1.0f;
  }

  StepLength = 0.0f;
  while(StepLength < HalfLength)
  {
    v2 Point = MidPoint - (Normal * StepLength);
    s32 X = RoundF32ToS32(Point.X);
    s32 Y = RoundF32ToS32(Point.Y);

    b32 PointHitTest = ((FillRect.Min.X <= X) && (X < FillRect.Max.X) &&
                        (FillRect.Min.Y <= Y) && (Y < FillRect.Max.Y));
    if(PointHitTest)
    {
      u32 *Pixel = (u32 *)Buffer->Memory + Buffer->Pitch * Y + X;
      *Pixel = Color;
    }

    StepLength += 1.0f;
  }
}

//TODO(bjorn): reintroduce this function.
#if 0
  internal_function void
DrawChar_(game_bitmap *Buffer, font *Font, u32 UnicodeCodePoint, 
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

		DrawLine(Buffer, Start, End, v3{RealR, RealG, RealB});
	}
}
#endif

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

struct shader_texel
{
  __m128 R;
  __m128 G;
  __m128 B;
  __m128 A;
};

#define SOFTWARE_SHADER(name) inline void name(shader_texel& In, shader_texel& TextureSample, \
                                               shader_texel& Color, shader_texel& Out, \
                                               __m128 U, __m128 V)
typedef SOFTWARE_SHADER(software_shader);

SOFTWARE_SHADER(BitmapShader)
{
#if 0
  Out.R = _mm_mul_ps(_mm_mul_ps(Const255, Const255), Const0);
  Out.G = _mm_mul_ps(_mm_mul_ps(Const255, Const255), V);
  Out.B = _mm_mul_ps(_mm_mul_ps(Const255, Const255), U);
  Out.A = _mm_mul_ps(Const255, Const1);
#else
  Out.R = _mm_mul_ps(TextureSample.R, Color.R);
  Out.G = _mm_mul_ps(TextureSample.G, Color.G);
  Out.B = _mm_mul_ps(TextureSample.B, Color.B);
  Out.A = _mm_mul_ps(TextureSample.A, Color.A);
#endif
};

SOFTWARE_SHADER(PlainFillShader)
{
  __m128 Const255 = _mm_set1_ps(255.0f);
  __m128 Const255x255 = _mm_set1_ps(255.0f*255.0f);

  Out.R = _mm_mul_ps(Const255x255, Color.R);
  Out.G = _mm_mul_ps(Const255x255, Color.G);
  Out.B = _mm_mul_ps(Const255x255, Color.B);
  Out.A = _mm_mul_ps(Const255, Color.A);
}

SOFTWARE_SHADER(PlainFillFlipShader)
{
  __m128 Const0 = _mm_set1_ps(0.0f);
  __m128 Const255 = _mm_set1_ps(255.0f);
  __m128 Const255x255 = _mm_set1_ps(255.0f*255.0f);

  __m128 FlipMask = _mm_cmpeq_ps(In.A, Const0);
  //TODO(bjorn): The way In and Out blends in the end forces me to edit the In
  //directly in order to actually flip the pixels.
  In.R = _mm_and_ps(Const255x255, FlipMask);
  In.G = _mm_and_ps(Const255x255, FlipMask);
  In.B = _mm_and_ps(Const255x255, FlipMask);
  In.A = _mm_and_ps(Const255,     FlipMask);

  Out.R = Const0;
  Out.G = Const0;
  Out.B = Const0;
  Out.A = Const0;
}

SOFTWARE_SHADER(QuadBezierFlipShader)
{
  __m128 Const0 = _mm_set1_ps(0.0f);
  __m128 Const1 = _mm_set1_ps(1.0f);
  __m128 Const255 = _mm_set1_ps(255.0f);
  __m128 Const255x255 = _mm_set1_ps(255.0f*255.0f);

  __m128 BezierMask = _mm_cmple_ps(_mm_add_ps(_mm_mul_ps(U, U), _mm_mul_ps(V, V)), Const1);
  __m128 FlipMask = _mm_cmpeq_ps(In.A, Const0);
  __m128 CombinedMask = _mm_and_ps(BezierMask, FlipMask);
  //TODO(bjorn): The way In and Out blends in the end forces me to edit the In
  //directly in order to actually flip the pixels.
  shader_texel Tmp;
  Tmp.R = _mm_and_ps(Const255x255, FlipMask);
  Tmp.G = _mm_and_ps(Const255x255, FlipMask);
  Tmp.B = _mm_and_ps(Const255x255, FlipMask);
  Tmp.A = _mm_and_ps(Const255,     FlipMask);

  In.R = _mm_or_ps(_mm_and_ps(BezierMask, Tmp.R),
                   _mm_andnot_ps(BezierMask, In.R));
  In.G = _mm_or_ps(_mm_and_ps(BezierMask, Tmp.G),
                   _mm_andnot_ps(BezierMask, In.G));
  In.B = _mm_or_ps(_mm_and_ps(BezierMask, Tmp.B),
                   _mm_andnot_ps(BezierMask, In.B));
  In.A = _mm_or_ps(_mm_and_ps(BezierMask, Tmp.A),
                   _mm_andnot_ps(BezierMask, In.A));

  Out.R = Const0;
  Out.G = Const0;
  Out.B = Const0;
  Out.A = Const0;
}

#if COMPILER_MSVC
#pragma warning(disable:4701)
#endif
  internal_function void
DrawTriangle(game_bitmap *Buffer, 
             camera_parameters* CamParam, output_target_screen_variables* ScreenVars,
             v3 CameraSpacePoint0, v3 CameraSpacePoint1, v3 CameraSpacePoint2, 
             v2 UV0, v2 UV1, v2 UV2, 
             game_bitmap* Bitmap, v4 RGBA,
             rectangle2s ClipRect, software_shader Shader)
{
  BEGIN_TIMED_BLOCK(DrawTriangle);

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

  rectangle2s FillRect;
  FillRect.Min.X = 
    RoundF32ToS32(Min(Min(PixelSpacePoint0.X, PixelSpacePoint1.X),PixelSpacePoint2.X));
  FillRect.Max.X = 
    RoundF32ToS32(Max(Max(PixelSpacePoint0.X, PixelSpacePoint1.X),PixelSpacePoint2.X));
  FillRect.Min.Y = 
    RoundF32ToS32(Min(Min(PixelSpacePoint0.Y, PixelSpacePoint1.Y),PixelSpacePoint2.Y));
  FillRect.Max.Y = 
    RoundF32ToS32(Max(Max(PixelSpacePoint0.Y, PixelSpacePoint1.Y),PixelSpacePoint2.Y));

  FillRect = Intersect(FillRect, ClipRect);
  if(HasNoArea(FillRect)) { return; }

  __m128i StartClipMask = _mm_set1_epi32(0xFFFFFFFF);
  __m128i EndClipMask   = _mm_set1_epi32(0xFFFFFFFF);

  __m128i StartClipMasks[] = { _mm_slli_si128(StartClipMask, 4*0),
                               _mm_slli_si128(StartClipMask, 4*1),
                               _mm_slli_si128(StartClipMask, 4*2),
                               _mm_slli_si128(StartClipMask, 4*3)};
  __m128i EndClipMasks[]   = { _mm_srli_si128(EndClipMask,   4*0),
                               _mm_srli_si128(EndClipMask,   4*3),
                               _mm_srli_si128(EndClipMask,   4*2),
                               _mm_srli_si128(EndClipMask,   4*1)};

  if(FillRect.Min.X & 3)
  {
    StartClipMask = StartClipMasks[FillRect.Min.X & 3];
    FillRect.Min.X = FillRect.Min.X & ~3;
  }
  if(FillRect.Max.X & 3)
  {
    EndClipMask = EndClipMasks[FillRect.Max.X & 3];
    FillRect.Max.X = (FillRect.Max.X & ~3) + 4;
  }

  v3 FocalPoint = {0,0,CamParam->LensChamberSize};
  v3 TriangleNormal = 
    Cross(CameraSpacePoint1-CameraSpacePoint0, CameraSpacePoint2-CameraSpacePoint0);
  v3 TriangleCenter = (CameraSpacePoint0 + CameraSpacePoint1 + CameraSpacePoint2) * (1.0f/3.0f);

  //TODO(bjorn): Better name here.
  f32 LinePlaneIntersection_Numerator = Dot(TriangleCenter - FocalPoint, TriangleNormal);
  b32 CameraAndTriangleNormalsAreOrthogonal = LinePlaneIntersection_Numerator  == 0;
  if(CameraAndTriangleNormalsAreOrthogonal) { return; }

  RGBA.RGB *= RGBA.A;

  shader_texel ColorModifier;
  ColorModifier.R = _mm_set1_ps(RGBA.R);
  ColorModifier.G = _mm_set1_ps(RGBA.G);
  ColorModifier.B = _mm_set1_ps(RGBA.B);
  ColorModifier.A = _mm_set1_ps(RGBA.A);

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

  __m128 Const0p5 = _mm_set_ps1(0.5f);
  __m128 UVBitmapWidth = {};
  __m128 UVBitmapHeight = {};
  u32* BitmapMemory = 0;
  u32 BitmapPitch = 0;
  __m128i BitmapPitchWide = {};
  if(Bitmap)
  {
    UVBitmapWidth  = _mm_set1_ps((f32)(Bitmap->Width -1));
    UVBitmapHeight = _mm_set1_ps((f32)(Bitmap->Height-1));

    BitmapMemory = Bitmap->Memory;
    BitmapPitch = Bitmap->Pitch;
    BitmapPitchWide = _mm_set1_epi32(Bitmap->Pitch);
  }

  u32 DefaultSSERoundingMode = _MM_GET_ROUNDING_MODE();
  _MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);

  __m128 Inv255 = _mm_set1_ps( 1.0f/255.0f);
  __m128 SquareInv255 = _mm_set1_ps(Square(1.0f/255.0f));

  __m128i Mask0xFF = _mm_set1_epi32(0x000000FF);

  __m128 Const4444 = _mm_set_ps(4.0f, 4.0f, 4.0f, 4.0f);

  s32 MinX = FillRect.Min.X;
  s32 MaxX = FillRect.Max.X;
  s32 MinY = FillRect.Min.Y;
  s32 MaxY = FillRect.Max.Y;
  __m128 LeftmostX = _mm_cvtepi32_ps(_mm_setr_epi32(MinX+0, MinX+1, MinX+2, MinX+3));

  s32 RowAdvance = Buffer->Pitch;

  u32 *UpperLeftPixel = Buffer->Memory + MinY * Buffer->Pitch + MinX;
  BEGIN_TIMED_BLOCK(ProcessPixel);
  for(s32 Y = MinY;
      Y < MaxY;
      Y++)
  {
    u32 *Pixel = UpperLeftPixel;

    __m128 PixelPointX = LeftmostX;
    __m128 PixelPointY = _mm_cvtepi32_ps(_mm_set1_epi32(Y));

    __m128i ClipMask = StartClipMask;

    for(s32 IX = MinX;
        IX < MaxX;
        IX += 4)
    {
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

#if 0
        //TODO(bjorn): Deal with artefacts due to indirect calculation of the last weight.
        __m128 Bary3D_Epsilon = _mm_set1_ps(0.0001f);

        BarycentricWeight0 = _mm_add_ps(BarycentricWeight0, Bary3D_Epsilon);
        BarycentricWeight1 = _mm_add_ps(BarycentricWeight1, Bary3D_Epsilon);
        BarycentricWeight2 = _mm_add_ps(BarycentricWeight2, Bary3D_Epsilon);
#endif
      }

      __m128i DrawMask = 
        _mm_castps_si128(_mm_and_ps(_mm_and_ps(_mm_cmpge_ps(BarycentricWeight0, Const0), 
                                               _mm_cmpge_ps(BarycentricWeight1, Const0)),
                                    _mm_cmpge_ps(BarycentricWeight2, Const0)));
      DrawMask = _mm_and_si128(DrawMask, ClipMask);
      if(_mm_movemask_epi8(DrawMask))
      {
        __m128  U = _mm_add_ps(_mm_add_ps(_mm_mul_ps(BarycentricWeight0, UV0U), 
                                          _mm_mul_ps(BarycentricWeight1, UV1U)), 
                               _mm_mul_ps(BarycentricWeight2, UV2U));
        __m128  V = _mm_add_ps(_mm_add_ps(_mm_mul_ps(BarycentricWeight0, UV0V), 
                                          _mm_mul_ps(BarycentricWeight1, UV1V)), 
                               _mm_mul_ps(BarycentricWeight2, UV2V));

        U = _mm_max_ps(Const0, _mm_min_ps(U, Const1));
        V = _mm_max_ps(Const0, _mm_min_ps(V, Const1));

        __m128i DestRaw = _mm_loadu_si128((__m128i*)Pixel);

        shader_texel Dest;
        Dest.R = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(DestRaw, 16), Mask0xFF));
        Dest.G = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(DestRaw,  8), Mask0xFF));
        Dest.B = _mm_cvtepi32_ps(_mm_and_si128(               DestRaw,      Mask0xFF));
        Dest.A = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(DestRaw, 24), Mask0xFF));

        Dest.R = _mm_mul_ps(Dest.R, Dest.R);
        Dest.G = _mm_mul_ps(Dest.G, Dest.G);
        Dest.B = _mm_mul_ps(Dest.B, Dest.B);

        shader_texel Texel;
        shader_texel TexSmp;
        if(Bitmap)
        {
          __m128 tX = _mm_mul_ps(U, UVBitmapWidth); 
          __m128 tY = _mm_mul_ps(V, UVBitmapHeight);

          __m128i wX = _mm_cvttps_epi32(tX);
          __m128i wY = _mm_cvttps_epi32(tY);

          __m128 fX = _mm_sub_ps(tX, _mm_cvtepi32_ps(wX));
          __m128 fY = _mm_sub_ps(tY, _mm_cvtepi32_ps(wY));

          __m128i Fetch = 
            _mm_add_epi32(_mm_or_si128(_mm_mullo_epi16(BitmapPitchWide, wY),
                                       _mm_slli_epi32(_mm_mulhi_epi16(BitmapPitchWide, wY), 16)), 
                          wX);
          u32* TexelPtr0 = BitmapMemory + Fetch.m128i_i32[0];
          u32* TexelPtr1 = BitmapMemory + Fetch.m128i_i32[1];
          u32* TexelPtr2 = BitmapMemory + Fetch.m128i_i32[2];
          u32* TexelPtr3 = BitmapMemory + Fetch.m128i_i32[3];

          __m128i TexSmp0 = _mm_setr_epi32(*TexelPtr0,
                                           *TexelPtr1,
                                           *TexelPtr2,
                                           *TexelPtr3);

          __m128i TexSmp1 = _mm_setr_epi32(*(TexelPtr0 + 1),
                                           *(TexelPtr1 + 1),
                                           *(TexelPtr2 + 1),
                                           *(TexelPtr3 + 1));

          __m128i TexSmp2 = _mm_setr_epi32(*(TexelPtr0 + BitmapPitch),
                                           *(TexelPtr1 + BitmapPitch),
                                           *(TexelPtr2 + BitmapPitch),
                                           *(TexelPtr3 + BitmapPitch));

          __m128i TexSmp3 = _mm_setr_epi32(*(TexelPtr0 + BitmapPitch + 1),
                                           *(TexelPtr1 + BitmapPitch + 1),
                                           *(TexelPtr2 + BitmapPitch + 1),
                                           *(TexelPtr3 + BitmapPitch + 1));

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

          TexSmp0R = _mm_mul_ps(TexSmp0R, TexSmp0R);
          TexSmp0G = _mm_mul_ps(TexSmp0G, TexSmp0G);
          TexSmp0B = _mm_mul_ps(TexSmp0B, TexSmp0B);

          TexSmp1R = _mm_mul_ps(TexSmp1R, TexSmp1R);
          TexSmp1G = _mm_mul_ps(TexSmp1G, TexSmp1G);
          TexSmp1B = _mm_mul_ps(TexSmp1B, TexSmp1B);

          TexSmp2R = _mm_mul_ps(TexSmp2R, TexSmp2R);
          TexSmp2G = _mm_mul_ps(TexSmp2G, TexSmp2G);
          TexSmp2B = _mm_mul_ps(TexSmp2B, TexSmp2B);

          TexSmp3R = _mm_mul_ps(TexSmp3R, TexSmp3R);
          TexSmp3G = _mm_mul_ps(TexSmp3G, TexSmp3G);
          TexSmp3B = _mm_mul_ps(TexSmp3B, TexSmp3B);

          __m128 ifY = _mm_sub_ps(Const1, fY);
          __m128 ifX = _mm_sub_ps(Const1, fX);
          __m128 ifYmifX = _mm_mul_ps(ifY, ifX);
          __m128 ifYmfX  = _mm_mul_ps(ifY,  fX);
          __m128 ifXmfY  = _mm_mul_ps(ifX,  fY);
          __m128 fXmfY   = _mm_mul_ps( fX,  fY);

          TexSmp.R = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(ifYmifX, TexSmp0R), _mm_mul_ps(ifYmfX, TexSmp1R)), 
                       _mm_add_ps(_mm_mul_ps(ifXmfY,  TexSmp2R), _mm_mul_ps(fXmfY,  TexSmp3R)));
          TexSmp.G = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(ifYmifX, TexSmp0G), _mm_mul_ps(ifYmfX, TexSmp1G)), 
                       _mm_add_ps(_mm_mul_ps(ifXmfY,  TexSmp2G), _mm_mul_ps(fXmfY,  TexSmp3G)));
          TexSmp.B = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(ifYmifX, TexSmp0B), _mm_mul_ps(ifYmfX, TexSmp1B)), 
                       _mm_add_ps(_mm_mul_ps(ifXmfY,  TexSmp2B), _mm_mul_ps(fXmfY,  TexSmp3B)));
          TexSmp.A = 
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(ifYmifX, TexSmp0A), _mm_mul_ps(ifYmfX, TexSmp1A)), 
                       _mm_add_ps(_mm_mul_ps(ifXmfY,  TexSmp2A), _mm_mul_ps(fXmfY,  TexSmp3A)));

          Shader(Dest, TexSmp, ColorModifier, Texel, U, V);
        }
        else
        {
          Shader(Dest, TexSmp, ColorModifier, Texel, U, V);
        }

        //TODO(bjorn): Should this be promoted to the shader?
        __m128 InvTexelA = _mm_sub_ps(Const1, _mm_mul_ps(Texel.A, Inv255));
        __m128 BlendedR = _mm_add_ps(_mm_mul_ps(Dest.R, InvTexelA), Texel.R);
        __m128 BlendedG = _mm_add_ps(_mm_mul_ps(Dest.G, InvTexelA), Texel.G);
        __m128 BlendedB = _mm_add_ps(_mm_mul_ps(Dest.B, InvTexelA), Texel.B);
        __m128 BlendedA = _mm_add_ps(_mm_mul_ps(Dest.A, InvTexelA), Texel.A);

#if 0
        //TODO(bjorn): Why is this producing artifacts when A == 0.
        BlendedR = _mm_mul_ps(_mm_rsqrt_ps(BlendedR), BlendedR);
        BlendedG = _mm_mul_ps(_mm_rsqrt_ps(BlendedG), BlendedG);
        BlendedB = _mm_mul_ps(_mm_rsqrt_ps(BlendedB), BlendedB);
#else
        BlendedR = _mm_sqrt_ps(BlendedR);
        BlendedG = _mm_sqrt_ps(BlendedG);
        BlendedB = _mm_sqrt_ps(BlendedB);
#endif

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
                                         _mm_andnot_si128(DrawMask, DestRaw));
        _mm_storeu_si128((__m128i*)Pixel, MaskedOut);
      }

      Pixel += 4;
      PixelPointX = _mm_add_ps(PixelPointX, Const4444);

      if(IX + 8 < MaxX)
      {
        ClipMask = _mm_set1_epi32(0xFFFFFFFF);
      }
      else
      {
        ClipMask = EndClipMask;
      }
    }

    UpperLeftPixel += RowAdvance;
  }

  u64 PixelCount = ((MaxX-MinX) > 0 && (MaxY-MinY) > 0) ? ((MaxX-MinX) * (MaxY-MinY)) : 0;
  END_TIMED_BLOCK_COUNTED(ProcessPixel, PixelCount);

  _MM_SET_ROUNDING_MODE(DefaultSSERoundingMode);

  END_TIMED_BLOCK(DrawTriangle);
}
#if COMPILER_MSVC
#pragma warning(default:4701)
#endif

  internal_function void
DrawCircle(game_bitmap *Buffer, 
           f32 RealX, f32 RealY, f32 RealRadius,
           f32 R, f32 G, f32 B, f32 A,
           rectangle2s ClipRect)
{
  rectangle2s FillRect;
  FillRect.Min.X = RoundF32ToS32(RealX - RealRadius);
  FillRect.Max.X = RoundF32ToS32(RealX + RealRadius);
  FillRect.Min.Y = RoundF32ToS32(RealY - RealRadius);
  FillRect.Max.Y = RoundF32ToS32(RealY + RealRadius);

  FillRect = Intersect(FillRect, ClipRect);
  if(HasNoArea(FillRect)) { return; }

  s32 Left   = FillRect.Min.X;
  s32 Bottom = FillRect.Min.Y;
  s32 Right  = FillRect.Max.X;
  s32 Top    = FillRect.Max.Y;

  s32 RadiusSquared = RoundF32ToS32(Square(RealRadius));

  s32 CenterX = RoundF32ToS32(RealX);
  s32 CenterY = RoundF32ToS32(RealY);

  u32 *UpperLeftPixel = (u32 *)Buffer->Memory + Left + Bottom * Buffer->Pitch;

  for(s32 Y = Bottom;
      Y < Top;
      Y++)
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
DrawCircle(game_bitmap *Buffer, v2 P, f32 R, v4 C, rectangle2s ClipRect)
{
	DrawCircle(Buffer, P.X, P.Y, R, C.R, C.G, C.B, C.A, ClipRect);
}

enum render_group_entry_type
{
	RenderGroupEntryType_render_entry_vector,
	RenderGroupEntryType_render_entry_coordinate_system,
	RenderGroupEntryType_render_entry_wire_cube,
	RenderGroupEntryType_render_entry_blank_quad,
	RenderGroupEntryType_render_entry_quad,
	RenderGroupEntryType_render_entry_triangle_fill_flip,
	RenderGroupEntryType_render_entry_quad_bezier_fill_flip,
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
struct render_entry_triangle_fill_flip
{
	v3 Sta;
	v3 Mid;
	v3 End;
};
struct render_entry_quad_bezier_fill_flip
{
	v3 Sta;
	v3 Mid;
	v3 End;
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
  *Result = {};

	Result->PushBufferBase = PushArray(Arena, MaxPushBufferSize, u8);
  Result->PushBufferSize = 0;
  Result->MaxPushBufferSize = MaxPushBufferSize;

	return Result;
}

internal_function void
ClearRenderGroup(render_group* RenderGroup)
{
  RenderGroup->ClearScreen = false;
  RenderGroup->PushBufferSize = 0;
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

    Result = (RenderGroup->PushBufferBase + 
              RenderGroup->PushBufferSize + 
              sizeof(render_entry_header));

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
PushTriangleFillFlip(render_group* RenderGroup, v2 Sta, v2 Mid, v2 End)
{
	render_entry_triangle_fill_flip* Entry = 
    PushRenderElement(RenderGroup, render_entry_triangle_fill_flip);
	Entry->Sta = ToV3(Sta, 0.0f);
	Entry->Mid = ToV3(Mid, 0.0f);
	Entry->End = ToV3(End, 0.0f);
}

internal_function void
PushQuadBezierFlip(render_group* RenderGroup, v2 Sta, v2 Mid, v2 End)
{
	render_entry_quad_bezier_fill_flip* Entry = 
    PushRenderElement(RenderGroup, render_entry_quad_bezier_fill_flip);
	Entry->Sta = ToV3(Sta, 0.0f);
	Entry->Mid = ToV3(Mid, 0.0f);
	Entry->End = ToV3(End, 0.0f);
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
SetCamera(render_group* RenderGroup, m44 WorldToCamera, f32 LensChamberSize, f32 NearClipPoint = 0)
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
           game_bitmap* OutputTarget, v3 V0, v3 V1, v3 Color,
           rectangle2s ClipRect)
{
  pixel_line_segment_result LineSegment = 
    ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                           ScreenVars, V0, V1);
  if(LineSegment.PartOfSegmentInView)
  {
    DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Color, ClipRect);
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
      DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Color, ClipRect);
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
      DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Color, ClipRect);
    }
  }
}

struct uv_triangle_clip_result
{
  u32 TriCount = 0;
  v3 Verts[4];
  v2 UV[4];
};
internal_function uv_triangle_clip_result
ClipUVTriangleByZPlane(f32 ClipZ, v3 Vert0, v3 Vert1, v3 Vert2, 
                       v2 UV0, v2 UV1, v2 UV2)
{
  uv_triangle_clip_result Result = {};

  v3 ClipPlaneNormal = {0,0,-1};
  v3 ClipPlanePoint = {0,0,ClipZ};
  v3 Verts[3] = {Vert0, Vert1, Vert2};
  v2 UV[3] = {UV0, UV1, UV2};

  b32 Beyond[3] = { (Verts[0].Z >= ClipZ)?(b32)1:(b32)0,
                    (Verts[1].Z >= ClipZ)?(b32)1:(b32)0,
                    (Verts[2].Z >= ClipZ)?(b32)1:(b32)0};

  s32 BeyondSum = Beyond[0] + Beyond[1] + Beyond[2];
  if(BeyondSum != 0 && BeyondSum != 3)
  {
    u32 StartVertexIndex;
    if(BeyondSum == 1)
    {
      Result.TriCount = 2;
      if     (Beyond[0] == 1) { StartVertexIndex = 2; }
      else if(Beyond[1] == 1) { StartVertexIndex = 0; }
      else                    { StartVertexIndex = 1; }
    }
    else
    {
      Result.TriCount = 1;
      if     (Beyond[0] == 0) { StartVertexIndex = 2; }
      else if(Beyond[1] == 0) { StartVertexIndex = 0; }
      else                    { StartVertexIndex = 1; }
    }

    v3 A = Verts[(StartVertexIndex+0)%3];
    v3 B = Verts[(StartVertexIndex+1)%3];
    v3 C = Verts[(StartVertexIndex+2)%3];
    v2 A_UV = UV[(StartVertexIndex+0)%3];
    v2 B_UV = UV[(StartVertexIndex+1)%3];
    v2 C_UV = UV[(StartVertexIndex+2)%3];

    line_plane_intersect IntersectAB = 
      LinePlaneIntersection(B, A, ClipPlaneNormal, ClipPlanePoint);
    line_plane_intersect IntersectCB = 
      LinePlaneIntersection(B, C, ClipPlaneNormal, ClipPlanePoint);

    Assert(IntersectAB.Hit && IntersectCB.Hit);

    if(BeyondSum == 1)
    {
      Result.Verts[0] = A;
      Result.Verts[1] = B + (A-B)*IntersectAB.t;
      Result.Verts[2] = B + (C-B)*IntersectCB.t;
      Result.Verts[3] = C;

      Result.UV[0] = A_UV;
      Result.UV[1] = B_UV + (A_UV-B_UV)*IntersectAB.t;
      Result.UV[2] = B_UV + (C_UV-B_UV)*IntersectCB.t;
      Result.UV[3] = C_UV;
    }
    else
    {
      Result.Verts[0] = B + (A-B)*IntersectAB.t;
      Result.Verts[1] = B;
      Result.Verts[2] = B + (C-B)*IntersectCB.t;

      Result.UV[0] = B_UV + (A_UV-B_UV)*IntersectAB.t;
      Result.UV[1] = B_UV;
      Result.UV[2] = B_UV + (C_UV-B_UV)*IntersectCB.t;
    }
  }
  else if(BeyondSum == 0)
  {
    Result.TriCount = 1;
    Result.Verts[0] = Verts[0];
    Result.Verts[1] = Verts[1];
    Result.Verts[2] = Verts[2];
    Result.UV[0] = UV[0];
    Result.UV[1] = UV[1];
    Result.UV[2] = UV[2];
  }

  return Result;
}

#define RenderGroup_DefineEntryAndAdvanceByteOffset(type) \
  type * Entry = (type *)UnidentifiedEntry; \
  PushBufferByteOffset += sizeof(render_entry_header) + sizeof(type)

  internal_function void
RenderGroupToOutput(render_group* RenderGroup, game_bitmap* OutputTarget, f32 ScreenHeightInMeters,
                    rectangle2s ClipRect)
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
    DrawRectangle(OutputTarget, RectMinMax(v2s{0, 0}, OutputTarget->Dim), 
                  RenderGroup->ClearScreenColor.RGB, ClipRect);
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
                     Entry->A, Entry->B, Entry->Color.RGB, ClipRect);
        } break;
      case RenderGroupEntryType_render_entry_coordinate_system
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_coordinate_system);

          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{1,0,0}, v3{1,0,0}, ClipRect);
          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{0,1,0}, v3{0,1,0}, ClipRect);
          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{0,0,1}, v3{0,0,1}, ClipRect);
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
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB, ClipRect);
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
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB, ClipRect);
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
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB, ClipRect);
            }
          }
        } break;
      //TODO(bjorn): Quad seam problems are still there.
      case RenderGroupEntryType_render_entry_blank_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_blank_quad);

          //TODO(bjorn): Think about how and when in the pipeline to render the hit-points.
          quad_verts_result Quad = GetQuadVertices(RenderGroup->WorldToCamera * Entry->Tran);

          {
            uv_triangle_clip_result ClipResult = 
              ClipUVTriangleByZPlane(RenderGroup->CamParam.NearClipPoint, 
                                     Quad.Verts[0], Quad.Verts[1], Quad.Verts[2], 
                                     {0,0}, {0,1}, {1,1});
            if(ClipResult.TriCount > 0)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[1], 
                           ClipResult.Verts[2], 
                           ClipResult.UV[0],
                           ClipResult.UV[1],
                           ClipResult.UV[2],
                           0,
                           Entry->Color,
                           ClipRect,
                           PlainFillShader);
            }
            if(ClipResult.TriCount > 1)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[2], 
                           ClipResult.Verts[3], 
                           ClipResult.UV[0],
                           ClipResult.UV[2],
                           ClipResult.UV[3],
                           0,
                           Entry->Color,
                           ClipRect,
                           PlainFillShader);
            }
          }

          {
            uv_triangle_clip_result ClipResult = 
              ClipUVTriangleByZPlane(RenderGroup->CamParam.NearClipPoint, 
                                     Quad.Verts[0], Quad.Verts[2], Quad.Verts[3], 
                                     {0,0}, {1,1}, {1,0});
            if(ClipResult.TriCount > 0)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[1], 
                           ClipResult.Verts[2], 
                           ClipResult.UV[0],
                           ClipResult.UV[1],
                           ClipResult.UV[2],
                           0,
                           Entry->Color,
                           ClipRect,
                           PlainFillShader);
            }
            if(ClipResult.TriCount > 1)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[2], 
                           ClipResult.Verts[3], 
                           ClipResult.UV[0],
                           ClipResult.UV[2],
                           ClipResult.UV[3],
                           0,
                           Entry->Color,
                           ClipRect,
                           PlainFillShader);
            }
          }

#if 0
          {
            u32 TriCount = 1;
            v3 Verts[4] = {Quad.Verts[0], Quad.Verts[2], Quad.Verts[3], {}};
            v3 UV[4] = {{0,0}, {1,1}, {1,0}, {}};

            DrawTriangle(OutputTarget, 
                         &RenderGroup->CamParam, &ScreenVars, 
                         Verts[0], 
                         Verts[1], 
                         Verts[2], 
                         UV[0],
                         UV[1],
                         UV[2],
                         0,
                         Entry->Color,
                         ClipRect);
            if(TriCount == 2)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           Verts[0], 
                           Verts[2], 
                           Verts[3], 
                           UV[0],
                           UV[2],
                           UV[3],
                           0,
                           Entry->Color,
                           ClipRect);
            }
          }
#endif
        } break;
      case RenderGroupEntryType_render_entry_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad);

          quad_verts_result Quad = GetQuadVertices(RenderGroup->WorldToCamera * Entry->Tran);

          //TODO(bjorn): Why cant this be the same as the exact lens chamber size?

          {
            uv_triangle_clip_result ClipResult = 
              ClipUVTriangleByZPlane(RenderGroup->CamParam.NearClipPoint, 
                                     Quad.Verts[0], Quad.Verts[1], Quad.Verts[2], 
                                     {0,0}, {0,1}, {1,1});
            if(ClipResult.TriCount > 0)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[1], 
                           ClipResult.Verts[2], 
                           ClipResult.UV[0],
                           ClipResult.UV[1],
                           ClipResult.UV[2],
                           Entry->Bitmap,
                           Entry->Color,
                           ClipRect,
                           BitmapShader);
            }
            if(ClipResult.TriCount > 1)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[2], 
                           ClipResult.Verts[3], 
                           ClipResult.UV[0],
                           ClipResult.UV[2],
                           ClipResult.UV[3],
                           Entry->Bitmap,
                           Entry->Color,
                           ClipRect,
                           BitmapShader);
            }
          }

          {
            uv_triangle_clip_result ClipResult = 
              ClipUVTriangleByZPlane(RenderGroup->CamParam.NearClipPoint, 
                                     Quad.Verts[0], Quad.Verts[2], Quad.Verts[3], 
                                     {0,0}, {1,1}, {1,0});
            if(ClipResult.TriCount > 0)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[1], 
                           ClipResult.Verts[2], 
                           ClipResult.UV[0],
                           ClipResult.UV[1],
                           ClipResult.UV[2],
                           Entry->Bitmap,
                           Entry->Color,
                           ClipRect,
                           BitmapShader);
            }
            if(ClipResult.TriCount > 1)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[2], 
                           ClipResult.Verts[3], 
                           ClipResult.UV[0],
                           ClipResult.UV[2],
                           ClipResult.UV[3],
                           Entry->Bitmap,
                           Entry->Color,
                           ClipRect,
                           BitmapShader);
            }
          }

#if 0
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
      case RenderGroupEntryType_render_entry_triangle_fill_flip
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_triangle_fill_flip);

          DrawTriangle(OutputTarget, 
                       &RenderGroup->CamParam, &ScreenVars, 
                       Entry->Sta, 
                       Entry->Mid, 
                       Entry->End, 
                       {1,0},
                       {1,1},
                       {0,1},
                       0,
                       {},
                       ClipRect,
                       PlainFillFlipShader);
        } break;
      case RenderGroupEntryType_render_entry_quad_bezier_fill_flip
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad_bezier_fill_flip);

          DrawTriangle(OutputTarget, 
                       &RenderGroup->CamParam, &ScreenVars, 
                       Entry->Sta, 
                       Entry->Mid, 
                       Entry->End, 
                       {1,0},
                       {1,1},
                       {0,1},
                       0,
                       {},
                       ClipRect,
                       QuadBezierFlipShader);
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

            DrawCircle(OutputTarget, PixPos.P, PixR, Entry->Color, ClipRect);
          }
        } break;
      InvalidDefaultCase;
    }
  }

  END_TIMED_BLOCK(RenderGroupToOutput);
}

struct render_work
{
  render_group* RenderGroup; 
  game_bitmap* OutputTarget; 
  f32 ScreenHeightInMeters;
  rectangle2s ClipRect; 
};
WORK_QUEUE_CALLBACK(DoRenderWork)
{
  render_work* RenderWork = (render_work*)Data;

  RenderGroupToOutput(RenderWork->RenderGroup, RenderWork->OutputTarget, 
                      RenderWork->ScreenHeightInMeters, RenderWork->ClipRect);
}

  internal_function void
TiledRenderGroupToOutput(work_queue* RenderQueue, render_group* RenderGroup,
                         game_bitmap* OutputTarget, f32 ScreenHeightInMeters)
{
#if 0
  s32 const TileCountY = 2;
  s32 const TileCountX = 2;

  Assert(((memi)OutputTarget->Memory & 15) == 0);

  s32 TileWidth = OutputTarget->Width / TileCountX;
  s32 TileHeight = OutputTarget->Height / TileCountY;

  TileWidth  = Align4(TileWidth);

  render_work RenderWork[TileCountX*TileCountY];
  s32 Index = 0;
  for(s32 TileY = 0;
      TileY < TileCountY;
      TileY++)
  {
    for(s32 TileX = 0;
        TileX < TileCountX;
        TileX++)
    {
      rectangle2s ClipRect;
      ClipRect.Min.X = TileX*TileWidth;
      ClipRect.Min.Y = TileY*TileHeight;
      ClipRect.Max.X = ClipRect.Min.X + TileWidth;
      ClipRect.Max.Y = ClipRect.Min.Y + TileHeight;

      ClipRect.Max.X = Min(ClipRect.Max.X, (s32)OutputTarget->Width);
      ClipRect.Max.Y = Min(ClipRect.Max.Y, (s32)OutputTarget->Height);

      Assert(ClipRect.Min.X >= 0);
      Assert(ClipRect.Min.Y >= 0);
      Assert(ClipRect.Max.X <= (s32)OutputTarget->Dim.X);
      Assert(ClipRect.Max.Y <= (s32)OutputTarget->Dim.Y);

      RenderWork[Index].RenderGroup = RenderGroup;
      RenderWork[Index].OutputTarget = OutputTarget;
      RenderWork[Index].ScreenHeightInMeters = ScreenHeightInMeters;
      RenderWork[Index].ClipRect = ClipRect;

#if 1
      PushWork(RenderQueue, DoRenderWork, &RenderWork[Index]);
#else
      DoRenderWork(&RenderWork[Index]);
#endif

      Index++;
    }
  }
  CompleteWork(RenderQueue);
#else
  // rectangle2s ClipRect = RectMinMax(v2s{200,200}, v2s{400,400});
  rectangle2s ClipRect = RectMinMax(v2s{0,0}, (v2s)OutputTarget->Dim);

  RenderGroupToOutput(RenderGroup, OutputTarget, ScreenHeightInMeters, ClipRect);
#endif
}


#define RENDER_GROUP_H
#endif
