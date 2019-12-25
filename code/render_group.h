#if !defined(RENDER_GROUP_H)

#include "resource.h"
#include "memory.h"
#include "renderer.h"
#include "primitive_shapes.h"

enum render_group_entry_type
{
	RenderGroupEntryType_render_entry_vector,
	RenderGroupEntryType_render_entry_coordinate_system,
	RenderGroupEntryType_render_entry_wire_cube,
	RenderGroupEntryType_render_entry_blank_quad,
	RenderGroupEntryType_render_entry_quad,
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
  m44 CameraTransform;
  f32 PixelsPerMeter;
  f32 LensChamberSize;
  f32 NearClipPoint;

  b32 ClearScreen;
  v4 ClearScreenColor;

  u32 MaxPushBufferSize;
  u32 PushBufferSize;
  u8* PushBufferBase;
};

internal_function render_group*
AllocateRenderGroup(memory_arena* Arena, u32 MaxPushBufferSize)
{
	render_group* Result = PushStruct(Arena, render_group);

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

    Result = RenderGroup->PushBufferBase + RenderGroup->PushBufferSize + sizeof(render_entry_header);

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
PushQuad(render_group* RenderGroup, m44 T, game_bitmap* Bitmap, v4 Color = {1,1,1,1})
{
	render_entry_quad* Entry = PushRenderElement(RenderGroup, render_entry_quad);
	Entry->Tran = T;
	Entry->Bitmap = Bitmap;
	Entry->Color = Color;
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
SetCamera(render_group* RenderGroup, m44 CameraTransform, f32 PixelsPerMeter, f32 LensChamberSize,
          f32 NearClipPoint)
{
  RenderGroup->CameraTransform = CameraTransform;
  RenderGroup->PixelsPerMeter = PixelsPerMeter;
  RenderGroup->LensChamberSize = LensChamberSize;
  RenderGroup->NearClipPoint = NearClipPoint;
}

internal_function void
ClearScreen(render_group* RenderGroup, v4 Color)
{
  RenderGroup->ClearScreen = true;
  RenderGroup->ClearScreenColor = Color;
}

struct pixel_pos_result
{
  b32 PointIsInView;
  f32 PerspectiveCorrection;
  v2 P;
};

internal_function pixel_pos_result
ProjectPointToScreen(m44 CameraTransform, f32 LensChamberSize, 
                     v2 ScreenCenter, m22 MeterToPixel, v3 Pos)
{
	pixel_pos_result Result = {};

  v3 PosRelativeCameraLens = CameraTransform * Pos;
  //NOTE(bjorn): The untransformed camera has the lens located at the origin
  //and is looking along the negative z-axis in a right-hand coordinate system. 
  f32 DistanceFromCameraLens = -PosRelativeCameraLens.Z;

  f32 PerspectiveCorrection = 
    LensChamberSize / (DistanceFromCameraLens + LensChamberSize);

  //TODO(bjorn): Add clipping plane parameters.
  b32 PointIsInView = (DistanceFromCameraLens + LensChamberSize) > 0;
  if(PointIsInView)
  {
    Result.PointIsInView = PointIsInView;
    Result.PerspectiveCorrection = PerspectiveCorrection;
    Result.P = ScreenCenter + (MeterToPixel * PosRelativeCameraLens.XY) * PerspectiveCorrection;
  }

  return Result;
}

struct pixel_line_segment_result
{
  b32 PartOfSegmentInView;
  v2 A;
  v2 B;
};
//TODO(bjorn): Do a unique solution to this that interpolates the points when nessecary.
  internal_function pixel_line_segment_result
ProjectSegmentToScreen(m44 CameraTransform, f32 LensChamberSize, 
                       v2 ScreenCenter, m22 MeterToPixel, v3 A, v3 B)
{
  pixel_line_segment_result Result = {};

  pixel_pos_result PixPosA = 
    ProjectPointToScreen(CameraTransform, LensChamberSize, ScreenCenter, MeterToPixel, A);
  pixel_pos_result PixPosB = 
    ProjectPointToScreen(CameraTransform, LensChamberSize, ScreenCenter, MeterToPixel, B);

  if(PixPosA.PointIsInView && PixPosB.PointIsInView)
  {
    Result.PartOfSegmentInView = true;
    Result.A = PixPosA.P;
    Result.B = PixPosB.P;
  }
  else if (PixPosA.PointIsInView || PixPosB.PointIsInView)
  {
    Result.PartOfSegmentInView = false;

    pixel_pos_result InView = PixPosA.PointIsInView ? PixPosA : PixPosB;
    pixel_pos_result OutOfView = PixPosB.PointIsInView ? PixPosA : PixPosB;

    Result.A = InView.P;
    //Result.B = 
  }

  return Result;
}

internal_function void
DrawVector(render_group* RenderGroup, v2 ScreenCenter, m22 MeterToPixel, game_bitmap* Output,
           v3 V0, v3 V1, v3 Color)
{
  pixel_line_segment_result LineSegment = 
    ProjectSegmentToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                           ScreenCenter, MeterToPixel, V0, V1);
  if(LineSegment.PartOfSegmentInView)
  {
    DrawLine(Output, LineSegment.A, LineSegment.B, Color);
  }

  v3 n = Normalize(V1-V0);

  //TODO(bjorn): Implement a good GetComplimentaryAxes() fuction.
  v3 t0 = {};
  v3 t1 = {};
  {
    f32 e = 0.01f;
    if(IsWithin(n.X, -e, e) && 
       IsWithin(n.Y, -e, e))
    {
      t0 = {1,0,0};
    }
    else
    {
      t0 = n + v3{0,0,100};
    }
    t1 = Normalize(Cross(n, t0));
    t0 = Cross(n, t1);
  }

  f32 Scale = 0.2f;
  v3 Verts[5];
  Verts[0] = V1 - n*Scale + t0 * 0.25f * Scale + t1 * 0.25f * Scale;
  Verts[1] = V1 - n*Scale - t0 * 0.25f * Scale + t1 * 0.25f * Scale;
  Verts[2] = V1 - n*Scale - t0 * 0.25f * Scale - t1 * 0.25f * Scale;
  Verts[3] = V1 - n*Scale + t0 * 0.25f * Scale - t1 * 0.25f * Scale;
  Verts[4] = V1;
  for(int VertIndex = 0; 
      VertIndex < 4; 
      VertIndex++)
  {
    V0 = Verts[(VertIndex+0)%4];
    V1 = Verts[(VertIndex+1)%4];
    LineSegment = 
      ProjectSegmentToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                             ScreenCenter, MeterToPixel, V0, V1);
    if(LineSegment.PartOfSegmentInView)
    {
      DrawLine(Output, LineSegment.A, LineSegment.B, Color);
    }
  }
  for(int VertIndex = 0; 
      VertIndex < 4; 
      VertIndex++)
  {
    V0 = Verts[VertIndex];
    V1 = Verts[4];
    LineSegment = 
      ProjectSegmentToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                             ScreenCenter, MeterToPixel, V0, V1);
    if(LineSegment.PartOfSegmentInView)
    {
      DrawLine(Output, LineSegment.A, LineSegment.B, Color);
    }
  }
}

