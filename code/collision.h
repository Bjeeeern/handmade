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

struct contact
{
	//TODO(bjorn): Do I really need to pass the entities here?
	entity* A;
	entity* B;
	v3 P;
	v3 N;
	f32 PenetrationDepth;
	f32 Restitution;
	f32 Friction;
};

struct contact_result
{
	u32 Count;
	contact E[16];
};

internal_function void 
AddContact(contact_result* Contacts, entity* A, entity* B, 
					 v3 P, v3 N, f32 PenetrationDepth, f32 Restitution, f32 Friction)
{
	Assert(IsWithinInclusive(Restitution, 0, 1));
	Assert(IsWithinInclusive(Friction, 0, 1));
	Assert(Contacts->Count < ArrayCount(Contacts->E));
	if(Contacts->Count < ArrayCount(Contacts->E))
	{
		contact* Contact = Contacts->E + Contacts->Count++;
		*Contact = {A, B, P, N, PenetrationDepth, Restitution, Friction};
	}
}

	internal_function void
GenerateContactsFromPrimitivePair(contact_result* Contacts, 
																	entity* Entity_A, entity* Entity_B,
																	body_primitive* A, body_primitive* B
#if HANDMADE_INTERNAL
																	,render_group* RenderGroup
#endif
																	)
{
	if(A->CollisionShape == CollisionShape_Sphere && 
		 B->CollisionShape == CollisionShape_Sphere)
	{
		v3 C0 = Entity_A->Tran * (A->Tran * v3{   0,0,0});
		v3 Q0 = Entity_A->Tran * (A->Tran * v3{0.5f,0,0});
		v3 C1 = Entity_B->Tran * (B->Tran * v3{   0,0,0});
		v3 Q1 = Entity_B->Tran * (B->Tran * v3{0.5f,0,0});

		b32 Overlaps = Magnitude(C1-C0) <= (Magnitude(Q0-C0) + Magnitude(Q1-C1));
		if(Overlaps)
		{
			AddContact(Contacts, Entity_A, Entity_B, 
								 (C0+C1)*0.5f, 
								 Normalize(C1-C0), 
								 (Magnitude(Q0-C0)+Magnitude(Q1-C1)) - Magnitude(C1-C0),
								 (Entity_A->Body.Restitution + Entity_B->Body.Restitution)*0.5f,
								 Entity_A->Body.Friction * Entity_B->Body.Friction);
		}
	}
	if(A->CollisionShape == CollisionShape_AABB && 
		 B->CollisionShape == CollisionShape_AABB)
	{
		m44 ObjAToWorld = Entity_A->Tran * A->Tran;
		v3 A_HalfSize[4]; 
		A_HalfSize[0] = ObjAToWorld * v3{0.5f,0.0f,0.0f} - ObjAToWorld * v3{0,0,0};
		A_HalfSize[1] = ObjAToWorld * v3{0.0f,0.5f,0.0f} - ObjAToWorld * v3{0,0,0};
		A_HalfSize[2] = ObjAToWorld * v3{0.0f,0.0f,0.5f} - ObjAToWorld * v3{0,0,0};
		A_HalfSize[3] = ObjAToWorld * v3{0.5f,0.5f,0.5f} - ObjAToWorld * v3{0,0,0};
		m44 ObjBToWorld = Entity_B->Tran * B->Tran;
		v3 B_HalfSize[4]; 
		B_HalfSize[0] = ObjBToWorld * v3{0.5f,0.0f,0.0f} - ObjBToWorld * v3{0,0,0};
		B_HalfSize[1] = ObjBToWorld * v3{0.0f,0.5f,0.0f} - ObjBToWorld * v3{0,0,0};
		B_HalfSize[2] = ObjBToWorld * v3{0.0f,0.0f,0.5f} - ObjBToWorld * v3{0,0,0};
		B_HalfSize[3] = ObjBToWorld * v3{0.5f,0.5f,0.5f} - ObjBToWorld * v3{0,0,0};

		//TODO(bjorn): Non origin center of mass probably messes these calculations up.
		v3 Axes[15];
		Axes[0] = Normalize(A_HalfSize[0]);
		Axes[1] = Normalize(A_HalfSize[1]);
		Axes[2] = Normalize(A_HalfSize[2]);
		Axes[3] = Normalize(B_HalfSize[0]);
		Axes[4] = Normalize(B_HalfSize[1]);
		Axes[5] = Normalize(B_HalfSize[2]);
		
		Axes[ 6] = Cross(Axes[0], Axes[3]);
		Axes[ 7] = Cross(Axes[0], Axes[4]);
		Axes[ 8] = Cross(Axes[0], Axes[5]);
		Axes[ 9] = Cross(Axes[1], Axes[3]);
		Axes[10] = Cross(Axes[1], Axes[4]);
		Axes[11] = Cross(Axes[1], Axes[5]);
		Axes[12] = Cross(Axes[2], Axes[3]);
		Axes[13] = Cross(Axes[2], Axes[4]);
		Axes[14] = Cross(Axes[2], Axes[5]);

		v3 DeltaP = Entity_B->P - Entity_A->P;
		f32 SmallestOverlap = positive_infinity32;
		s32 SmallestOverlapIndex = -1;
		for(u32 AxisIndex = 0;
				AxisIndex < ArrayCount(Axes);
				AxisIndex++)
		{
			v3 Axis = Axes[AxisIndex];
			if(MagnitudeSquared(Axis) < 0.001f) { continue; }
			if(AxisIndex >= 6)
			{
				Axis = Normalize(Axis);
				Axes[AxisIndex] = Axis;
			}

			f32 Overlap;
			{
				f32 AProjection = (Absolute(Dot(A_HalfSize[0], Axis)) +
													 Absolute(Dot(A_HalfSize[1], Axis)) +
													 Absolute(Dot(A_HalfSize[2], Axis)));
				f32 BProjection = (Absolute(Dot(B_HalfSize[0], Axis)) +
													 Absolute(Dot(B_HalfSize[1], Axis)) +
													 Absolute(Dot(B_HalfSize[2], Axis)));
				f32 DistanceProjected = Absolute(Dot(DeltaP, Axis));

				Overlap = AProjection + BProjection - DistanceProjected;
			}

			if(Overlap < 0) { return; }
			if(Overlap < SmallestOverlap)
			{
				SmallestOverlap = Overlap;
				SmallestOverlapIndex = AxisIndex;
			}
		}
		Assert(SmallestOverlapIndex >= 0);

		b32 IsFaceAxis = SmallestOverlapIndex < 6; 
		if(IsFaceAxis)
		{
			//TODO(bjorn): Maybe find up to eight contacts here.
			v3 Axis = Axes[SmallestOverlapIndex];
			if(Dot(DeltaP, Axis) < 0) { Axis *= -1.0f; }

			v3 Vertex;
			b32 AFaceBVertex = SmallestOverlapIndex < 3;
			if(AFaceBVertex)
			{
				Vertex = B_HalfSize[3];
				if(Dot(B_HalfSize[0], Axis) > 0) { Vertex -= 2.0f*B_HalfSize[0]; }
				if(Dot(B_HalfSize[1], Axis) > 0) { Vertex -= 2.0f*B_HalfSize[1]; }
				if(Dot(B_HalfSize[2], Axis) > 0) { Vertex -= 2.0f*B_HalfSize[2]; }
				Vertex += Entity_B->P;
			}
			else
			{
				Vertex = A_HalfSize[3];
				if(Dot(A_HalfSize[0], Axis) < 0) { Vertex -= 2.0f*A_HalfSize[0]; }
				if(Dot(A_HalfSize[1], Axis) < 0) { Vertex -= 2.0f*A_HalfSize[1]; }
				if(Dot(A_HalfSize[2], Axis) < 0) { Vertex -= 2.0f*A_HalfSize[2]; }
				Vertex += Entity_A->P;
			}

#if HANDMADE_INTERNAL
			{
				v3 APSize = 0.08f*v3{1,1,1};
				m44 T = ConstructTransform(Vertex, QuaternionIdentity(), APSize);
				PushWireCube(RenderGroup, T, {0.7f,0,0,1});
			}
#endif

			AddContact(Contacts, Entity_A, Entity_B, 
								 Vertex, 
								 Axis, 
								 SmallestOverlap,
								 (Entity_A->Body.Restitution + Entity_B->Body.Restitution)*0.5f,
								 Entity_A->Body.Friction * Entity_B->Body.Friction);
		}
		else
		{
			v3 Axis = Axes[SmallestOverlapIndex];
			if(Dot(DeltaP, Axis) < 0) { Axis *= -1.0f; }

			v3 A_EdgeEnds[2] = {A_HalfSize[3], A_HalfSize[3]};
			v3 B_EdgeEnds[2] = {B_HalfSize[3], B_HalfSize[3]};
			u32 A_AxisIndex = (SmallestOverlapIndex-6) / 3;
			u32 B_AxisIndex = (SmallestOverlapIndex-6) % 3;
			for(u32 AxisIndex = 0;
					AxisIndex < 3;
					AxisIndex++)
			{
				if(AxisIndex != A_AxisIndex &&
					 Dot(A_HalfSize[AxisIndex], Axis) < 0) 
				{ 
					A_EdgeEnds[0] -= 2.0f*A_HalfSize[AxisIndex]; 
					A_EdgeEnds[1] -= 2.0f*A_HalfSize[AxisIndex]; 
				}

				if(AxisIndex != B_AxisIndex &&
					 Dot(B_HalfSize[AxisIndex], Axis) > 0) 
				{ 
					B_EdgeEnds[0] -= 2.0f*B_HalfSize[AxisIndex]; 
					B_EdgeEnds[1] -= 2.0f*B_HalfSize[AxisIndex]; 
				}
			}
			A_EdgeEnds[1] -= 2.0f*A_HalfSize[A_AxisIndex]; 
			B_EdgeEnds[1] -= 2.0f*B_HalfSize[B_AxisIndex]; 

			A_EdgeEnds[0] += Entity_A->P;
			A_EdgeEnds[1] += Entity_A->P;
			B_EdgeEnds[0] += Entity_B->P;
			B_EdgeEnds[1] += Entity_B->P;

#if HANDMADE_INTERNAL
			PushVector(RenderGroup, M44Identity(), A_EdgeEnds[0], A_EdgeEnds[1], {1,1,0});
			PushVector(RenderGroup, M44Identity(), B_EdgeEnds[0], B_EdgeEnds[1], {1,1,0});
#endif

			v3 ContactPoint;
			{
				v3 P0 = A_EdgeEnds[0];
				v3 P1 = A_EdgeEnds[1];
				v3 U = P1 - P0;
				v3 Q0 = B_EdgeEnds[0];
				v3 Q1 = B_EdgeEnds[1];
				v3 V = Q1 - Q0;
				v3 W0 = P0 - Q0;

				f32 a = Dot(U,U);
				f32 b = Dot(U,V);
				f32 c = Dot(V,V);
				f32 d = Dot(U,W0);
				f32 e = Dot(V,W0);
				f32 iDenom = SafeRatio0(1.0f, a*c - Square(b));

				f32 sc, tc;
				if(iDenom)
				{
					sc = (b*e-c*d) * iDenom;
					tc = (a*e-b*d) * iDenom;
				}
				else
				{
					//TODO(bjorn): Deal with parallel lines in a smarter way.
					sc = 0.5f;
					tc = 0.5f;
				}
				//TODO(bjorn):Decide on a reasonable 32-bit epsilon.
				f32 Epsilon = 0.001f;
				//Assert(IsWithinInclusive(sc, -Epsilon, 1.0f + Epsilon));
				//Assert(IsWithinInclusive(tc, -Epsilon, 1.0f + Epsilon));
				//TODO IMPORTANT(bjorn): This feels like a dumb bruteforce solution that might come back and bite me in future debugging. But since this code is temporary anyway and since the physics seems to behave stably on aggregate frames with me just clamping the variables I will leave this in for now.
				sc = Clamp(0.0f, sc, 1.0f);
				tc = Clamp(0.0f, tc, 1.0f);

				ContactPoint = ((P0 + U*sc) + (Q0 + V*tc))*0.5f;
			}

#if HANDMADE_INTERNAL
			{
				v3 APSize = 0.08f*v3{1,1,1};
				m44 T = ConstructTransform(ContactPoint, QuaternionIdentity(), APSize);
				PushWireCube(RenderGroup, T, {0.7f,0,0,1});
			}
#endif

			AddContact(Contacts, Entity_A, Entity_B, 
								 ContactPoint, 
								 Axis, 
								 SmallestOverlap,
								 (Entity_A->Body.Restitution + Entity_B->Body.Restitution)*0.5f,
								 Entity_A->Body.Friction * Entity_B->Body.Friction);
		}
	}
	if(A->CollisionShape != B->CollisionShape)
	{
		if(A->CollisionShape > B->CollisionShape) 
		{ 
			Swap(Entity_A, Entity_B, entity*); 
			Swap(A, B, body_primitive*); 
		}
		if(A->CollisionShape == CollisionShape_Sphere && 
			 B->CollisionShape == CollisionShape_AABB)
		{
			m44 AObjToUnscaledBObj = 
				(B->iTranUnscaled * (Entity_B->iTranUnscaled * (Entity_A->Tran * A->Tran)));
			v3 C = AObjToUnscaledBObj * v3{   0,0,0};
			v3 Q = AObjToUnscaledBObj * v3{0.5f,0,0};
			f32 R_sq = MagnitudeSquared(Q - C);
			f32 R = Magnitude(Q - C);

			if((Absolute(C.X) - R) > B->S.X*0.5f ||
				 (Absolute(C.Y) - R) > B->S.Y*0.5f ||
				 (Absolute(C.Z) - R) > B->S.Z*0.5f)
			{
				return;
			}
			v3 ClosestP = C;
			ClosestP.X = Clamp(B->S.X*-0.5f, ClosestP.X, B->S.X*0.5f);
			ClosestP.Y = Clamp(B->S.Y*-0.5f, ClosestP.Y, B->S.Y*0.5f);
			ClosestP.Z = Clamp(B->S.Z*-0.5f, ClosestP.Z, B->S.Z*0.5f);
			if(MagnitudeSquared(ClosestP - C) > R_sq)
			{
				return;
			}

			m44 UnscaledBObjToWorld = Entity_B->TranUnscaled * B->TranUnscaled;
			v3 ContactP = UnscaledBObjToWorld * ClosestP;
			f32 Penetration = R - Magnitude(ContactP - (UnscaledBObjToWorld * C));
			AddContact(Contacts, Entity_A, Entity_B, 
								 ContactP, 
								 Normalize(ContactP - Entity_A->P), 
								 Penetration,
								 (Entity_A->Body.Restitution + Entity_B->Body.Restitution)*0.5f,
								 Entity_A->Body.Friction * Entity_B->Body.Friction);
		}
	}
}

	internal_function contact_result
