#if !defined(RENDERER_H)

#include "math.h"
#include "font.h"
#include "resource.h"

struct camera
{
	v3 Pos;
	f32 FocalPointDistanceFromFurstum;
	f32 FurstumWidth;
};

struct depth_buffer
{
	void *Memory;
	s32 Width;
	s32 Height;
	s32 BytesPerPixel;
	s32 Pitch;
};

internal_function void
ClearDepthBuffer(depth_buffer* DepthBuffer)
{
	s32 PixelPitch = DepthBuffer->Width;
	f32 *UpperLeftPixel = (f32 *)DepthBuffer->Memory;

	for(s32 Y = 0;
			Y < DepthBuffer->Height;
			++Y)
	{
		f32 *Z = UpperLeftPixel;

		for(s32 X = 0;
				X < DepthBuffer->Width;
				++X)
		{
			*Z++ = -inf32;
		}
		UpperLeftPixel += PixelPitch;
	}
}

	internal_function void
DrawLine(game_offscreen_buffer *Buffer, depth_buffer* DepthBuffer,
				 f32 PixelStartX, f32 PixelStartY, f32 StartZ, 
				 f32 PixelEndX, f32 PixelEndY, f32 EndZ,
				 f32 RealR, f32 RealG, f32 RealB)
{
	f32 R = RealR;
	f32 G = RealG;
	f32 B = RealB;
	u32 Color = ((RoundF32ToS32(R * 255.0f) << 16) |
							 (RoundF32ToS32(G * 255.0f) << 8) |
							 (RoundF32ToS32(B * 255.0f) << 0));

	s32 PixelPitch = Buffer->Width;

	v2 Start = {PixelStartX, PixelStartY};
	v2 End = {PixelEndX, PixelEndY};

	f32 K = (End.Y - Start.Y) / (End.X - Start.X);
	f32 M = Start.Y - K * Start.X;

	b32 StartIsInside = (((0 <= Start.X) && (Start.X < Buffer->Width)) &&
											 ((0 <= Start.Y) && (Start.Y < Buffer->Height)));
	b32 EndIsInside = (((0 <= End.X) && (End.X < Buffer->Width)) &&
											 ((0 <= End.Y) && (End.Y < Buffer->Height)));

	if(!StartIsInside && !EndIsInside) return;

	v2 OldStart = Start;
	v2 OldEnd = End;

	if(Start.X < 0)
	{
		Start.X = 0;
		Start.Y = K * Start.X + M;
	}
	if(Start.X > (f32)Buffer->Width)
	{
		Start.X = (f32)Buffer->Width;
		Start.Y = K * Start.X + M;
	}
	if(End.X < 0)
	{
		End.X = 0;
		End.Y = K * End.X + M;
	}
	if(End.X > (f32)Buffer->Width)
	{
		End.X = (f32)Buffer->Width;
		End.Y = K * End.X + M;
	}

	if(Start.Y < 0)
	{
		Start.Y = 0;
		Start.X = (Start.Y - M) / K;
	}
	if(Start.Y > (f32)Buffer->Height)
	{
		Start.Y = (f32)Buffer->Height;
		Start.X = (Start.Y - M) / K;
	}
	if(End.Y < 0)
	{
		End.Y = 0;
		End.X = (End.Y - M) / K;
	}
	if(End.Y > (f32)Buffer->Height)
	{
		End.Y = (f32)Buffer->Height;
		End.X = (End.Y - M) / K;
	}

	f32 NewStartZ;
	f32 NewEndZ;
	{
		v2 OldStartToEnd = OldEnd - OldStart;
		f32 RefLength = SquareRoot(OldStartToEnd.X * OldStartToEnd.X + 
															 OldStartToEnd.Y * OldStartToEnd.Y);

		v2 OStoE = End - OldStart;
		v2 OStoS = Start - OldStart;

		f32 LengthToStart = InverseSquareRoot(OStoS.X * OStoS.X + OStoS.Y * OStoS.Y);
		f32 LengthToEnd = InverseSquareRoot(OStoE.X * OStoE.X + OStoE.Y * OStoE.Y);

		NewStartZ = Lerp(LengthToStart / RefLength, StartZ, EndZ);
		NewEndZ = Lerp(LengthToEnd / RefLength, StartZ, EndZ);
	}

	StartZ = NewStartZ;
	EndZ = NewEndZ;

	v2 Vector = End - Start;

	f32 InverseLength = InverseSquareRoot(Vector.X * Vector.X + Vector.Y * Vector.Y);
	f32 Length = 1.0f / InverseLength;
	v2 Normal = Vector * InverseLength;

	f32 StepLength = 0.0f;
	while(StepLength < Length)
	{
		v2 Point = Start + (Normal * StepLength);
		s32 X = RoundF32ToS32(Point.X);
		s32 Y = RoundF32ToS32(Point.Y);
		f32 InterpZ = Lerp(StepLength / Length, StartZ, EndZ);

		if(((0 <= X) && (X < Buffer->Width)) &&
			 ((0 <= Y) && (Y < Buffer->Height)))
		{
			u32 *Pixel = (u32 *)Buffer->Memory + X + PixelPitch * Y;
			f32 *Z = (f32 *)DepthBuffer->Memory + X + PixelPitch * Y;

			b32 DepthTest = InterpZ >= *Z;

			*Pixel = DepthTest ? Color : *Pixel;
			*Z = DepthTest ? InterpZ : *Z;
		}

		StepLength += 1.0f;
	}
}

	internal_function void
