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
	//TODO
	//f32 Friction;
};

struct contact_result
{
	u32 Count;
	contact E[16];
};

internal_function void 
AddContact(contact_result* Contacts, entity* A, entity* B, 
					 v3 P, v3 N, f32 PenetrationDepth, f32 Restitution)
{
	Assert(Contacts->Count < ArrayCount(Contacts->E));
	if(Contacts->Count < ArrayCount(Contacts->E))
	{
		contact* Contact = Contacts->E + Contacts->Count++;
		*Contact = {A, B, P, N, PenetrationDepth, Restitution};
	}
}

	internal_function void
GenerateContactsFromPrimitivePair(contact_result* Contacts, 
																	entity* Entity_A, entity* Entity_B,
																	m44* T_A, m44* T_B,
																	body_primitive* A, body_primitive* B)
{
	if(A->CollisionShape == CollisionShape_Sphere && 
		 B->CollisionShape == CollisionShape_Sphere)
	{
		v3 C0 = *T_A * (A->Tran * v3{   0,0,0});
		v3 Q0 = *T_A * (A->Tran * v3{0.5f,0,0});
		v3 C1 = *T_B * (B->Tran * v3{   0,0,0});
		v3 Q1 = *T_B * (B->Tran * v3{0.5f,0,0});

		b32 Overlaps = Magnitude(C1-C0) <= (Magnitude(Q0-C0) + Magnitude(Q1-C1));
		if(Overlaps)
		{
			AddContact(Contacts, Entity_A, Entity_B, 
								 (C0+C1)*0.5f, 
								 Normalize(C1-C0), 
								 (Magnitude(Q0-C0)+Magnitude(Q1-C1)) - Magnitude(C1-C0),
								 (Entity_A->Body.Restitution + Entity_B->Body.Restitution)*0.5f);
		}
	}
	if(A->CollisionShape == CollisionShape_AABB && 
		 B->CollisionShape == CollisionShape_AABB)
	{
	}
	if(A->CollisionShape != B->CollisionShape)
	{
		if(A->CollisionShape > B->CollisionShape) 
		{ 
			Swap(Entity_A, Entity_B, entity*); 
			Swap(T_A, T_B, m44*); 
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
								 (Entity_A->Body.Restitution + Entity_B->Body.Restitution)*0.5f);
		}
	}
}

	internal_function contact_result
GenerateContacts(entity* A, entity* B)
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
																					&A->Tran, &B->Tran, 
																					Prim_A, Prim_B);
			}
		}
	}

	return Result;
}

#define COLLISION_H
#endif
