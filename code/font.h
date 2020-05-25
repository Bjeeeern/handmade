#if !defined(FONT_H)

#include "math.h"
#include "memory.h"
#include "intrinsics.h"

#pragma pack(push, 1)
struct font
{
	f32 AdvanceWidth;
	f32 MaximumDescent;

	s32 UnicodeCodePointCount;
#if 0
unicode_to_glyph_data *Entries = (unicode_to_glyph_data *)(Font + 1);
#endif
};

struct unicode_to_glyph_data
{
	u32 UnicodeCodePoint;
	u32 OffsetToGlyphData;
	s32 QuadraticCurveCount;
};

struct quadratic_curve
{
	v2 Srt;
	v2 Con;
	v2 End;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct true_type_font
{
	u32 TypeOfFontFile;
	u16 NumberOfTables;
	u16 BinarySearchRange;
	u16 BinarySearchIterations;
	u16 BinarySearchClosestPowerToRangeDiff;
};

struct ttf_table_entry
{
	u8 Tag[4];
	u32 Checksum;
	u32 Offset;
	u32 Length;
};

struct ttf_header
{
	s32 Version;
	s32 FontRevision;
	u32 ChecksumAdjustment;
	u32 MagicNumber;
	u16 Flags;
	u16 UnitsPerEM;
	s64 Created;
	s64 Modified;
	s16 xMin;
	s16 yMin;
	s16 xMax;
	s16 yMax;
	u16 MacStyle;
	u16 LowestRecPPEM;
	s16 FontDirectionHint;
	s16 IndexToLocFormat;
	s16 GlyphDataFormat;
};

struct ttf_cmap_index
{
	u16 Version;
	u16 NumberOfSubtables;
};

struct ttf_cmap_encoding
{
	u16 PlatformID;
	u16 PlatformSpecificID;
	u32 Offset;
};

struct ttf_cmap_format_4
{
	u16 FormatID;
	u16 SubtableLength;
	u16 LanguageCode;

	u16 SegCountx2;
	u16 SearchRange;
	u16 EntrySelector;
	u16 RangeShift;
};

struct ttf_glyph_top
{
	s16 NumberOfContours;
	
	s16 MinX;
	s16 MinY;
	s16 MaxX;
	s16 MaxY;