GenerateContacts(entity* A, entity* B
#if HANDMADE_INTERNAL
								 ,render_group* RenderGroup
#endif
								)
{
	Assert(A->HasBody && B->HasBody);
	contact_result Result = {};

	b32 BoundingVolumeCollides = false;
	{
		//TODO STUDY(bjorn): This just seems horrendously sub-optimal. Can I use
		//the radian info directly? What about the case where the primitive is
		//unscaled but the entity is?
		Assert(A->Body.PrimitiveCount >= 2);
		Assert(B->Body.PrimitiveCount >= 2);
		body_primitive* BoundingVolume_A = A->Body.Primitives + 0;
		body_primitive* BoundingVolume_B = B->Body.Primitives + 0;
		Assert(BoundingVolume_A->CollisionShape == CollisionShape_Sphere);
		Assert(BoundingVolume_B->CollisionShape == CollisionShape_Sphere);

		v3 C0 = A->Tran * (BoundingVolume_A->Tran * v3{   0,0,0});
		v3 Q0 = A->Tran * (BoundingVolume_A->Tran * v3{0.5f,0,0});
		v3 C1 = B->Tran * (BoundingVolume_B->Tran * v3{   0,0,0});
		v3 Q1 = B->Tran * (BoundingVolume_B->Tran * v3{0.5f,0,0});

		f32 m0 = Magnitude(C1 - C0);
		f32 m1 = Magnitude(Q0 - C0);
		f32 m2 = Magnitude(Q1 - C1);
		BoundingVolumeCollides = m0 <= (m1 + m2);
	}

	if(BoundingVolumeCollides)
	{
		for(u32 PrimitiveIndex_A = 1;
				PrimitiveIndex_A < A->Body.PrimitiveCount;
				PrimitiveIndex_A++)
		{
			body_primitive* Prim_A = A->Body.Primitives + PrimitiveIndex_A;
			for(u32 PrimitiveIndex_B = 1;
					PrimitiveIndex_B < B->Body.PrimitiveCount;
					PrimitiveIndex_B++)
			{
				body_primitive* Prim_B = B->Body.Primitives + PrimitiveIndex_B;
				GenerateContactsFromPrimitivePair(&Result, A, B, 
																					Prim_A, Prim_B
#if HANDMADE_INTERNAL
																					,RenderGroup
#endif
																					);
			}
		}
	}

	return Result;
}

#define COLLISION_H
#endif
