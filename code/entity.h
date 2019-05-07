#if !defined(ENTITY_H)

#include"sim_entity"
#include"stored_entity"

internal_function entity
GetEntityFromSimEntity(entities* Entities, sim_entity* SimEntity)
{
	entity Result = {};

	Assert(SimEntity && SimEntity->StoredIndex);
	Result.Sim = SimEntity;
	Result.Stored = GetStoredEntityByIndex(Entities, SimEntity->StoredIndex);
}

struct entity_pair
{
	entity Obj;
	entity Sub;
};

struct vertices
{
	u32 Count;
	v3 Verts[16];
};

internal_function vertices
GetEntityVertices(entity* Entity)
{
	vertices Result = {};
	Result.Count = 4;

	v2 OrderOfCorners[4] = {{-0.5f,  0.5f}, 
													{ 0.5f,  0.5f}, 
													{ 0.5f, -0.5f}, 
													{-0.5f, -0.5f}};
	
	m22 Transform = M22(CW90M22() * Entity->Low->R.XY, 
											Entity->Low->R.XY);
	for(u32 VertexIndex = 0; 
			VertexIndex < Result.Count; 
			VertexIndex++)
	{
		Result.Verts[VertexIndex] = (Transform * 
																 Hadamard(OrderOfCorners[VertexIndex], Entity->Low->Dim.XY) + 
																 Entity->High->P);
	}

	return Result;
}

inline entity*
GetEntityOfType(entity_type Type, entity* A, entity* B)
{
	entity* Result = 0;
	if(A->Low->Type == Type)
	{
		Result = A;
	}
	if(B->Low->Type == Type)
	{
		Result = B;
	}
	return Result;
}

inline entity*
GetRemainingEntity(entity* E, entity* A, entity* B)
{
	Assert(E == A || E == B);
	return E==A ? B:A;
}

#define ENTITY_H
#endif
