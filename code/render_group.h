#if !defined(RENDER_GROUP_H)

#include "resource.h"
#include "memory.h"
#include "renderer.h"
#include "primitive_shapes.h"

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
};

struct render_group
{
  m44 CameraTransform;
  f32 PixelsPerMeter;
  f32 LensChamberSize;

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
}
internal_function void
PushRenderPieceSphere(render_group* RenderGroup, m44 T, v4 Color = {0,0.4f,0.8f,1})
{
	render_piece* RenderPiece = PushRenderPieceRaw(RenderGroup, T);
	RenderPiece->Type = RenderPieceType_Sphere;
	RenderPiece->Color = Color;
}

internal_function void
PushCamera(render_group* RenderGroup, m44 CameraTransform, f32 PixelsPerMeter, f32 LensChamberSize)
{
  RenderGroup->CameraTransform = CameraTransform;
  RenderGroup->PixelsPerMeter = PixelsPerMeter;
  RenderGroup->LensChamberSize = LensChamberSize;
}

struct pixel_pos_result
{
  b32 PointIsInView;
  f32 PerspectiveCorrection;
  v2 P;
};

internal_function pixel_pos_result
ProjectPointToScreen(render_group* RenderGroup, v2 ScreenCenter, m22 MeterToPixel, v3 Pos)
{
	pixel_pos_result Result = {};

  v3 PosRelativeCameraLens = RenderGroup->CameraTransform * Pos;
  //TODO(bjorn): Why is this negative? I have a feeling it should not be and
  //that Im making a misstake somewhere else.
  f32 DistanceFromCameraLens = -PosRelativeCameraLens.Z;

  f32 PerspectiveCorrection = 
    RenderGroup->LensChamberSize / (DistanceFromCameraLens + RenderGroup->LensChamberSize);

  //TODO(bjorn): Add clipping plane parameters.
  b32 PointIsInView = (DistanceFromCameraLens + RenderGroup->LensChamberSize) > 0;
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
ProjectSegmentToScreen(render_group* RenderGroup, v2 ScreenCenter, m22 MeterToPixel, v3 A, v3 B)
{
  pixel_line_segment_result Result = {};

  pixel_pos_result PixPosA = ProjectPointToScreen(RenderGroup, ScreenCenter, MeterToPixel, A);
  pixel_pos_result PixPosB = ProjectPointToScreen(RenderGroup, ScreenCenter, MeterToPixel, B);

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

#if 0
//TODO(bjorn): Refactor this into something that looks more sane.
  internal_function void
TransformLineToScreenSpaceAndRender(game_bitmap* Buffer, render_piece* RenderPiece,
                                    f32 CamDist, m44 CamTrans, v3 V0, v3 V1,
                                    v2 ScreenCenter, m22 GameSpaceToScreenSpace,
                                    v3 RGB)
{
  transform_result Tran0 = WorldToScreen(CamDist, CamTrans, V0);
  transform_result Tran1 = WorldToScreen(CamDist, CamTrans, V1);

  if(Tran0.Valid && Tran1.Valid)
  {
    v2 PixelV0 = ScreenCenter + (GameSpaceToScreenSpace * Tran0.P) * Tran0.f;
    v2 PixelV1 = ScreenCenter + (GameSpaceToScreenSpace * Tran1.P) * Tran1.f;
    DrawLine(Buffer, PixelV0, PixelV1, RGB);
  }
};
#endif

  internal_function void
RenderGroupToOutput(render_group* RenderGroup, game_bitmap* Output)
{
  //TODO(bjorn): Make these one m33 transform.
  v2 ScreenCenter = v2{(f32)Output->Width, (f32)Output->Height} * 0.5f;
  m22 MeterToPixel = 
  {RenderGroup->PixelsPerMeter, 0                         ,
    0                         ,-RenderGroup->PixelsPerMeter};

  for(u32 PushBufferByteOffset = 0;
      PushBufferByteOffset < RenderGroup->PushBufferSize;
      PushBufferByteOffset += sizeof(render_piece))
  {
    render_piece* RenderPiece = (render_piece*)(RenderGroup->PushBufferBase + PushBufferByteOffset);

    if(RenderPiece->Type == RenderPieceType_Quad)
    {
      v3 Pos = RenderPiece->Tran * v3{0,0,0};
      pixel_pos_result PixPos = ProjectPointToScreen(RenderGroup, ScreenCenter, MeterToPixel, Pos);

      if(PixPos.PointIsInView)
      {
        if(RenderPiece->BMP)
        {
          DrawBitmap(Output, 
                     RenderPiece->BMP, 
                     PixPos.P - RenderPiece->BMP->Alignment * PixPos.PerspectiveCorrection, 
                     RenderPiece->BMP->Dim * PixPos.PerspectiveCorrection, 
                     RenderPiece->Color.A);
        }
        else
        {
          //TODO(bjorn): Think about how and when in the pipeline to render the hit-points.
#if 0
          v2 PixelDim = RenderPiece->Dim.XY * (PixelsPerMeter * f);
          rectangle2 Rect = RectCenterDim(PixelPos, PixelDim);

          DrawRectangle(Output, Rect, RenderPiece->Color.RGB);
#endif
        }
      }
    }
    if(RenderPiece->Type == RenderPieceType_DimCube)
    {
      aabb_verts_result AABB = GetAABBVertices(&RenderPiece->Tran);

      for(u32 VertIndex = 0; 
          VertIndex < 4; 
          VertIndex++)
      {
        v3 V0 = AABB.Verts[(VertIndex+0)%4];
        v3 V1 = AABB.Verts[(VertIndex+1)%4];
        pixel_line_segment_result LineSegment = 
          ProjectSegmentToScreen(RenderGroup, ScreenCenter, MeterToPixel, V0, V1);
        if(LineSegment.PartOfSegmentInView)
        {
          DrawLine(Output, LineSegment.A, LineSegment.B, RenderPiece->Color.RGB);
        }
      }
      for(u32 VertIndex = 0; 
          VertIndex < 4; 
          VertIndex++)
      {
        v3 V0 = AABB.Verts[(VertIndex+0)%4 + 4];
        v3 V1 = AABB.Verts[(VertIndex+1)%4 + 4];
        pixel_line_segment_result LineSegment = 
          ProjectSegmentToScreen(RenderGroup, ScreenCenter, MeterToPixel, V0, V1);
        if(LineSegment.PartOfSegmentInView)
        {
          DrawLine(Output, LineSegment.A, LineSegment.B, RenderPiece->Color.RGB);
        }
      }
      for(u32 VertIndex = 0; 
          VertIndex < 4; 
          VertIndex++)
      {
        v3 V0 = AABB.Verts[VertIndex];
        v3 V1 = AABB.Verts[VertIndex + 4];
        pixel_line_segment_result LineSegment = 
          ProjectSegmentToScreen(RenderGroup, ScreenCenter, MeterToPixel, V0, V1);
        if(LineSegment.PartOfSegmentInView)
        {
          DrawLine(Output, LineSegment.A, LineSegment.B, RenderPiece->Color.RGB);
        }
      }
    }
    if(RenderPiece->Type == RenderPieceType_LineSegment)
    {
      v3 V0 = RenderPiece->Tran * RenderPiece->A;
      v3 V1 = RenderPiece->Tran * RenderPiece->B;

      pixel_line_segment_result LineSegment = 
        ProjectSegmentToScreen(RenderGroup, ScreenCenter, MeterToPixel, V0, V1);
      if(LineSegment.PartOfSegmentInView)
      {
        DrawLine(Output, LineSegment.A, LineSegment.B, RenderPiece->Color.RGB);
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
          ProjectSegmentToScreen(RenderGroup, ScreenCenter, MeterToPixel, V0, V1);
        if(LineSegment.PartOfSegmentInView)
        {
          DrawLine(Output, LineSegment.A, LineSegment.B, RenderPiece->Color.RGB);
        }
      }
      for(int VertIndex = 0; 
          VertIndex < 4; 
          VertIndex++)
      {
        V0 = Verts[VertIndex];
        V1 = Verts[4];
        LineSegment = 
          ProjectSegmentToScreen(RenderGroup, ScreenCenter, MeterToPixel, V0, V1);
        if(LineSegment.PartOfSegmentInView)
        {
          DrawLine(Output, LineSegment.A, LineSegment.B, RenderPiece->Color.RGB);
        }
      }
    }
    if(RenderPiece->Type == RenderPieceType_Sphere)
    {
      v3 P0 = RenderPiece->Tran * v3{0,0,0};
      v3 P1 = RenderPiece->Tran * v3{0.5f,0,0};

      pixel_pos_result PixPos = ProjectPointToScreen(RenderGroup, ScreenCenter, MeterToPixel, P0);
      if(PixPos.PointIsInView)
      {
        f32 PixR = RenderGroup->PixelsPerMeter * Magnitude(P1 - P0) * PixPos.PerspectiveCorrection;

        DrawCircle(Output, PixPos.P, PixR, RenderPiece->Color);
      }
    }
  }
}


#define RENDER_GROUP_H
#endif