#define RenderGroup_DefineEntryAndAdvanceByteOffset(type) \
  type * Entry = (type *)UnidentifiedEntry; \
  PushBufferByteOffset += sizeof(render_entry_header) + sizeof(type)

  internal_function void
RenderGroupToOutput(render_group* RenderGroup, game_bitmap* Output)
{
  //TODO(bjorn): Make these one m33 transform.
  v2 ScreenCenter = v2{(f32)Output->Width, (f32)Output->Height} * 0.5f;
  m22 MeterToPixel = 
  {RenderGroup->PixelsPerMeter, 0                         ,
    0                         ,-RenderGroup->PixelsPerMeter};

  if(RenderGroup->ClearScreen)
  {
    DrawRectangle(Output, RectMinMax(v2{0.0f, 0.0f}, Output->Dim), 
                  RenderGroup->ClearScreenColor.RGB);
  }

  for(u32 PushBufferByteOffset = 0;
      PushBufferByteOffset < RenderGroup->PushBufferSize;
      )
  {
    render_entry_header* Header = 
      (render_entry_header*)(RenderGroup->PushBufferBase + PushBufferByteOffset);
    u8* UnidentifiedEntry = 
      RenderGroup->PushBufferBase + PushBufferByteOffset + sizeof(render_entry_header);

    switch(Header->Type)
    {
      case RenderGroupEntryType_render_entry_vector
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_vector);

          DrawVector(RenderGroup, ScreenCenter, MeterToPixel, Output,
                     Entry->A, Entry->B, Entry->Color.RGB);
        } break;
      case RenderGroupEntryType_render_entry_coordinate_system
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_coordinate_system);

          DrawVector(RenderGroup, ScreenCenter, MeterToPixel, Output,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{1,0,0}, v3{1,0,0});
          DrawVector(RenderGroup, ScreenCenter, MeterToPixel, Output,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{0,1,0}, v3{0,1,0});
          DrawVector(RenderGroup, ScreenCenter, MeterToPixel, Output,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{0,0,1}, v3{0,0,1});
        } break;
      case RenderGroupEntryType_render_entry_wire_cube
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_wire_cube);

          aabb_verts_result AABB = GetAABBVertices(&Entry->Tran);

          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[(VertIndex+0)%4];
            v3 V1 = AABB.Verts[(VertIndex+1)%4];
            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                                     ScreenCenter, MeterToPixel, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(Output, LineSegment.A, LineSegment.B, Entry->Color.RGB);
            }
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[(VertIndex+0)%4 + 4];
            v3 V1 = AABB.Verts[(VertIndex+1)%4 + 4];
            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                                     ScreenCenter, MeterToPixel, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(Output, LineSegment.A, LineSegment.B, Entry->Color.RGB);
            }
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[VertIndex];
            v3 V1 = AABB.Verts[VertIndex + 4];
            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                                     ScreenCenter, MeterToPixel, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(Output, LineSegment.A, LineSegment.B, Entry->Color.RGB);
            }
          }
        } break;
      case RenderGroupEntryType_render_entry_blank_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_blank_quad);

          //TODO(bjorn): Think about how and when in the pipeline to render the hit-points.