DrawLine(game_offscreen_buffer* Buffer, 
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
DrawLine(game_offscreen_buffer* Buffer, v2 A, v2 B, v3 C)
{
	DrawLine(Buffer, A.X, A.Y, B.X, B.Y, C.R, C.G, C.B);
}

	internal_function void
DrawChar(game_offscreen_buffer *Buffer, font *Font, u32 UnicodeCodePoint, 
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
DrawString(game_offscreen_buffer *Buffer, depth_buffer* DepthBuffer, 
					 font *Font, char *String, 
					 f32 GlyphPixelWidth, f32 GlyphPixelHeight, 
					 f32 GlyphOriginLeft, f32 GlyphOriginTop, f32 RealZ,
					 f32 RealR, f32 RealG, f32 RealB)
{
	s32 StringLength = 0;
	u8 *StringToMeasure = (u8 *)String;
	while(*StringToMeasure++ != '\0') { StringLength++; if(StringLength > 256) break; }

#define MAX_LENGHT_FOR_DRAWABLE_STRING 256
	u16 UnicodeCodePointBuffer[MAX_LENGHT_FOR_DRAWABLE_STRING] = {};

	unicode_array_result UnicodeArray = {0};
	if(StringLength <= MAX_LENGHT_FOR_DRAWABLE_STRING)
	{
		UnicodeArray = UnicodeStringToUnicodeCodePointArray((u8 *)String, 
																												UnicodeCodePointBuffer, 
																												MAX_LENGHT_FOR_DRAWABLE_STRING);
	}
	else
	{
		char* ErrorString = "!-!-!-!-[String is too long]-!-!-!-!";
		UnicodeArray = UnicodeStringToUnicodeCodePointArray((u8*)ErrorString, 
																												UnicodeCodePointBuffer, 
																												MAX_LENGHT_FOR_DRAWABLE_STRING);
	}

	f32 BottomLeftAnchorX = GlyphOriginLeft;
	f32 BottomLeftAnchorY = GlyphOriginTop + GlyphPixelHeight;
	v2 Cursor = {BottomLeftAnchorX, BottomLeftAnchorY};

	unicode_to_glyph_data *Entries = (unicode_to_glyph_data *)(Font + 1);

	for(s32 ArrayIndex = 0;
			ArrayIndex < UnicodeArray.Count;
			ArrayIndex++)
	{
		u32 UnicodeCodePoint = UnicodeArray.UnicodeCodePoint[ArrayIndex];

		if(UnicodeCodePoint == '\n')
		{
			Cursor.Y += GlyphPixelHeight;// * Font->AdvanceWidth;
			Cursor.X = 0.0f;
		}
		else
		{
			for(s32 EntryIndex = 0;
					EntryIndex < Font->UnicodeCodePointCount;
					EntryIndex++)
			{
				if(UnicodeCodePoint == Entries[EntryIndex].UnicodeCodePoint)
				{
					if(Entries[EntryIndex].OffsetToGlyphData)
					{
						quadratic_curve *Curves = 
							(quadratic_curve *)((u8 *)Font + Entries[EntryIndex].OffsetToGlyphData);
						for(s32 CurveIndex = 0;
								CurveIndex < Entries[EntryIndex].QuadraticCurveCount;
								CurveIndex++)
						{
							quadratic_curve Curve = Curves[CurveIndex];

							v2 GlyphDim = {GlyphPixelWidth, -GlyphPixelHeight};
							v2 Start = Hadamard(Curve.Srt, GlyphDim) + Cursor;
							v2 End = Hadamard(Curve.End, GlyphDim) + Cursor;

							//Start.Y = GlyphPixelHeight - Start.Y;
							//End.Y = GlyphPixelHeight - End.Y;

							DrawLine(Buffer, DepthBuffer, 
											 Start.X, Start.Y, RealZ, 
											 End.X, End.Y, RealZ, 
											 RealR, RealG, RealB);
						}
					}

					Cursor.X += GlyphPixelWidth;// * Font->AdvanceWidth;
				}
			}
		}
	}
}

	internal_function void
DrawRectangle(game_offscreen_buffer *Buffer, rectangle2 Rect, v3 RGB)
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

	internal_function void
DrawRectangle(game_offscreen_buffer *Buffer, depth_buffer* DepthBuffer, 
							f32 RealLeft, f32 RealRight, 
							f32 RealTop, f32 RealBottom, 
							f32 RealZ,
							f32 R, f32 G, f32 B, f32 A)
{
	b32 XNotFlipped = RealLeft < RealRight;
	b32 YNotFlipped = RealTop < RealBottom;

	s32 Left = RoundF32ToS32(XNotFlipped ? RealLeft : RealRight);
	s32 Right = RoundF32ToS32(XNotFlipped ? RealRight : RealLeft);
	s32 Top = RoundF32ToS32(YNotFlipped ? RealTop : RealBottom);
	s32 Bottom = RoundF32ToS32(YNotFlipped ? RealBottom : RealTop);

	Left = Left < 0 ? 0 : Left;
	Top = Top < 0 ? 0 : Top;

	Right = Right > Buffer->Width ? Buffer->Width : Right;
	Bottom = Bottom > Buffer->Height ? Buffer->Height : Bottom;

	s32 PixelPitch = Buffer->Width;

	u32 *UpperLeftPixel = (u32 *)Buffer->Memory + Left + Top * PixelPitch;
	f32 *UpperLeftZ = (f32 *)DepthBuffer->Memory + Left + Top * PixelPitch;

	for(s32 Y = Top;
			Y < Bottom;
			++Y)
	{
		u32 *Pixel = UpperLeftPixel;
		f32 *Z = UpperLeftZ;

		for(s32 X = Left;
				X < Right;
				++X)
		{
			b32 DepthTest = RealZ >= *Z;

			f32 OldR = ((*Pixel & 0x00FF0000) >> 16) / 255.0f;
			f32 OldG = ((*Pixel & 0x0000FF00) >> 8 ) / 255.0f;
			f32 OldB = ((*Pixel & 0x000000FF) >> 0 ) / 255.0f;

			u32 Color = ((RoundF32ToS32(Lerp(A, OldR, R) * 255.0f) << 16) |
									 (RoundF32ToS32(Lerp(A, OldG, G) * 255.0f) << 8) |
									 (RoundF32ToS32(Lerp(A, OldB, B) * 255.0f) << 0));

			*Pixel = DepthTest ? Color : *Pixel;
			*Z = DepthTest ? RealZ : *Z;

			Pixel++;
			Z++;
		}

		UpperLeftPixel += PixelPitch;
		UpperLeftZ += PixelPitch;
	}
}

	internal_function v2
XYZMetersToXYPixels(game_offscreen_buffer* Buffer, camera* Camera, v3 Point)
{
	v2 Result = {};

	v3 FocalPoint = Camera->Pos;
	FocalPoint.Z -= Camera->FocalPointDistanceFromFurstum;

	f32 FurstumWidth = Camera->FurstumWidth;
	f32 ScreenRatio = (f32)Buffer->Height/(f32)Buffer->Width;
	f32 FurstumHeight = FurstumWidth * ScreenRatio;

	f32 ZRatio = Absolute(FocalPoint.Z - Camera->Pos.Z) / Absolute(FocalPoint.Z - Point.Z);

	f32 HLong = Distance(FocalPoint, Point);
	f32 HShort = HLong * ZRatio;

	v3 PointOnFurstum = Normalize(Point - FocalPoint) * HShort;

	PointOnFurstum.X -= (Camera->Pos.X - FurstumWidth*0.5f);
	PointOnFurstum.Y -= (Camera->Pos.Y + FurstumHeight*0.5f);

	Result.X = PointOnFurstum.X * ((f32)Buffer->Width / FurstumWidth);
	Result.Y = -(PointOnFurstum.Y * ((f32)Buffer->Height / FurstumHeight));

	return Result;
}

	internal_function void
DrawRectangleRelativeCamera(game_offscreen_buffer* Buffer, 
														depth_buffer* DepthBuffer, 
														camera* Camera,
														f32 RealLeft, f32 RealRight, 
														f32 RealTop, f32 RealBottom, 
														f32 RealZ,
														f32 R, f32 G, f32 B, f32 A)
{
	v3 FocalPoint = Camera->Pos;
	FocalPoint.Z -= Camera->FocalPointDistanceFromFurstum;

	if(RealZ < FocalPoint.Z) { return; }

	v3 TopLeft = {RealLeft, RealTop, RealZ};
	v3 BottomRight = {RealRight, RealBottom, RealZ};

	v2 TopLeftPixelPoint = XYZMetersToXYPixels(Buffer, Camera, TopLeft);
	v2 BottomRightPixelPoint = XYZMetersToXYPixels(Buffer, Camera, BottomRight);
	f32 DistanceFromCamera = -(RealZ - Camera->Pos.Z);

	DrawRectangle(Buffer, DepthBuffer,
								TopLeftPixelPoint.X, BottomRightPixelPoint.X, 
								TopLeftPixelPoint.Y, BottomRightPixelPoint.Y, 
								DistanceFromCamera,
								R, G, B, A);
}

// TODO(bjorn): There is a visual bug when drawn from BottomRight to TopLeft.
	internal_function void
DrawFrame(game_offscreen_buffer *Buffer, depth_buffer* DepthBuffer,
					v2 Start, v2 End, f32 RealZ, f32 Thickness, v3 Color)
{
	f32 Left = Start.X;
	f32 Top = Start.Y;
	f32 Right = End.X;
	f32 Bottom = End.Y;

	f32 Pad = Thickness*0.5f;

	DrawRectangle(Buffer, DepthBuffer, Left-Pad, Right+Pad, Top-Pad, Top+Pad, RealZ, 
								Color.R, Color.G, Color.B, 1.0f);
	DrawRectangle(Buffer, DepthBuffer, Left-Pad, Left+Pad, Top-Pad, Bottom+Pad, RealZ, 
								Color.R, Color.G, Color.B, 1.0f);
	DrawRectangle(Buffer, DepthBuffer, Right-Pad, Right+Pad, Top-Pad, Bottom+Pad, RealZ, 
								Color.R, Color.G, Color.B, 1.0f);
	DrawRectangle(Buffer, DepthBuffer, Left-Pad, Right+Pad, Bottom-Pad, Bottom+Pad, RealZ, 
								Color.R, Color.G, Color.B, 1.0f);
}

// TODO(bjorn): There is a visual bug when drawn from BottomRight to TopLeft.
	internal_function void
DrawFrame(game_offscreen_buffer *Buffer, rectangle2 R, v2 WorldDir, v3 Color)
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
DrawBitmap(game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap, 
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

	u32 *DestBufferRow = ((u32 *)Buffer->Memory) + (Buffer->Width * DestTop + DestLeft);

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
				u32 SourceColor = Bitmap->Pixels[Bitmap->Width * ((Bitmap->Height-1) - SrcY) + SrcX];
				u32 DestColor = *DestPixel;

				u8 A =  (u8)(SourceColor >> 24);
				u8 SR = (u8)(SourceColor >> 16);
				u8 SG = (u8)(SourceColor >>  8);
				u8 SB = (u8)(SourceColor >>  0);

				u8 DR = (u8)(DestColor >> 16);
				u8 DG = (u8)(DestColor >>  8);
				u8 DB = (u8)(DestColor >>  0);

				f32 t = (A / 255.0f) * Alpha;
 				u8 R = (u8)Lerp(t, DR, SR);
				u8 G = (u8)Lerp(t, DG, SG);
				u8 B = (u8)Lerp(t, DB, SB);

				*DestPixel = (R << 16) | (G << 8) | (B << 0);
			}

			DestPixel++;
		}

		DestBufferRow += Buffer->Width;
	}
}

	internal_function void
