#if !defined(ENTITY_H)

#include"sim_region.h"

struct entity_pair
{
	sim_entity Obj;
	sim_entity Sub;
};

struct vertices
{
	u32 Count;
	v3 Verts[16];
};

internal_function vertices
GetEntityVertices(sim_entity* Entity)
{
	vertices Result = {};
	Result.Count = 4;

	v2 OrderOfCorners[4] = {{-0.5f,  0.5f}, 
													{ 0.5f,  0.5f}, 
													{ 0.5f, -0.5f}, 
													{-0.5f, -0.5f}};
	
	m22 Transform = M22(CW90M22() * Entity->R.XY, 
											Entity->R.XY);
	for(u32 VertexIndex = 0; 
			VertexIndex < Result.Count; 
			VertexIndex++)
	{
		Result.Verts[VertexIndex] = (Transform * 
																 Hadamard(OrderOfCorners[VertexIndex], Entity->Dim.XY) + 
																 Entity->P);
	}

	return Result;
}

inline sim_entity*
GetEntityOfType(entity_type Type, sim_entity* A, sim_entity* B)
{
	sim_entity* Result = 0;
	if(A->Type == Type)
	{
		Result = A;
	}
	if(B->Type == Type)
	{
		Result = B;
	}
	return Result;
}

inline sim_entity*
GetRemainingEntity(sim_entity* E, sim_entity* A, sim_entity* B)
{
	Assert(E == A || E == B);
	return E==A ? B:A;
}

#define ENTITY_H
#endif
