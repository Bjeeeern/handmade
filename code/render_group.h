#if !defined(RENDER_GROUP_H)

#include "resource.h"
#include "memory.h"
#include "renderer.h"

enum render_piece_type
{
	RenderPieceType_LineSegment,
	RenderPieceType_DimCube,
	RenderPieceType_Quad,
	RenderPieceType_Sphere,
};

struct render_piece
{
	render_piece_type Type;

	game_bitmap* BMP;

	v4 Color;

	m44 Tran;

	v3 A;
	v3 B;

#if HANDMADE_INTERNAL
	b32 DEBUG_IsDebug;
#endif
};

struct render_group
{
	u32 PieceCount;
  f32 PixelsPerMeter;

  u32 MaxPushBufferSize;
  u32 PushBufferSize;
  u8* PushBufferBase;
};

//TODO(bjorn): Make rendering a separate call and actually figure out what kind
//of information that is needed.
// internal_function void
// RenderGroupToBuffer()
// {
// }

internal_function render_group*
AllocateRenderGroup(memory_arena* Arena, u32 MaxPushBufferSize, f32 PixelsPerMeter)
{
	render_group* Result = PushStruct(Arena, render_group);
	Result->PushBufferBase = PushArray(Arena, MaxPushBufferSize, u8);
  Result->PushBufferSize = 0;
  Result->MaxPushBufferSize = MaxPushBufferSize;

  Result->PieceCount = 0;
  Result->PixelsPerMeter = PixelsPerMeter;

	return Result;
}

internal_function void*
PushRenderElement(render_group* RenderGroup, u32 Size)
{
  void* Result = 0;

  if(RenderGroup->PushBufferSize + Size < RenderGroup->MaxPushBufferSize)
  {
    Result = RenderGroup->PushBufferBase + RenderGroup->PushBufferSize;
    RenderGroup->PushBufferSize += Size;
  }
  else
  {
    InvalidCodePath;
  }

  return Result;
}

internal_function render_piece*
PushRenderPieceRaw(render_group* RenderGroup, m44 T)
{
	render_piece* Result = (render_piece*)PushRenderElement(RenderGroup, sizeof(render_piece));
  RenderGroup->PieceCount++;

	*Result = {};
	Result->Tran = T;

	return Result;
}

internal_function void
PushRenderPieceQuad(render_group* RenderGroup, m44 T, v4 Color)
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, T);
	RenderPiece->Type = RenderPieceType_Quad;
	RenderPiece->BMP = 0;
	RenderPiece->Color = Color;
}

internal_function void
PushRenderPieceQuad(render_group* RenderGroup, m44 T, game_bitmap* BMP, v4 Color = {1,1,1,1})
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, T);
	RenderPiece->Type = RenderPieceType_Quad;
	Assert(BMP);
	RenderPiece->BMP = BMP;
	RenderPiece->Color = Color;
}

internal_function void
PushRenderPieceWireFrame(render_group* RenderGroup, m44 T, v4 Color = {0,0.4f,0.8f,1})
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, T);
	RenderPiece->Type = RenderPieceType_DimCube;
	RenderPiece->Color = Color;
#if HANDMADE_INTERNAL
	RenderPiece->DEBUG_IsDebug = true;
#endif
}
internal_function void
PushRenderPieceWireFrame(render_group* RenderGroup, rectangle3 Rect, v4 Color = {0,0,1,1})
{
	center_dim_v3_result CenterDim = RectToCenterDim(Rect);

	m44 T = ConstructTransform(CenterDim.Center, QuaternionIdentity(), CenterDim.Dim);
	PushRenderPieceWireFrame(RenderGroup, T, Color);
}
//TODO(bjorn): Rename LineSegment -> Vector
internal_function void
PushRenderPieceLineSegment(render_group* RenderGroup, m44 T, v3 A, v3 B, v4 Color = {0,0.4f,0.8f,1})
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, T);
	RenderPiece->Type = RenderPieceType_LineSegment;
	RenderPiece->A = A;
	RenderPiece->B = B;
	RenderPiece->Color = Color;
#if HANDMADE_INTERNAL
	RenderPiece->DEBUG_IsDebug = true;
#endif
}
internal_function void
PushRenderPieceSphere(render_group* RenderGroup, m44 T, v4 Color = {0,0.4f,0.8f,1})
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, T);
	RenderPiece->Type = RenderPieceType_Sphere;
	RenderPiece->Color = Color;
#if HANDMADE_INTERNAL
	RenderPiece->DEBUG_IsDebug = true;
#endif
}

struct transform_result
{
	b32 Valid;
	v2 P;
	f32 f;
};

internal_function transform_result
TransformPoint(f32 dp, m44 CamTrans, v3 P)
{
	transform_result Result = {};

	//TODO(bjorn): "de" should probably be grouped together with the other data defining the camera.
	v3 Pos = (CamTrans * P);
	f32 de = 20.0f;
	f32 s = -Pos.Z + (de+dp);

	Result.P = Pos.XY;
	Result.Valid = s > 0;
	Result.f = de/s;

	return Result;
}

#define RENDER_GROUP_H
#endif