DrawBitmap(game_offscreen_buffer *Buffer, depth_buffer* DepthBuffer, loaded_bitmap *Bitmap, 
					 f32 RealLeft, f32 RealRight, 
					 f32 RealTop, f32 RealBottom, 
					 f32 RealZ)
{
	v2 TopLeft = {RealLeft, RealTop};
	v2 BottomRight = {RealRight, RealBottom};

	f32 ScreenPixelPerBitmapPixelX = Absolute(BottomRight.X - TopLeft.X) / (f32)Buffer->Width;
	f32 ScreenPixelPerBitmapPixelY = Absolute(BottomRight.Y - TopLeft.Y) / (f32)Buffer->Height;

	f32 BitmapPixelPerScreenPixelX = (f32)Bitmap->Width / Absolute(BottomRight.X - TopLeft.X); 
	f32 BitmapPixelPerScreenPixelY = (f32)Bitmap->Height / Absolute(BottomRight.Y - TopLeft.Y);

	f32 BitmapPixelHalfX = BitmapPixelPerScreenPixelX * 0.5f; 
	f32 BitmapPixelHalfY = BitmapPixelPerScreenPixelY * 0.5f;

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

	v2 ClampedTopLeft = 
	{Clamp(TopLeft.X, 0.0f, (f32)Buffer->Width), 
		Clamp(TopLeft.Y, 0.0f, (f32)Buffer->Height)};
	v2 ClampedBottomRight = 
	{Clamp(BottomRight.X, 0.0f, (f32)Buffer->Width), 
		Clamp(BottomRight.Y, 0.0f, (f32)Buffer->Height)};

	f32 OnePixelInBitmapAreaInversed =  
		1 / (BitmapPixelPerScreenPixelX * BitmapPixelPerScreenPixelY);
	f32 OnePixelInBitmapArea =  BitmapPixelPerScreenPixelX * BitmapPixelPerScreenPixelY;

	f32 SumOfHalfWidthX = 1.0f * 0.5f + BitmapPixelPerScreenPixelX * 0.5f;
	f32 SumOfHalfWidthY = 1.0f * 0.5f + BitmapPixelPerScreenPixelY * 0.5f;

	u32 *DestBufferRow = ((u32 *)Buffer->Memory) + (Buffer->Width * DestTop + DestLeft);
	f32 *DepthBufferRow = (f32 *)DepthBuffer->Memory + DestLeft + DestTop * DepthBuffer->Width;

	for(s32 DestY = DestTop;
			DestY < DestBottom;
			++DestY)
	{
		u32 *DestPixel = DestBufferRow;
		f32 *Z = DepthBufferRow;

		for(s32 DestX = DestLeft;
				DestX < DestRight;
				++DestX)
		{
			v2 BitmapPixel = {};

			BitmapPixel.X = (DestX - TopLeft.X) * BitmapPixelPerScreenPixelX;
			BitmapPixel.Y = (DestY - TopLeft.Y) * BitmapPixelPerScreenPixelY;

			/*
				 s32 MinX = RoundF32ToS32(BitmapPixel.X - BitmapPixelHalfX);
				 s32 MinY = RoundF32ToS32(BitmapPixel.Y - BitmapPixelHalfY);

				 s32 MaxX = RoundF32ToS32(BitmapPixel.X + BitmapPixelHalfX);
				 s32 MaxY = RoundF32ToS32(BitmapPixel.Y + BitmapPixelHalfY);

				 MaxX += 1;
				 MaxY += 1;

				 MinX = MinX < 0 ? 0 : MinX;
				 MinY = MinY < 0 ? 0 : MinY;
				 MaxX = MaxX > Bitmap->Width ? Bitmap->Width : MaxX;
				 MaxY = MaxY > Bitmap->Height ? Bitmap->Height : MaxY;
				 */
			s32 SrcX = RoundF32ToS32(BitmapPixel.X);
			s32 SrcY = RoundF32ToS32(BitmapPixel.Y);

			if(0 <= SrcX && SrcX < Bitmap->Width &&
				 0 <= SrcY && SrcY < Bitmap->Height)
			{
				u32 Color = Bitmap->Pixels[Bitmap->Width * ((Bitmap->Height-1) - SrcY) + SrcX];

				f32 A = (Color >> 24) / 255.0f;

				b32 DepthTest = RealZ >= *Z;
				b32 AlphaTest = A >= 0.5f;

				Color = DepthTest ? Color : *DestPixel;
				*DestPixel = AlphaTest ? Color : *DestPixel;

				*Z = (DepthTest && AlphaTest) ? RealZ : *Z;
			}

			DestPixel++;
			Z++;
		}

		DestBufferRow += Buffer->Width;
		DepthBufferRow += DepthBuffer->Width;
	}

#if 0	// NOTE(bjorn): Debug stuff for the bitmap renderer.
	DrawFrame(Buffer, {(f32)DestLeft, (f32)DestTop}, {(f32)DestRight, (f32)DestBottom}, 
						2.0f, {1.0f, 1.0f, 1.0f});

	DrawFrame(Buffer, TopLeft, BottomRight, 1.0f, {1.0f, 0.0f, 0.0f});


	DrawFrame(Buffer, DepthBuffer, ClampedTopLeft, ClampedTopLeft, RealZ, 2.0f, 
						{0.5f, 0.5f, 1.0f});
#endif
}

	internal_function void