#if 0
          v2 PixelDim = Entry->Dim.XY * (PixelsPerMeter * f);
          rectangle2 Rect = RectCenterDim(PixelPos, PixelDim);

          DrawRectangle(Output, Rect, Entry->Color.RGB);
#endif
        } break;
      case RenderGroupEntryType_render_entry_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad);

          quad_verts_result Quad = GetQuadVertices(&Entry->Tran);

          v2 PixVerts[4] = {};
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = Quad.Verts[VertIndex];
            v3 V1 = Quad.Verts[(VertIndex + 1)%4];

            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                                     ScreenCenter, MeterToPixel, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              PixVerts[VertIndex] = LineSegment.A;
            }
          }

          DrawTriangleSlowly(Output, 
                             RenderGroup->LensChamberSize, ScreenCenter, MeterToPixel, 
                             RenderGroup->NearClipPoint,
                             RenderGroup->CameraTransform * Quad.Verts[0], 
                             RenderGroup->CameraTransform * Quad.Verts[1], 
                             RenderGroup->CameraTransform * Quad.Verts[2], 
                             {0,0},
                             {1,0},
                             {1,1},
                             Entry->Bitmap,
                             Entry->Color);
          DrawTriangleSlowly(Output, 
                             RenderGroup->LensChamberSize, ScreenCenter, MeterToPixel,
                             RenderGroup->NearClipPoint,
                             RenderGroup->CameraTransform * Quad.Verts[0], 
                             RenderGroup->CameraTransform * Quad.Verts[2], 
                             RenderGroup->CameraTransform * Quad.Verts[3], 
                             {0,0},
                             {1,1},
                             {0,1},
                             Entry->Bitmap,
                             Entry->Color);

#if 0
          DrawLine(Output, PixVerts[0], PixVerts[2], {1.0f, 0.25f, 1.0f});
          DrawCircle(Output, (PixVerts[0] + PixVerts[2]) * 0.5f, 3.0f, {1.0f, 1.0f, 0.0f, 1.0f});

          pixel_pos_result PixPos = 
            ProjectPointToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                                 ScreenCenter, MeterToPixel, (Quad.Verts[0]+Quad.Verts[2])*0.5f);
          if(PixPos.PointIsInView)
          {
            DrawCircle(Output, PixPos.P, 3.0f, {0.0f, 1.0f, 1.0f, 1.0f});
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            DrawLine(Output, PixVerts[VertIndex], PixVerts[(VertIndex + 1)%4], 
                     {1.0f, VertIndex*0.25f, 1.0f});
          }
#endif
        } break;
      case RenderGroupEntryType_render_entry_sphere
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_sphere);

          v3 P0 = Entry->Tran * v3{0,0,0};
          v3 P1 = Entry->Tran * v3{0.5f,0,0};

          pixel_pos_result PixPos = 
            ProjectPointToScreen(RenderGroup->CameraTransform, RenderGroup->LensChamberSize, 
                                 ScreenCenter, MeterToPixel, P0);
          if(PixPos.PointIsInView)
          {
            f32 PixR = 
              RenderGroup->PixelsPerMeter * Magnitude(P1 - P0) * PixPos.PerspectiveCorrection;

            DrawCircle(Output, PixPos.P, PixR, Entry->Color);
          }
        } break;
      InvalidDefaultCase;
    }
  }
}


#define RENDER_GROUP_H
#endif
