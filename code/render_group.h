#if !defined(RENDER_GROUP_H)

#include "primitive_shapes.h"
#include "math.h"

// NOTE(bjorn):
//
// All Color variables passed to the renderer are in NON-premultiplied alpha.
//

enum render_group_entry_type
{
	RenderGroupEntryType_render_entry_camera,
	RenderGroupEntryType_render_entry_clear_screen,
	RenderGroupEntryType_render_entry_vector,
	RenderGroupEntryType_render_entry_coordinate_system,
	RenderGroupEntryType_render_entry_wire_cube,
	RenderGroupEntryType_render_entry_blank_quad,
	RenderGroupEntryType_render_entry_quad,
	RenderGroupEntryType_render_entry_triangle_fill_flip,
	RenderGroupEntryType_render_entry_quad_bezier_fill_flip,
	RenderGroupEntryType_render_entry_sphere,
};

struct render_entry_header
{
  render_group_entry_type Type;
};

struct render_entry_clear_screen
{
  v4 Color;
};

struct render_entry_camera
{
  m44 WorldToCamera;

  f32 LensChamberSize;
  f32 NearClipPoint;
  f32 FarClipPoint;

	game_bitmap* RenderTarget;
  //TODO(Bjorn): FoV?
  f32 ScreenWidthInMeters;
  f32 ScreenHeightInMeters;
};

struct render_entry_vector
{
	v4 Color;
	v3 A;
	v3 B;
};
struct render_entry_coordinate_system
{
	m44 Tran;
};
struct render_entry_wire_cube
{
	v4 Color;
	m44 Tran;
};
struct render_entry_quad
{
	game_bitmap* Bitmap;
	v4 Color;
	m44 Tran;
  rectangle2 BitmapUVRect;
};
struct render_entry_triangle_fill_flip
{
	v3 Sta;
	v3 Mid;
	v3 End;
};
struct render_entry_quad_bezier_fill_flip
{
	v3 Sta;
	v3 Mid;
	v3 End;
};
struct render_entry_blank_quad
{
	v4 Color;
	m44 Tran;
};
struct render_entry_sphere
{
	v4 Color;
	m44 Tran;
};

struct render_group
{
  u32 MaxPushBufferSize;
  u32 PushBufferSize;
  u8* PushBufferBase;
};

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

#define RenderGroup_DefineEntryAndAdvanceByteOffset(type) \
  type * Entry = (type *)UnidentifiedEntry; \
  PushBufferByteOffset += sizeof(render_entry_header) + sizeof(type)

internal_function void
ClearRenderGroup(render_group* RenderGroup)
{
  RenderGroup->PushBufferSize = 0;
}

#define RENDER_GROUP_H
#endif