DrawBitmapResource(game_offscreen_buffer* Buffer, depth_buffer* DepthBuffer, 
									 font* Font, bitmap_resource* BitmapResource, 
									 f32 RealLeft, f32 RealRight, 
									 f32 RealTop, f32 RealBottom, 
									 f32 RealZ)
{
	f32 Left = RealLeft < RealRight ? RealLeft : RealRight;
	f32 Right = RealLeft > RealRight ? RealLeft : RealRight;
	f32 Top = RealTop < RealBottom ? RealTop : RealBottom;
	f32 Bottom = RealTop > RealBottom ? RealTop : RealBottom;

	f32 Width = Right - Left;
	f32 Height = Bottom - Top;

	if(BitmapResource->Valid)
	{
		DrawBitmap(Buffer, DepthBuffer, &BitmapResource->Bitmap, 
							 Left, Right, Top, Bottom, RealZ);
	}
	else
	{
#if 0 
		v3 Color = {0.0f, 0.5, 0.0f};

		DrawFrame(Buffer, DepthBuffer, Start, End, RealZ, 1.0f, Color);
		DrawLine(Buffer, Start.X, Start.Y, End.X, End.Y, Color.R, Color.G, Color.B);

		//TODO(bjorn): Make the error string get generated when loading the file
		//and then get passed on with the resource package.
		DrawString(Buffer, Font, "Bitmap corrupted or not found.", 
							 Width * 0.023f, Width * 0.023f * 2.0f,
							 Start.X, (f32)Buffer->Height - (Start.Y + Width * 0.023f * 2.0f),
							 Color.R, Color.G, Color.B);
#endif
	}
}

	internal_function void
