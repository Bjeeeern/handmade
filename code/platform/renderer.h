#if !defined(RENDERER_H)

#include "math.h"
#include "font.h"

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
			u32 *Pixel = (u32 *)Buffer->Memory + Buffer->Width * Y + X;
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
				 f32 GlyphOriginLeft, f32 GlyphOriginTop,
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
	v2 GlyphDim = {GlyphPixelWidth, -GlyphPixelHeight};
	v2 GlyphOrigin = {GlyphOriginLeft, GlyphOriginTop};

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

	internal_function void
DrawTriangleSlowly(game_bitmap *Buffer, 
                   f32 LensChamberSize, v2 ScreenCenter, m22 MeterToPixel, f32 NearClipPoint,
                   v3 CameraSpacePoint0, v3 CameraSpacePoint1, v3 CameraSpacePoint2, 
                   v2 UV0, v2 UV1, v2 UV2, 
                   game_bitmap* Bitmap, v4 RGBA)
{
  Assert(NearClipPoint <= LensChamberSize);

  b32 AllPointsBehindClipPoint = false;
  {
    if(CameraSpacePoint0.Z > NearClipPoint &&
       CameraSpacePoint1.Z > NearClipPoint &&
       CameraSpacePoint2.Z > NearClipPoint)
    {
      AllPointsBehindClipPoint = true;
    }
    else
    {
      //TODO(bjorn): Come up with a good clipping scheme.
      if(CameraSpacePoint0.Z > NearClipPoint ||
         CameraSpacePoint1.Z > NearClipPoint ||
         CameraSpacePoint2.Z > NearClipPoint)
      {
        AllPointsBehindClipPoint = true;
      }
    }
  }
  if(AllPointsBehindClipPoint) { return; }

  v2 PixelSpacePoint0, PixelSpacePoint1, PixelSpacePoint2;
  {
    f32 Divisor = (LensChamberSize - CameraSpacePoint0.Z);
    f32 PerspectiveCorrection = SafeRatio0(LensChamberSize, Divisor);

    PixelSpacePoint0 = 
      (ScreenCenter + (MeterToPixel * CameraSpacePoint0.XY) * PerspectiveCorrection);
  }
  {
    f32 Divisor = (LensChamberSize - CameraSpacePoint1.Z);
    f32 PerspectiveCorrection = SafeRatio0(LensChamberSize, Divisor);

    PixelSpacePoint1 = 
      (ScreenCenter + (MeterToPixel * CameraSpacePoint1.XY) * PerspectiveCorrection);
  }
  {
    f32 Divisor = (LensChamberSize - CameraSpacePoint2.Z);
    f32 PerspectiveCorrection = SafeRatio0(LensChamberSize, Divisor);

    PixelSpacePoint2 = 
      (ScreenCenter + (MeterToPixel * CameraSpacePoint2.XY) * PerspectiveCorrection);
  }

  v3 FocalPoint = {0,0,LensChamberSize};
  v3 TriangleNormal = 
    Cross(CameraSpacePoint1-CameraSpacePoint0, CameraSpacePoint2-CameraSpacePoint0);
  v3 TriangleCenter = (CameraSpacePoint0 + CameraSpacePoint1 + CameraSpacePoint2) * (1.0f/3.0f);

  b32 CameraAndTriangleNormalsAreOrthogonal = Dot(TriangleNormal, TriangleCenter - FocalPoint) == 0;
  if(CameraAndTriangleNormalsAreOrthogonal) { return; }

  s32 Left   = RoundF32ToS32(Min(Min(PixelSpacePoint0.X, PixelSpacePoint1.X),PixelSpacePoint2.X));
  s32 Right  = RoundF32ToS32(Max(Max(PixelSpacePoint0.X, PixelSpacePoint1.X),PixelSpacePoint2.X));
  s32 Top    = RoundF32ToS32(Min(Min(PixelSpacePoint0.Y, PixelSpacePoint1.Y),PixelSpacePoint2.Y));
  s32 Bottom = RoundF32ToS32(Max(Max(PixelSpacePoint0.Y, PixelSpacePoint1.Y),PixelSpacePoint2.Y));

  Left = Left < 0 ? 0 : Left;
  Top = Top < 0 ? 0 : Top;

  Right = Right > Buffer->Width ? Buffer->Width : Right;
  Bottom = Bottom > Buffer->Height ? Buffer->Height : Bottom;

  RGBA.RGB *= RGBA.A;
  m22 PixelToMeter = {1.0f/MeterToPixel.A, 0.0f,
                      0.0f,                1.0f/MeterToPixel.D};

  u32 *UpperLeftPixel = Buffer->Memory + Left + Top * Buffer->Pitch;
  for(s32 Y = Top;
      Y < Bottom;
			++Y)
	{
		u32 *Pixel = UpperLeftPixel;

		for(s32 X = Left;
				X < Right;
				++X)
    {
      v2 PixelPoint = Vec2(X, Y);

      v3 Point;
      {
        v3 PointInCameraSpace = ToV3(PixelToMeter * (PixelPoint - ScreenCenter), 0);
        v3 LineDirection = PointInCameraSpace - FocalPoint;

        //NOTE(bjorn): Source: https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
        //NOTE(bjorn): TriangleNormal does not actually need to be a normal
        //since it cancles out when calculating d.
        //TODO(bjorn): SafeRatio here?
        f32 d = (Dot((TriangleCenter - FocalPoint), TriangleNormal) / 
                 Dot(LineDirection, TriangleNormal));

        //TODO(bjorn): Does this assertion make sense? Do I have a problem when 
        //             NearClipPoint == LensChamberSize ?
        //Assert(d >= 0);

        Point = FocalPoint + d * LineDirection;
      }

      v3 TriangleWeights = PointToBarycentricCoordinates(Point, 
                                                         CameraSpacePoint0, 
                                                         CameraSpacePoint1, 
                                                         CameraSpacePoint2);

      b32 InsideTriangle = (TriangleWeights.X >= 0 &&
                            TriangleWeights.Y >= 0 &&
                            TriangleWeights.Z >= 0);
      if(InsideTriangle)
      {

        v2 UV = (TriangleWeights.X * UV0 +
                 TriangleWeights.Y * UV1 +
                 TriangleWeights.Z * UV2);

        Assert(0.0f <= UV.U && UV.U <= 1.0f);
        Assert(0.0f <= UV.V && UV.V <= 1.0f);

        f32 tX = (1.0f + (UV.U * (f32)(Bitmap->Width -3)) + 0.5f);
        f32 tY = (1.0f + (UV.V * (f32)(Bitmap->Height-3)) + 0.5f);

        s32 wX = (s32)tX;
        s32 wY = (s32)tY;

        f32 fX = tX - wX;
        f32 fY = tY - wY;

				u32* TexelPtr = Bitmap->Memory + Bitmap->Pitch * wY + wX;
				u32* TexelPtrA = TexelPtr;
				u32* TexelPtrB = TexelPtr + 1;
				u32* TexelPtrC = TexelPtr + Bitmap->Pitch;
				u32* TexelPtrD = TexelPtr + Bitmap->Pitch + 1;

        v4 TexelA = { (f32)((*TexelPtrA >> 16)&0xFF),
                      (f32)((*TexelPtrA >>  8)&0xFF),
                      (f32)((*TexelPtrA >>  0)&0xFF),
                      (f32)((*TexelPtrA >> 24)&0xFF)};
        v4 TexelB = { (f32)((*TexelPtrB >> 16)&0xFF),
                      (f32)((*TexelPtrB >>  8)&0xFF),
                      (f32)((*TexelPtrB >>  0)&0xFF),
                      (f32)((*TexelPtrB >> 24)&0xFF)};
        v4 TexelC = { (f32)((*TexelPtrC >> 16)&0xFF),
                      (f32)((*TexelPtrC >>  8)&0xFF),
                      (f32)((*TexelPtrC >>  0)&0xFF),
                      (f32)((*TexelPtrC >> 24)&0xFF)};
        v4 TexelD = { (f32)((*TexelPtrD >> 16)&0xFF),
                      (f32)((*TexelPtrD >>  8)&0xFF),
                      (f32)((*TexelPtrD >>  0)&0xFF),
                      (f32)((*TexelPtrD >> 24)&0xFF)};

        TexelA = sRGB255ToLinear1(TexelA);
        TexelB = sRGB255ToLinear1(TexelB);
        TexelC = sRGB255ToLinear1(TexelC);
        TexelD = sRGB255ToLinear1(TexelD);

        v4 Texel = Lerp(fY, Lerp(fX, TexelA, TexelB), Lerp(fX, TexelC, TexelD));

        v4 DestColor = { (f32)((*Pixel >> 16)&0xFF),
                         (f32)((*Pixel >>  8)&0xFF),
                         (f32)((*Pixel >>  0)&0xFF),
                         (f32)((*Pixel >> 24)&0xFF)};
        DestColor = sRGB255ToLinear1(DestColor);

        Texel = Hadamard(Texel, RGBA);

        v4 Blended = DestColor*(1.0f - Texel.A) + Texel;

        Blended = Linear1TosRGB255(Blended);

        u32 Color = (((u32)(Blended.A+0.5f) << 24) | 
                     ((u32)(Blended.R+0.5f) << 16) | 
                     ((u32)(Blended.G+0.5f) <<  8) | 
                     ((u32)(Blended.B+0.5f) <<  0));

        *Pixel = Color;
      }

      Pixel++;
    }

    UpperLeftPixel += Buffer->Pitch;
  }
}

  internal_function void
