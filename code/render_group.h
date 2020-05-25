#if !defined(RENDER_GROUP_H)

#include "primitive_shapes.h"
#include "math.h"

// NOTE(bjorn):
//
// All Color variables passed to the renderer are in NON-premultiplied alpha.
//

struct camera_parameters
{
  f32 LensChamberSize;
  f32 NearClipPoint;
};

enum render_group_entry_type
{
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
  m44 WorldToCamera;

  camera_parameters CamParam;

  b32 ClearScreen;
  v4 ClearScreenColor;

  u32 MaxPushBufferSize;
  u32 PushBufferSize;
  u8* PushBufferBase;
};

#define RenderGroup_DefineEntryAndAdvanceByteOffset(type) \
  type * Entry = (type *)UnidentifiedEntry; \
  PushBufferByteOffset += sizeof(render_entry_header) + sizeof(type)

#define RENDER_GROUP_H
#endif