DrawBitmapResourceRelativeCamera(game_offscreen_buffer* Buffer, 
																 depth_buffer* DepthBuffer, camera* Camera,
																 font* Font, bitmap_resource* BitmapResource, 
																 f32 RealLeft, f32 RealRight, 
																 f32 RealTop, f32 RealBottom, 
																 f32 RealZ)
{
	v3 FocalPoint = Camera->Pos;
	FocalPoint.Z -= Camera->FocalPointDistanceFromFurstum;

	if(RealZ < FocalPoint.Z) { return; }

	v3 TopLeft = {RealLeft, RealTop, RealZ};
	v3 BottomRight = {RealRight, RealBottom, RealZ};

	v2 TopLeftPixelPoint = XYZMetersToXYPixels(Buffer, Camera, TopLeft);
	v2 BottomRightPixelPoint = XYZMetersToXYPixels(Buffer, Camera, BottomRight);
	f32 DistanceFromCamera = -(RealZ - Camera->Pos.Z);

	if(BitmapResource->Valid)
	{
		DrawBitmap(Buffer, DepthBuffer, &BitmapResource->Bitmap, 
							 TopLeftPixelPoint.X, BottomRightPixelPoint.X, 
							 TopLeftPixelPoint.Y, BottomRightPixelPoint.Y, 
							 DistanceFromCamera);
	}
	else
	{
		f32 GlyphPixelWidth = (BottomRightPixelPoint.X - TopLeftPixelPoint.X) / 30.0f;
		f32 GlyphPixelHeight = GlyphPixelWidth * 2.0f;

		v3 Color = {0.0f, 0.5, 0.0f};

		DrawFrame(Buffer, DepthBuffer, TopLeftPixelPoint, BottomRightPixelPoint, RealZ, 1.0f, Color);
		DrawLine(Buffer, DepthBuffer,
						 TopLeftPixelPoint.X, TopLeftPixelPoint.Y, DistanceFromCamera,
						 BottomRightPixelPoint.X, BottomRightPixelPoint.Y, DistanceFromCamera,
						 Color.R, Color.G, Color.B);

		//TODO(bjorn): Make the error string get generated when loading the file
		//and then get passed on with the resource package.
		DrawString(Buffer, DepthBuffer, Font, "Bitmap corrupted or not found.", 
							 GlyphPixelWidth, GlyphPixelHeight,
							 TopLeftPixelPoint.X, TopLeftPixelPoint.Y,
							 DistanceFromCamera,
							 Color.R, Color.G, Color.B);
		DrawString(Buffer, DepthBuffer, Font, (char*)&BitmapResource->FileMeta.Path, 
							 GlyphPixelWidth * 0.5f, GlyphPixelHeight * 0.5f,
							 TopLeftPixelPoint.X, TopLeftPixelPoint.Y + GlyphPixelHeight * 1.5f,
							 DistanceFromCamera,
							 Color.R, Color.G, Color.B);
	}
}

	internal_function void
