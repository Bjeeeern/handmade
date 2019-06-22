#if !defined(RENDER_GROUP_H)

#include "resource.h"
#include "memory.h"
#include "renderer.h"

enum render_piece_type
{
	RenderPieceType_DimCube,
	RenderPieceType_Quad,
};

struct render_piece
{
	render_piece_type Type;

	loaded_bitmap* BMP;

	v4 Color;
	v3 Dim;

	v3 P;
};

struct render_group
{
	u32 PieceCount;
	render_piece RenderPieces[4096];
};

internal_function render_group*
AllocateRenderGroup(memory_arena* Arena)
{
	render_group* Result = PushStruct(Arena, render_group);

	Result->PieceCount = 0;

	return Result;
}

internal_function render_piece*
PushRenderPieceRaw(render_group* RenderGroup, v3 P)
{
	Assert(RenderGroup->PieceCount < ArrayCount(RenderGroup->RenderPieces));
	render_piece* Result = RenderGroup->RenderPieces + RenderGroup->PieceCount++;

	*Result = {};
	Result->P = P;

	return Result;
}

internal_function void
PushRenderPieceQuad(render_group* RenderGroup, v3 P, v3 Dim, v4 Color)
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, P);
	RenderPiece->Type = RenderPieceType_Quad;
	RenderPiece->BMP = 0;
	RenderPiece->Dim = Dim;
	RenderPiece->Color = Color;
}

internal_function void
PushRenderPieceQuad(render_group* RenderGroup, v3 P, loaded_bitmap* BMP, v4 Color = {1,1,1,1})
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, P);
	RenderPiece->Type = RenderPieceType_Quad;
	Assert(BMP);
	RenderPiece->BMP = BMP;
	RenderPiece->Color = Color;
}

internal_function void
PushRenderPieceWireFrame(render_group* RenderGroup, v3 P, v3 Dim, v4 Color = {0,0,1,1})
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, P);
	RenderPiece->Type = RenderPieceType_DimCube;
	RenderPiece->Color = Color;
	RenderPiece->Dim = Dim;
}

struct transform_result
{
	b32 Valid;
	v2 P;
	f32 f;
};

internal_function transform_result
TransformPoint(f32 dp, m33 RotMat, v3 P)
{
	transform_result Result = {};

	v3 Pos = (RotMat * P);
	f32 de = 20.0f;
	f32 s = -Pos.Z + (de+dp);

	Result.P = Pos.XY;
	Result.Valid = s >= 0;
	Result.f = de/s;

	return Result;
}

#define RENDER_GROUP_H
#endif