DrawRectangle(game_bitmap *Buffer, rectangle2 Rect, v3 RGB)
{
  s32 Left = RoundF32ToS32(Rect.Min.X);
  s32 Right = RoundF32ToS32(Rect.Max.X);
  s32 Top = RoundF32ToS32(Rect.Min.Y);
  s32 Bottom = RoundF32ToS32(Rect.Max.Y);

  Left = Left < 0 ? 0 : Left;
  Top = Top < 0 ? 0 : Top;

  Right = Right > Buffer->Width ? Buffer->Width : Right;
  Bottom = Bottom > Buffer->Height ? Buffer->Height : Bottom;

  u32 Color = ((RoundF32ToS32(RGB.R * 255.0f) << 16) |
               (RoundF32ToS32(RGB.G * 255.0f) << 8) |
               (RoundF32ToS32(RGB.B * 255.0f) << 0));

  s32 PixelPitch = Buffer->Width;

	u32 *UpperLeftPixel = (u32 *)Buffer->Memory + Left + Top * PixelPitch;

	for(s32 Y = Top;
			Y < Bottom;
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
	v2 ScreenSpaceDir = v2{WorldDir.X, -WorldDir.Y};
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

	internal_function void
DrawCircle(game_bitmap *Buffer, 
					 f32 RealX, f32 RealY, f32 RealRadius,
					 f32 R, f32 G, f32 B, f32 A)
{
	s32 CenterX = RoundF32ToS32(RealX);
	s32 CenterY = RoundF32ToS32(RealY);
	s32 Left = RoundF32ToS32(RealX - RealRadius);
	s32 Right = RoundF32ToS32(RealX + RealRadius);
	s32 Top = RoundF32ToS32(RealY - RealRadius);
	s32 Bottom = RoundF32ToS32(RealY + RealRadius);

	s32 RadiusSquared = RoundF32ToS32(Square(RealRadius));

	Left = Left < 0 ? 0 : Left;
	Top = Top < 0 ? 0 : Top;
	Right = Right > Buffer->Width ? Buffer->Width : Right;
	Bottom = Bottom > Buffer->Height ? Buffer->Height : Bottom;

	s32 PixelPitch = Buffer->Width;

	u32 *UpperLeftPixel = (u32 *)Buffer->Memory + Left + Top * PixelPitch;

	for(s32 Y = Top;
			Y < Bottom;
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

		UpperLeftPixel += PixelPitch;
	}
}

	internal_function void
DrawCircle(game_bitmap *Buffer, v2 P, f32 R, v4 C)
{
	DrawCircle(Buffer, P.X, P.Y, R, C.R, C.G, C.B, C.A);
}

#define RENDERER_H
#endif
