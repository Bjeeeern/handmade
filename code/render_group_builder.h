#if !defined(RENDER_GROUP_BUILDER_H)

#include "render_group.h"
#include "asset.h"


internal_function void
PushBlankQuad(render_group* RenderGroup, m44 T, v4 Color)
{
	render_entry_blank_quad* Entry = PushRenderElement(RenderGroup, render_entry_blank_quad);
	Entry->Tran = T;
	Entry->Color = Color;
}

internal_function void
PushQuad(render_group* RenderGroup, m44 T, game_assets* Assets, game_asset_id ID, 
         rectangle2 BitmapUVRect = RectMinDim({0,0},{1,1}), v4 Color = {1,1,1,1})
{
	render_entry_quad* Entry = PushRenderElement(RenderGroup, render_entry_quad);
	Entry->Tran = T;
	Entry->AssetID = ID;
	Entry->Color = Color;
	Entry->BitmapUVRect = BitmapUVRect;
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

internal_function void
PushCamera(render_group* RenderGroup, game_assets* Assets, game_asset_id ID, f32 ScreenHeightInMeters, 
           m44 WorldToCamera, f32 LensChamberSize, 
           f32 NearClipPoint = 0, f32 FarClipPoint = 100.0f)
{
  game_bitmap* Target = GetBitmap(Assets, ID);
  Assert(Target);

	render_entry_camera* Entry = PushRenderElement(RenderGroup, render_entry_camera);

  Entry->WorldToCamera = WorldToCamera;

  Assert(NearClipPoint <= LensChamberSize);
  Assert(NearClipPoint <= FarClipPoint);
  Entry->LensChamberSize = LensChamberSize;
  Entry->NearClipPoint = NearClipPoint;
  Entry->FarClipPoint = FarClipPoint;

  Entry->ScreenHeightInMeters = ScreenHeightInMeters;
  Entry->ScreenWidthInMeters = Target->WidthOverHeight * ScreenHeightInMeters;
}

internal_function void
ClearScreen(render_group* RenderGroup, v4 Color)
{
	render_entry_clear_screen* Entry = PushRenderElement(RenderGroup, render_entry_clear_screen);

  Entry->Color = Color;
}

#define RENDER_GROUP_BUILDER_H
#endif
