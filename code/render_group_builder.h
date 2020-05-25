#if !defined(RENDER_GROUP_BUILDER_H)

#include "render_group.h"
#include "memory.h"
#include "asset.h"

internal_function render_group*
AllocateRenderGroup(memory_arena* Arena, u32 MaxPushBufferSize)
{
	render_group* Result = PushStruct(Arena, render_group);
  *Result = {};

	Result->PushBufferBase = PushArray(Arena, MaxPushBufferSize, u8);
  Result->PushBufferSize = 0;
  Result->MaxPushBufferSize = MaxPushBufferSize;

	return Result;
}

#define PushRenderElement(Group, type) (type*)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type)
internal_function void*
PushRenderElement_(render_group* RenderGroup, u32 Size, render_group_entry_type Type)
{
  void* Result = 0;

  Size += sizeof(render_entry_header);

  if(RenderGroup->PushBufferSize + Size < RenderGroup->MaxPushBufferSize)
  {
    render_entry_header* Header = 
      (render_entry_header*)(RenderGroup->PushBufferBase + RenderGroup->PushBufferSize);
    Header->Type = Type;

    Result = (RenderGroup->PushBufferBase + 
              RenderGroup->PushBufferSize + 
              sizeof(render_entry_header));

    RenderGroup->PushBufferSize += Size;
  }
  else
  {
    InvalidCodePath;
  }

  return Result;
}

internal_function void
PushBlankQuad(render_group* RenderGroup, m44 T, v4 Color)
{
	render_entry_blank_quad* Entry = PushRenderElement(RenderGroup, render_entry_blank_quad);
	Entry->Tran = T;
	Entry->Color = Color;
}

internal_function void
PushQuad(render_group* RenderGroup, m44 T, game_bitmap* Bitmap, 
         rectangle2 BitmapUVRect = RectMinDim({0,0},{1,1}), v4 Color = {1,1,1,1})
{
	render_entry_quad* Entry = PushRenderElement(RenderGroup, render_entry_quad);
	Entry->Tran = T;
	Entry->Bitmap = Bitmap;
	Entry->Color = Color;
	Entry->BitmapUVRect = BitmapUVRect;
}

internal_function void
PushQuad(render_group* RenderGroup, m44 T, game_assets* Assets, game_asset_id ID, 
         rectangle2 BitmapUVRect = RectMinDim({0,0},{1,1}), v4 Color = {1,1,1,1})
{
  game_bitmap* Bitmap = GetBitmap(Assets, ID);
  if(Bitmap)
  {
    PushQuad(RenderGroup, T, Bitmap, BitmapUVRect, Color);
  }
}

internal_function void
PushTriangleFillFlip(render_group* RenderGroup, v2 Sta, v2 Mid, v2 End)
{
	render_entry_triangle_fill_flip* Entry = 
    PushRenderElement(RenderGroup, render_entry_triangle_fill_flip);
	Entry->Sta = ToV3(Sta, 0.0f);
	Entry->Mid = ToV3(Mid, 0.0f);
	Entry->End = ToV3(End, 0.0f);
}

internal_function void
PushQuadBezierFlip(render_group* RenderGroup, v2 Sta, v2 Mid, v2 End)
{
	render_entry_quad_bezier_fill_flip* Entry = 
    PushRenderElement(RenderGroup, render_entry_quad_bezier_fill_flip);
	Entry->Sta = ToV3(Sta, 0.0f);
	Entry->Mid = ToV3(Mid, 0.0f);
	Entry->End = ToV3(End, 0.0f);
}

internal_function void
PushWireCube(render_group* RenderGroup, m44 T, v4 Color = {0,0.4f,0.8f,1})
{
	render_entry_wire_cube* Entry = PushRenderElement(RenderGroup, render_entry_wire_cube);
	Entry->Tran = T;
	Entry->Color = Color;
}
internal_function void
PushWireCube(render_group* RenderGroup, rectangle3 Rect, v4 Color = {0,0,1,1})
{
	center_dim_v3_result CenterDim = RectToCenterDim(Rect);
	m44 T = ConstructTransform(CenterDim.Center, QuaternionIdentity(), CenterDim.Dim);

	PushWireCube(RenderGroup, T, Color);
}

internal_function void
PushVector(render_group* RenderGroup, m44 T, v3 A, v3 B, v4 Color = {0,0.4f,0.8f,1})
{
	render_entry_vector* Entry = PushRenderElement(RenderGroup, render_entry_vector);
	Entry->A = T * A;
	Entry->B = T * B;
	Entry->Color = Color;
}

internal_function void
PushCoordinateSystem(render_group* RenderGroup, m44 T)
{
	render_entry_coordinate_system* Entry = 
    PushRenderElement(RenderGroup, render_entry_coordinate_system);
	Entry->Tran = T;
}

internal_function void
PushSphere(render_group* RenderGroup, m44 T, v4 Color = {0,0.4f,0.8f,1})
{
	render_entry_sphere* Entry = PushRenderElement(RenderGroup, render_entry_sphere);
	Entry->Tran = T;
	Entry->Color = Color;
}

//TODO(Bjorn): SetCamera(f32 HeightOfScreenInGameMeters, f32 YFoV)
internal_function void
SetCamera(render_group* RenderGroup, m44 WorldToCamera, f32 LensChamberSize, 
          f32 NearClipPoint = 0, f32 FarClipPoint = 100.0f)
{
  RenderGroup->WorldToCamera = WorldToCamera;

  camera_parameters* CamParam = &RenderGroup->CamParam;

  Assert(NearClipPoint <= LensChamberSize);
  Assert(NearClipPoint <= FarClipPoint);
  CamParam->LensChamberSize = LensChamberSize;
  CamParam->NearClipPoint = NearClipPoint;
  CamParam->FarClipPoint = FarClipPoint;
}

internal_function void
ClearScreen(render_group* RenderGroup, v4 Color)
{
  RenderGroup->ClearScreen = true;
  RenderGroup->ClearScreenColor = Color;
}

internal_function void
ClearRenderGroup(render_group* RenderGroup)
{
  RenderGroup->ClearScreen = false;
  RenderGroup->PushBufferSize = 0;
}

#define RENDER_GROUP_BUILDER_H
#endif
