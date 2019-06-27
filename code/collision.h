#if !defined(COLLISION_H)
#include "entity.h"

//STUDY(bjorn): Lazy fix. https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
// I dont even understand the maths behind it.
struct intersection_result
{
	b32 Valid;
	f32 t;
};
	internal_function intersection_result
GetTimeOfIntersectionWithLineSegment(v2 S, v2 E, v2 A, v2 B)
{
	intersection_result Result = {};
	f32 denominator = (S.X-E.X) * (A.Y-B.Y) - (S.Y-E.Y) * (A.X-B.X);
	if(denominator != 0)
	{
		f32 u = -(((S.X-E.X) * (S.Y-A.Y) - (S.Y-E.Y) * (S.X-A.X)) / denominator);
		if((u >= 0.0f) && (u <= 1.0f))
		{
			Result.Valid = true;
			Result.t = ((S.X-A.X) * (A.Y-B.Y) - (S.Y-A.Y) * (A.X-B.X)) / denominator;
		}
	}

	return Result;
}

struct closest_point_to_line_result
{
	f32 t;
	v2 P;
};
	internal_function closest_point_to_line_result 
GetClosestPointOnLineSegment(v2 A, v2 B, v2 P)
{
	closest_point_to_line_result Result = {};
	v2 AP = P - A;
	v2 AB = B - A;

	f32 t = Dot(AP, AB) / LengthSquared(AB);

	Result.t = t;
	if (t <= 0)
	{
		Result.P = A;
	}
	else if (t >= 1)
	{
		Result.P = B;
	}
	else
	{
		Result.P = A + AB * t;
	}

	return Result;
}

struct collision_result
{
	b32 Hit;
	b32 Inside;
	f32 TimeOfImpact;
	v2 Normal;
};

enum minkowski_sum_origin
{
	MinkowskiGenus_Target,
	MinkowskiGenus_Movable,
};
struct polygon
{
	s32 NodeCount;
	v2 Nodes[8];
	v2 OriginalLineSeg[8][2];
	minkowski_sum_origin Genus[8];
};
	internal_function polygon 
MinkowskiSum(entity* Target, entity* Movable)
{
	polygon Result = {};
	Result.NodeCount = 8;

	v2 MovableP = Movable->P.XY;
	v2 TargetP  = Target->P.XY;

	vertices TVerts = GetEntityVertices(Target);
	vertices MVerts = GetEntityVertices(Movable);

	//TODO(bjorn): This sum is _NOT_ made for general polygons atm!
	s32 MovableStartIndex;
	{
		v2 MovableR = Movable->R.XY;
		v2 TargetR  = Target->R.XY;
		if(Dot(TargetR, MovableR) > 0)
		{
			MovableStartIndex = (Cross((v3)TargetR, (v3)MovableR).Z > 0) ? 1 : 0;
		}
		else
		{
			MovableStartIndex = (Cross((v3)TargetR, (v3)MovableR).Z > 0) ? 2 : 3;
		}
	}

	v2 MovingCorner = TVerts.Verts[0].XY + (MVerts.Verts[MovableStartIndex].XY - MovableP);
	for(int CornerIndex = 0; 
			CornerIndex < 4; 
			CornerIndex++)
	{
		{
			v2 V0 = TVerts.Verts[(CornerIndex+0)%4].XY;
			v2 V1 = TVerts.Verts[(CornerIndex+1)%4].XY;
			v2 NodeDiff = (V1 - V0);

			s32 ResultIndex = CornerIndex*2+0;

			Result.Genus[ResultIndex] = MinkowskiGenus_Target;
			Result.OriginalLineSeg[ResultIndex][0] = V0;
			Result.OriginalLineSeg[ResultIndex][1] = V1;
			Result.Nodes[ResultIndex] = MovingCorner;

			MovingCorner += NodeDiff;
		}
		{
			v2 V0 = MVerts.Verts[(MovableStartIndex+CornerIndex+0)%4].XY;
			v2 V1 = MVerts.Verts[(MovableStartIndex+CornerIndex+1)%4].XY;
			v2 NodeDiff = (V1 - V0);

			s32 ResultIndex = CornerIndex*2+1;

			Result.Genus[ResultIndex] = MinkowskiGenus_Movable;
			Result.OriginalLineSeg[ResultIndex][0] = V0;
			Result.OriginalLineSeg[ResultIndex][1] = V1;
			Result.Nodes[ResultIndex] = MovingCorner;

			MovingCorner += NodeDiff;
		}
	}

	return Result;
}

#if HANDMADE_INTERNAL
	internal_function void
DEBUGMinkowskiSum(game_offscreen_buffer* Buffer, 
									entity* Target, entity* Movable, 
									m22 GameSpaceToScreenSpace, v2 ScreenCenter)
{
	polygon Sum = MinkowskiSum(Target, Movable);

	for(s32 NodeIndex = 0; 
			NodeIndex < Sum.NodeCount; 
			NodeIndex++)
	{
		v2 N0 = Sum.Nodes[NodeIndex];
		v2 N1 = Sum.Nodes[(NodeIndex+1) % Sum.NodeCount];

		v2 WallNormal = Normalize(CCW90M22() * (N1 - N0));

		DrawLine(Buffer, 
						 ScreenCenter + GameSpaceToScreenSpace * N0, 
						 ScreenCenter + GameSpaceToScreenSpace * N1, 
						 {0, 0, 1});

		v3 NormalColor = {1, 0, 1};
		if(Sum.Genus[NodeIndex] == MinkowskiGenus_Target) { NormalColor = {0, 1, 1}; }

		DrawLine(Buffer, 
						 ScreenCenter + GameSpaceToScreenSpace * (N0 + N1) * 0.5f, 
						 ScreenCenter + GameSpaceToScreenSpace * ((N0 + N1) * 0.5f + WallNormal * 0.2f), 
						 NormalColor);
	}
}
#endif

//STUDY(bjorn): This is from a book about collision. Comeback and make sure you
//understand what is happening.
	inline f32 
SquareDistancePointToLineSegment(v2 A, v2 B, v2 C)
{
	v2 AB = B - A;
	v2 AC = C - A;
	v2 BC = C - B;
	f32 e = Dot(AC, AB);
	if(e <= 0.0f) { return Dot(AC, AC); }
	f32 f = Dot(AB, AB);
	if(e >= f) { return Dot(BC, BC); }
	return Dot(AC, AC) - Square(e) / f;
}

#define COLLISION_H
#endif