DrawStringRelativeCamera(game_offscreen_buffer *Buffer, font *Font, char *String, 
												 depth_buffer* DepthBuffer, camera* Camera,
												 f32 GlyphWidth, f32 GlyphHeight, 
												 f32 GlyphOriginLeft, f32 GlyphOriginTop, 
												 f32 RealZ,
												 f32 RealR, f32 RealG, f32 RealB)
{
	v3 FocalPoint = Camera->Pos;
	FocalPoint.Z -= Camera->FocalPointDistanceFromFurstum;

	if(RealZ < FocalPoint.Z) { return; }

	v3 TopLeft = {GlyphOriginLeft, GlyphOriginTop, RealZ};
	v3 BottomRight = {TopLeft.X + GlyphWidth, TopLeft.Y - GlyphHeight, RealZ};

	v2 TopLeftPixelPoint = XYZMetersToXYPixels(Buffer, Camera, TopLeft);
	v2 BottomRightPixelPoint = XYZMetersToXYPixels(Buffer, Camera, BottomRight);
	f32 DistanceFromCamera = -(RealZ - Camera->Pos.Z);

	v2 GlyphPixelDim = BottomRightPixelPoint - TopLeftPixelPoint;

	DrawString(Buffer, DepthBuffer, Font, String, 
						 GlyphPixelDim.X, GlyphPixelDim.Y,
						 TopLeftPixelPoint.X, TopLeftPixelPoint.Y,
						 DistanceFromCamera,
						 RealR, RealG, RealB);
}

	internal_function void
