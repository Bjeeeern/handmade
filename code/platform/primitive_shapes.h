#if !defined(PRIMITIVE_SHAPES_H)

struct aabb_verts_result
{
	u32 Count;
	v3 Verts[8];
};

v3 UnscaledAABB[8] = {{-0.5f,  0.5f, 0.5f}, 
											 { 0.5f,  0.5f, 0.5f}, 
											 { 0.5f, -0.5f, 0.5f}, 
											 {-0.5f, -0.5f, 0.5f},
											 {-0.5f,  0.5f,-0.5f}, 
											 { 0.5f,  0.5f,-0.5f}, 
											 { 0.5f, -0.5f,-0.5f}, 
											 {-0.5f, -0.5f,-0.5f}};

	internal_function aabb_verts_result
GetAABBVertices(m44* Tran)
{
	aabb_verts_result Result = {};
	Result.Count = 8;

	for(u32 CornerIndex = 0; 
			CornerIndex < ArrayCount(UnscaledAABB); 
			CornerIndex++)
	{
		v3 Corner = UnscaledAABB[CornerIndex];
		Result.Verts[CornerIndex] = *Tran * Corner;
	}

	return Result;
}

v3 UnscaledQuad[4] = {{-0.5f,  0.5f, 0.0f}, 
                      { 0.5f,  0.5f, 0.0f}, 
                      { 0.5f, -0.5f, 0.0f}, 
											{-0.5f, -0.5f, 0.0f}};
struct quad_verts_result
{
	u32 Count;
	v3 Verts[4];
};

	internal_function quad_verts_result
GetQuadVertices(m44* Tran)
{
	quad_verts_result Result = {};
	Result.Count = 8;

	for(u32 CornerIndex = 0; 
			CornerIndex < ArrayCount(UnscaledQuad); 
			CornerIndex++)
	{
		v3 Corner = UnscaledQuad[CornerIndex];
		Result.Verts[CornerIndex] = *Tran * Corner;
	}

	return Result;
}

#define PRIMITIVE_SHAPES_H
#endif