	u8 Flags;
};
#pragma pack(pop)

struct glyph_point_flags
{
	b32 PointIsOnCurve;
	b32 ShortX;
	b32 ShortY;
	b32 RepeatThisFlagXTimes;
	b32 IsSameAsPreviousX;
	b32 PositiveShortX;
	b32 IsSameAsPreviousY;
	b32 PositiveShortY;

};
glyph_point_flags 
GetGlyphPointFlags(u8 Flags)
{
	glyph_point_flags Result = {};

	Result.PointIsOnCurve = (0x01 & Flags) != 0;
	Result.ShortX = (0x02 & Flags) != 0;
	Result.ShortY = (0x04 & Flags) != 0;
	Result.RepeatThisFlagXTimes = (0x08 & Flags) != 0;
	Result.IsSameAsPreviousX = (0x10 & Flags) != 0;
	Result.PositiveShortX = Result.IsSameAsPreviousX;
	Result.IsSameAsPreviousY = (0x20 & Flags) != 0;
	Result.PositiveShortY = Result.IsSameAsPreviousY;

	return Result;
}

struct glyph_component_flags
{
	b32 ArgsAre16Bit;
	b32 ArgsAreSigned;
	b32 ArgsNeedsToBeGridFit;
	b32 Unscaled;
	b32 SimpleScalar;
	b32 XYScalingVector;
	b32 TwoByTwoTransform;
	b32 UseThisComponentsMetrics;
	b32 OneMoreComponentFollowing;
};
glyph_component_flags 
GetGlyphComponentFlags(u16 Flags)
{
	glyph_component_flags Result = {};

	Result.ArgsAre16Bit = (0x0001 & Flags) != 0;
	Result.ArgsAreSigned = (0x0002 & Flags) != 0;
	Result.ArgsNeedsToBeGridFit = (0x0004 & Flags) != 0;
	Result.SimpleScalar = (0x0008 & Flags) != 0;
	Result.OneMoreComponentFollowing = (0x0020 & Flags) != 0;
	Result.XYScalingVector = (0x0040 & Flags) != 0;
	Result.TwoByTwoTransform = (0x0080 & Flags) != 0;
	Result.UseThisComponentsMetrics = (0x0200 & Flags) != 0;

	Result.Unscaled = (!Result.SimpleScalar && 
										 !Result.XYScalingVector && 
										 !Result.TwoByTwoTransform);
	return Result;
}


struct glyph_result
{
	u32 Offset;
	s32 CurveCount;
};
	internal_function glyph_result
CreateGlyph(memory_arena *Arena, u16 DEBUG_GlyphIndex, f32 DEBUG_MaxAdvanceWidth,
						u8 *TOTFF, u32 GlyphData, u8 *TOFontFile, s16 ContourCount, 
						s16 OriginX, s16 OriginY, 
						s16 YZero, s16 YOne, 
						s16 XZero, s16 XOne)
{
	glyph_result Result = {};

	Assert(ContourCount > 0);

	u16 *ContourEndpoints = (u16 *)(TOTFF + GlyphData);
	u16 InstructionLength = EndianSwap(*(u16 *)(TOTFF + GlyphData + ContourCount * 2));

	u32 FlagsOffset = GlyphData + ContourCount * 2 + 2 + InstructionLength;
	u8* Flags = TOTFF + FlagsOffset;

	u32 PosXOffset;
	u32 PosYOffset;
	{
		s32 LastContourEndpoint = EndianSwap(ContourEndpoints[ContourCount - 1]);

		s32 FlagByteCount = 0;
		s32 PointXByteCount = 0;

		for(s32 PointIndex = 0;
				PointIndex <= LastContourEndpoint;
			 )
		{
			glyph_point_flags Flag = GetGlyphPointFlags(Flags[FlagByteCount]);

			s32 XTimes = Flag.RepeatThisFlagXTimes ? Flags[FlagByteCount+1] : 0;

			if(Flag.ShortX)
			{
				PointXByteCount += 1 + 1 * XTimes;
			}
			else
			{
				if(!Flag.IsSameAsPreviousX)
				{
					PointXByteCount += 2 + 2 * XTimes;
				}
			}

			FlagByteCount += Flag.RepeatThisFlagXTimes ? 2 : 1;
			PointIndex += 1 + XTimes;
		}

		PosXOffset = FlagsOffset + FlagByteCount;
		PosYOffset = PosXOffset + PointXByteCount;
	}

	s32 QuadraticCurveCount = 0;
	u32 OffsetToGlyphData = 0;
	{
		s32 FlagByteCount = 0;
		s32 PointXByteCount = 0;
		s32 PointYByteCount = 0;

		s16 AbsX = OriginX;
		s16 AbsY = OriginY;

		s32 ContourIndex = 0;
		s32 LastContourEndpoint = EndianSwap(ContourEndpoints[ContourCount - 1]);
		b32 ReachedEndpoint = 0;
		b32 IsOnCurve = false;

		v2 ContourStartPoint = {};
		v2 PreviousCurveEndPoint = {};
		v2 Point = {};

		s32 CurveState = 0;

		for(s32 PointIndex = 0;
				PointIndex <= LastContourEndpoint;
			 )
		{
			glyph_point_flags Flag = GetGlyphPointFlags(Flags[FlagByteCount]);
			if(PointIndex == 0) { }//Assert(Flag.PointIsOnCurve); }

			s32 XTimes = Flag.RepeatThisFlagXTimes ? Flags[FlagByteCount+1] : 0;

			for(s32 XTimesIndex = 0;
					XTimesIndex < XTimes + 1;
					XTimesIndex++)
			{
				s16 RelX;
				if(Flag.ShortX)
				{
					RelX = (Flag.PositiveShortX ? 1 : -1) * 
						(s16)(*(u8 *)(TOTFF + PosXOffset + PointXByteCount));
				}
				else
				{
					RelX = Flag.IsSameAsPreviousX ? 0 : 
						EndianSwap(*(s16 *)(TOTFF + PosXOffset + PointXByteCount));
				}
				AbsX += RelX;

				s16 RelY;
				if(Flag.ShortY)
				{
					RelY = (Flag.PositiveShortY ? 1 : -1) * 
						(s16)(*(u8 *)(TOTFF + PosYOffset + PointYByteCount));
				}
				else
				{
					RelY = Flag.IsSameAsPreviousY ? 0 : 
						EndianSwap(*(s16 *)(TOTFF + PosYOffset + PointYByteCount));
				}
				AbsY += RelY;

				b32 ReachedEndpointAtPreviousPointIndex = ReachedEndpoint;
				s32 ContourEndpoint = EndianSwap(ContourEndpoints[ContourIndex]);
				ReachedEndpoint = ContourEndpoint == PointIndex;
				if(ReachedEndpoint) { ContourIndex++; }

				b32 IsOnCurveAtPreviousPointIndex = IsOnCurve;
				IsOnCurve = Flag.PointIsOnCurve;

				if(ReachedEndpointAtPreviousPointIndex)
				{
					//Assert(IsOnCurve);
				}

				b32 IsAtEnd = PointIndex == LastContourEndpoint;

				//TODO(bjorn): Better names, these are confusing.
				//TODO(bjorn): The left side bearing makes no sense.
				f32 X = (f32)(AbsX - YZero) / (f32)(YOne - YZero);
				f32 Y = (f32)(AbsY - XZero) / (f32)(XOne - XZero);
				Assert(-1.0f <= X && X <= DEBUG_MaxAdvanceWidth);
				Assert(-1.0f <= Y && Y <= 1.0f);

				v2 PreviousPoint = Point;
				Point = {X, Y};

				if(PointIndex == 0)
				{
					ContourStartPoint = Point;
					PreviousCurveEndPoint = Point;
				}
				else if(ReachedEndpointAtPreviousPointIndex)
				{
					quadratic_curve *QC = PushStruct(Arena, quadratic_curve); 

					if(IsOnCurveAtPreviousPointIndex)
					{
						QC->Srt = PreviousCurveEndPoint;
						QC->Con = (PreviousCurveEndPoint + ContourStartPoint) * 0.5f;
						QC->End = ContourStartPoint;
					}
					else
					{
						QC->Srt = PreviousCurveEndPoint;
						QC->Con = PreviousPoint;
						QC->End = ContourStartPoint;
					}

					QuadraticCurveCount++;
					if(!OffsetToGlyphData)
					{
						OffsetToGlyphData = (u32)((u8 *)QC - TOFontFile);
					}

					ContourStartPoint = Point;
					PreviousCurveEndPoint = Point;
				}
				else
				{
					if(CurveState == 0)
					{
						if(IsOnCurve)
						{
							quadratic_curve *QC = PushStruct(Arena, quadratic_curve); 

							QC->Srt = PreviousCurveEndPoint;
							QC->Con = (PreviousCurveEndPoint + Point) * 0.5f;
							QC->End = Point;

							PreviousCurveEndPoint = QC->End;
							QuadraticCurveCount++;
							if(!OffsetToGlyphData)
							{
								OffsetToGlyphData = (u32)((u8 *)QC - TOFontFile);
							}
						}
						else
						{
							CurveState = 1;
						}
					}
					else if(CurveState == 1)
					{
						quadratic_curve *QC = PushStruct(Arena, quadratic_curve); 

						if(IsOnCurve)
						{
							QC->Srt = PreviousCurveEndPoint;
							QC->Con = PreviousPoint;
							QC->End = Point;

							CurveState = 0;
						}
						else
						{
							QC->Srt = PreviousCurveEndPoint;
							QC->Con = PreviousPoint;
							QC->End = (PreviousPoint + Point) * 0.5f;
						}

						PreviousCurveEndPoint = QC->End;
						QuadraticCurveCount++;
						if(!OffsetToGlyphData)
						{
							OffsetToGlyphData = (u32)((u8 *)QC - TOFontFile);
						}
					}
					else
					{
						Assert(0);
					}
				}
				if(IsAtEnd)
				{
					quadratic_curve *QC = PushStruct(Arena, quadratic_curve); 

					if(IsOnCurve)
					{
						QC->Srt = PreviousCurveEndPoint;
						QC->Con = (PreviousCurveEndPoint + ContourStartPoint) * 0.5f;
						QC->End = ContourStartPoint;
					}
					else
					{
						QC->Srt = PreviousCurveEndPoint;
						QC->Con = Point;
						QC->End = ContourStartPoint;
					}

					QuadraticCurveCount++;
					if(!OffsetToGlyphData)
					{
						OffsetToGlyphData = (u32)((u8 *)QC - TOFontFile);
					}
				}

				PointXByteCount += Flag.ShortX ? 1 : ( Flag.IsSameAsPreviousX ? 0 : 2);
				PointYByteCount += Flag.ShortY ? 1 : ( Flag.IsSameAsPreviousY ? 0 : 2);

				PointIndex += 1;
			}

			FlagByteCount += Flag.RepeatThisFlagXTimes ? 2 : 1;
		}
	}

	Result.Offset = OffsetToGlyphData;
	Result.CurveCount = QuadraticCurveCount;
	return Result;
}

internal_function font *
TrueTypeFontToFont(memory_arena *Arena, true_type_font *TTF)
{
	u8 *TOF = (u8 *)TTF;

	u32 Header = 0;
	u32 Hmtx = 0;
	u32 HorizontalHeader = 0;
	u32 CharacterGlyphMapping = 0;
	u32 GlyphLocation = 0;
	u32 Glyphs = 0;
	//s32 OSData = 0;
	{
		s32 NumberOfTables = EndianSwap(*(u16 *)(TOF + 4));
		u32 Tables = 12;

		for(s32 TableIndex = 0;
				TableIndex < NumberOfTables;
				++TableIndex)
		{
			s32 Table = Tables + TableIndex * 16;

			u8 *Tag = TOF + Table;
			u32 Offset = EndianSwap(*(u32 *)(TOF + Table + 8));

			if(*(s32 *)Tag == *(s32 *)&"head"){ Header = Offset; }
			if(*(s32 *)Tag == *(s32 *)&"hmtx"){ Hmtx = Offset; }
			if(*(s32 *)Tag == *(s32 *)&"hhea"){ HorizontalHeader = Offset; }
			if(*(s32 *)Tag == *(s32 *)&"cmap"){ CharacterGlyphMapping = Offset; }
			if(*(s32 *)Tag == *(s32 *)&"loca"){ GlyphLocation = Offset; }
			if(*(s32 *)Tag == *(s32 *)&"glyf"){ Glyphs = Offset; }
			//if(*(s32 *)Tag == *(s32 *)&"OS/2"){ OSData = Offset; }
		}
	}
	Assert(Header && HorizontalHeader && CharacterGlyphMapping && GlyphLocation && Glyphs);

	b32 BaselineIsFontUnitZero = 0;
	b32 IndexToGlyphLocationFormat = 0;
	{
		u16 Flags = EndianSwap(*(u16 *)(TOF + Header + 16));
		BaselineIsFontUnitZero = (Flags & 0x0001);

		IndexToGlyphLocationFormat = (b32)EndianSwap(*(s16 *)(TOF + Header + 50));
	}
	Assert(BaselineIsFontUnitZero);

	font *Font = PushStruct(Arena, font);
	s16 BaselineBaselineDiff = 0;
	{
		s16 Ascent = EndianSwap(*(s16 *)(TOF + HorizontalHeader + 4));
		s16 Descent = EndianSwap(*(s16 *)(TOF + HorizontalHeader + 6));
		s16 LineGap = EndianSwap(*(s16 *)(TOF + HorizontalHeader + 8));

		Assert(Descent <= 0);
		BaselineBaselineDiff = Ascent - Descent + LineGap;
		Assert(BaselineBaselineDiff > 0);
		Font->MaximumDescent = (f32)Descent / (f32)BaselineBaselineDiff;

		u16 NumberOfAdvanceMetrics = EndianSwap(*(u16 *)(TOF + HorizontalHeader + 34));
		Font->AdvanceWidth = 1.0f;

		u16 AdvanceWidth = EndianSwap(*(u16 *)(TOF + HorizontalHeader + 10));
		if(NumberOfAdvanceMetrics > 5)//NOTE(bjorn): Is there data for the character 'A'.
		{
			u16 PotentiallySmallerAdvanceWidth = EndianSwap(*(u16 *)(TOF + Hmtx + 4 * 4));
			Font->AdvanceWidth = (f32)AdvanceWidth / (f32)PotentiallySmallerAdvanceWidth;
		}
		Assert(AdvanceWidth > 0);
		Assert(Font->AdvanceWidth >= 1.0f);
	}

	s32 Format4 = 0;
	{
		s32 NumberOfSubtables = EndianSwap(*(u16 *)(TOF + CharacterGlyphMapping + 2));
		u32 SubTables = CharacterGlyphMapping + 4;
		for(s32 SubtableIndex = 0;
				SubtableIndex < NumberOfSubtables;
				++SubtableIndex)
		{
			u32 SubTable = SubTables + SubtableIndex * 8;
			u32 OffsetFromTopOfCMapTable = EndianSwap(*(u32 *)(TOF + SubTable + 4));
			u32 Format = CharacterGlyphMapping + OffsetFromTopOfCMapTable;

			u16 FormatID = EndianSwap(*(u16 *)(TOF + Format));
			if(FormatID == 4) { Format4 = Format; }
		}
	}
	Assert(Format4);

	s32 NumberOfUnicodeCodePoints = 0;
	{
		s32 SegmentCount = EndianSwap(*(u16 *)(TOF + Format4 + 6)) >> 1;
		u32 EndCodes = Format4 + 14;
		u32 StartCodes = EndCodes + SegmentCount * 2 + 2;

		for(s32 SegmentIndex = 0;
				SegmentIndex < SegmentCount;
				SegmentIndex++)
		{
			u32 EndCode = EndianSwap(*(u16 *)(TOF + EndCodes + 2 * SegmentIndex));
			u32 StartCode = EndianSwap(*(u16 *)(TOF + StartCodes + 2 * SegmentIndex));
			NumberOfUnicodeCodePoints += EndCode - StartCode + 1;
		}
	}
	Assert(NumberOfUnicodeCodePoints);
	Font->UnicodeCodePointCount = NumberOfUnicodeCodePoints;

	unicode_to_glyph_data *UnicodeToGlyphArray = PushArray(Arena, 
																												 NumberOfUnicodeCodePoints, 
																												 unicode_to_glyph_data);
	{
		s32 SegmentCount = EndianSwap(*(u16 *)(TOF + Format4 + 6)) >> 1;
		u32 EndCodes = Format4 + 14;
		u32 StartCodes = EndCodes + SegmentCount * 2 + 2;
		u32 Deltas = StartCodes + SegmentCount * 2;
		u32 RangeOffsets = Deltas + SegmentCount * 2;

		s32 MappingIndex = 0;
		for(s32 SegmentIndex = 0;
				SegmentIndex < SegmentCount;
				SegmentIndex++)
		{
			u16 EndCode = EndianSwap(*(u16 *)(TOF + EndCodes + 2 * SegmentIndex));
			u16 StartCode = EndianSwap(*(u16 *)(TOF + StartCodes + 2 * SegmentIndex));
			u16 Delta = EndianSwap(*(u16 *)(TOF + Deltas + 2 * SegmentIndex));
			u16 RangeOffset = EndianSwap(*(u16 *)(TOF + RangeOffsets + 2 * SegmentIndex));

			if(SegmentIndex == SegmentCount - 1)
			{
				UnicodeToGlyphArray[MappingIndex].UnicodeCodePoint = StartCode;
				UnicodeToGlyphArray[MappingIndex].OffsetToGlyphData = 0;
				continue;
			}

			for(u16 UnicodeCodePoint = StartCode;
					UnicodeCodePoint < (EndCode + 1);
					UnicodeCodePoint++, MappingIndex++)
			{
				UnicodeToGlyphArray[MappingIndex].UnicodeCodePoint = UnicodeCodePoint;

				u16 GlyphIndex = 0;

				if(RangeOffset == 0)
				{
					GlyphIndex = Delta + UnicodeCodePoint;
				}
				else
				{
					GlyphIndex = EndianSwap(*(u16 *)((TOF + RangeOffsets + SegmentIndex * 2) + 
																					 RangeOffset + 
																					 (UnicodeCodePoint - StartCode) * 2));
					if(GlyphIndex != 0)
					{
						GlyphIndex += Delta;
					}
				}

				UnicodeToGlyphArray[MappingIndex].OffsetToGlyphData = GlyphIndex;
			}
		}
	}

	{
		for(s32 MappingIndex = 0;
				MappingIndex < Font->UnicodeCodePointCount;
				MappingIndex++)
		{
			u16 GlyphIndex = (u16)UnicodeToGlyphArray[MappingIndex].OffsetToGlyphData;

			u16 AdvanceWidth = EndianSwap(*(u16 *)(TOF + Hmtx + GlyphIndex * 4));
			u16 LeftSideBearing = EndianSwap(*(u16 *)(TOF + Hmtx + GlyphIndex * 4 + 2));

			u32 Glyph = 0;
			u32 GlyphSize = 0;
			u32 LocationOffset = GlyphLocation + GlyphIndex * (IndexToGlyphLocationFormat ? 4 : 2);
			if(IndexToGlyphLocationFormat)
			{
				u32 OffsetFromGlyphTable = EndianSwap(*(u32 *)(TOF + LocationOffset));
				u32 FollowingOffsetFromGlyphTable = EndianSwap(*(u32 *)(TOF + LocationOffset + 4));

				Glyph = Glyphs + OffsetFromGlyphTable;
				GlyphSize = FollowingOffsetFromGlyphTable - OffsetFromGlyphTable;
			}
			else
			{
				u32 OffsetFromGlyphTable = (EndianSwap(*(u16 *)(TOF + LocationOffset)) << 1);
				u32 FollowingOffsetFromGlyphTable = (EndianSwap(*(u16 *)(TOF + LocationOffset + 2)) << 1);

				Glyph = Glyphs + OffsetFromGlyphTable;
				GlyphSize = FollowingOffsetFromGlyphTable - OffsetFromGlyphTable;
			}

			s16 NumberOfContours = EndianSwap(*(s16 *)(TOF + Glyph));
			u32 GlyphData = Glyph + 10;

			//TODO(bjorn): Support composite glyphs.
			//Assert(NumberOfContours >= 0);
			u32 GlyphOffset = 0; 
			s32 QuadraticCurveCount = 0;
			s16 OriginX = 0;
			s16 OriginY = 0;
			if(NumberOfContours == 0 || GlyphSize == 0)
			{
				//NOTE(bjorn): Do nothing, this glyph is invisible.
			}
			else if(NumberOfContours > 0)
			{
				glyph_result Result = CreateGlyph(Arena, GlyphIndex, Font->AdvanceWidth,
																					TOF, GlyphData, (u8 *)Font, NumberOfContours,
																					OriginX, OriginY, 
																					LeftSideBearing, LeftSideBearing + AdvanceWidth, 
																					0, 0 + BaselineBaselineDiff);
				
				GlyphOffset = Result.Offset;
				QuadraticCurveCount = Result.CurveCount;
			}
			else
			{
			}

			UnicodeToGlyphArray[MappingIndex].OffsetToGlyphData = GlyphOffset;
			UnicodeToGlyphArray[MappingIndex].QuadraticCurveCount = QuadraticCurveCount;
		}
	}

	return Font;
}

struct unicode_array_result
{
	s32 Count;
	u16 *UnicodeCodePoint;
};
	internal_function unicode_array_result 
UnicodeStringToUnicodeCodePointArray(u8 *String, u16 *Buffer, s32 BufferSize)
{
	unicode_array_result  Result = {0, Buffer};

	while(*String != 0)
	{
		while((*String & 0b11000000) == 0b10000000) 
		{ 
			String += 1; 
			if(*String != 0) { return Result; }
		}

		s32 NumberOfBytes;
		{
			if((*String & 0b10000000) == 0b00000000) { NumberOfBytes = 1; }
			else if((*String & 0b11100000) == 0b11000000) { NumberOfBytes = 2; }
			else if((*String & 0b11110000) == 0b11100000) { NumberOfBytes = 3; }
			else if((*String & 0b11111000) == 0b11110000) { NumberOfBytes = 4; }
			else { NumberOfBytes = 1; }
		}

		u16 CodePoint;
		{
			if(NumberOfBytes == 4)
			{
				CodePoint = 0;
			}
			else if(NumberOfBytes == 3)
			{
				b32 SecondByteIsLegal = (String[1] & 0b11000000) == 0b10000000;
				b32 ThirdByteIsLegal = (String[2] & 0b11000000) == 0b10000000;
				if(!SecondByteIsLegal || !ThirdByteIsLegal)
				{
					CodePoint = 0;
				}
				else
				{
					CodePoint = ((((u16)String[0] & 0b0000000000001111) << 12) |
											 (((u16)String[1] & 0b0000000000111111) << 6) |
											 (((u16)String[2] & 0b0000000000111111) << 0) );
				}
			}
			else if(NumberOfBytes == 2)
			{
				b32 SecondByteIsLegal = (String[1] & 0b11000000) == 0b10000000;
				if(!SecondByteIsLegal)
				{
					CodePoint = 0;
				}
				else
				{
					CodePoint = ((((u16)String[0] & 0b0000000000011111) << 6) |
											 (((u16)String[1] & 0b0000000000111111) << 0) );
				}
			}
			else
			{
				CodePoint = (u16)String[0] & 0b0000000001111111;
			}
		}

		if(Result.Count >= BufferSize) { return Result; }
		Result.UnicodeCodePoint[Result.Count++] = CodePoint;

		String += NumberOfBytes;
	}
	return Result;
}

#define FONT_H
#endif