DrawLineRelativeCamera(game_offscreen_buffer *Buffer, depth_buffer* DepthBuffer, 
											 camera* Camera,
											 f32 StartX, f32 StartY, f32 StartZ, 
											 f32 EndX, f32 EndY, f32 EndZ,
											 f32 RealR, f32 RealG, f32 RealB)
{
	v3 FocalPoint = Camera->Pos;
	FocalPoint.Z -= Camera->FocalPointDistanceFromFurstum;

	if(StartZ < FocalPoint.Z && 
		 EndZ < FocalPoint.Z) { return; }

	v3 Start = {StartX, StartY, StartZ};
	v3 End = {EndX, EndY, EndZ};

	v2 PixelStart = XYZMetersToXYPixels(Buffer, Camera, Start);
	v2 PixelEnd = XYZMetersToXYPixels(Buffer, Camera, End);

	f32 DistanceFromCameraStart = -(StartZ - Camera->Pos.Z);
	f32 DistanceFromCameraEnd = -(EndZ - Camera->Pos.Z);

	DrawLine(Buffer, DepthBuffer,
					 PixelStart.X, PixelStart.Y, DistanceFromCameraStart, 
					 PixelEnd.X, PixelEnd.Y, DistanceFromCameraEnd,
					 RealR, RealG, RealB);
}

	internal_function void
DrawCircle(game_offscreen_buffer *Buffer, 
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
DrawCircle(game_offscreen_buffer *Buffer, v2 P, f32 R, v4 C)
{
	DrawCircle(Buffer, P.X, P.Y, R, C.R, C.G, C.B, C.A);
}

#define RENDERER_H
#endif
