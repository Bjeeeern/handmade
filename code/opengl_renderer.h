#if !defined(OPENGL_RENDERER_H)

#include "platform.h"
#include "render_group.h"
#include <gl/gl.h>

RENDER_GROUP_TO_OUTPUT(OpenGLRenderGroupToOutput)
{
#if 0
  output_target_screen_variables ScreenVars = {};
  {
    Assert(ScreenHeightInMeters > 0);
    f32 PixelsPerMeter = (f32)OutputTarget->Height / ScreenHeightInMeters;
    ScreenVars.MeterToPixel = 
    { PixelsPerMeter,              0,
      0,              PixelsPerMeter};
    ScreenVars.PixelToMeter = 
    { 1.0f/PixelsPerMeter,                   0,
      0,                  1.0f/PixelsPerMeter};
    ScreenVars.Center = OutputTarget->Dim * 0.5f;
  }

  if(RenderGroup->ClearScreen)
  {
    DrawRectangle(OutputTarget, RectMinMax(v2s{0, 0}, OutputTarget->Dim), 
                  RenderGroup->ClearScreenColor.RGB, ClipRect);
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

          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->A, Entry->B, Entry->Color.RGB, ClipRect);
        } break;
      case RenderGroupEntryType_render_entry_coordinate_system
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_coordinate_system);

          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{1,0,0}, v3{1,0,0}, ClipRect);
          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{0,1,0}, v3{0,1,0}, ClipRect);
          DrawVector(RenderGroup, &ScreenVars, OutputTarget,
                     Entry->Tran*v3{0,0,0}, Entry->Tran*v3{0,0,1}, v3{0,0,1}, ClipRect);
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
              ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                     &ScreenVars, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB, ClipRect);
            }
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[(VertIndex+0)%4 + 4];
            v3 V1 = AABB.Verts[(VertIndex+1)%4 + 4];
            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                     &ScreenVars, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB, ClipRect);
            }
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[VertIndex];
            v3 V1 = AABB.Verts[VertIndex + 4];
            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                     &ScreenVars, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              DrawLine(OutputTarget, LineSegment.A, LineSegment.B, Entry->Color.RGB, ClipRect);
            }
          }
        } break;
      //TODO(bjorn): Quad seam problems are still there.
      case RenderGroupEntryType_render_entry_blank_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_blank_quad);

          //TODO(bjorn): Think about how and when in the pipeline to render the hit-points.
          quad_verts_result Quad = GetQuadVertices(RenderGroup->WorldToCamera * Entry->Tran);

          {
            uv_triangle_clip_result ClipResult = 
              ClipUVTriangleByZPlane(RenderGroup->CamParam.NearClipPoint, 
                                     Quad.Verts[0], Quad.Verts[1], Quad.Verts[2], 
                                     {0,0}, {0,1}, {1,1});
            if(ClipResult.TriCount > 0)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[1], 
                           ClipResult.Verts[2], 
                           ClipResult.UV[0],
                           ClipResult.UV[1],
                           ClipResult.UV[2],
                           0,
                           Entry->Color,
                           ClipRect,
                           PlainFillShader);
            }
            if(ClipResult.TriCount > 1)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[2], 
                           ClipResult.Verts[3], 
                           ClipResult.UV[0],
                           ClipResult.UV[2],
                           ClipResult.UV[3],
                           0,
                           Entry->Color,
                           ClipRect,
                           PlainFillShader);
            }
          }

          {
            uv_triangle_clip_result ClipResult = 
              ClipUVTriangleByZPlane(RenderGroup->CamParam.NearClipPoint, 
                                     Quad.Verts[0], Quad.Verts[2], Quad.Verts[3], 
                                     {0,0}, {1,1}, {1,0});
            if(ClipResult.TriCount > 0)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[1], 
                           ClipResult.Verts[2], 
                           ClipResult.UV[0],
                           ClipResult.UV[1],
                           ClipResult.UV[2],
                           0,
                           Entry->Color,
                           ClipRect,
                           PlainFillShader);
            }
            if(ClipResult.TriCount > 1)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[2], 
                           ClipResult.Verts[3], 
                           ClipResult.UV[0],
                           ClipResult.UV[2],
                           ClipResult.UV[3],
                           0,
                           Entry->Color,
                           ClipRect,
                           PlainFillShader);
            }
          }

#if 0
          {
            u32 TriCount = 1;
            v3 Verts[4] = {Quad.Verts[0], Quad.Verts[2], Quad.Verts[3], {}};
            v3 UV[4] = {{0,0}, {1,1}, {1,0}, {}};

            DrawTriangle(OutputTarget, 
                         &RenderGroup->CamParam, &ScreenVars, 
                         Verts[0], 
                         Verts[1], 
                         Verts[2], 
                         UV[0],
                         UV[1],
                         UV[2],
                         0,
                         Entry->Color,
                         ClipRect);
            if(TriCount == 2)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           Verts[0], 
                           Verts[2], 
                           Verts[3], 
                           UV[0],
                           UV[2],
                           UV[3],
                           0,
                           Entry->Color,
                           ClipRect);
            }
          }
#endif
        } break;
      case RenderGroupEntryType_render_entry_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad);

          quad_verts_result Quad = GetQuadVertices(RenderGroup->WorldToCamera * Entry->Tran);

          //TODO(bjorn): Why cant this be the same as the exact lens chamber size?

          rect_corner_v2_result UVCorners = GetRectCorners(Entry->BitmapUVRect);
          {
            uv_triangle_clip_result ClipResult = 
              ClipUVTriangleByZPlane(RenderGroup->CamParam.NearClipPoint, 
                                     Quad.Verts[0], Quad.Verts[1], Quad.Verts[2], 
                                     UVCorners.BL, UVCorners.TL, UVCorners.TR);
            if(ClipResult.TriCount > 0)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[1], 
                           ClipResult.Verts[2], 
                           ClipResult.UV[0],
                           ClipResult.UV[1],
                           ClipResult.UV[2],
                           Entry->Bitmap,
                           Entry->Color,
                           ClipRect,
                           BitmapShader);
            }
            if(ClipResult.TriCount > 1)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[2], 
                           ClipResult.Verts[3], 
                           ClipResult.UV[0],
                           ClipResult.UV[2],
                           ClipResult.UV[3],
                           Entry->Bitmap,
                           Entry->Color,
                           ClipRect,
                           BitmapShader);
            }
          }

          {
            uv_triangle_clip_result ClipResult = 
              ClipUVTriangleByZPlane(RenderGroup->CamParam.NearClipPoint, 
                                     Quad.Verts[0], Quad.Verts[2], Quad.Verts[3], 
                                     UVCorners.BL, UVCorners.TR, UVCorners.BR);
            if(ClipResult.TriCount > 0)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[1], 
                           ClipResult.Verts[2], 
                           ClipResult.UV[0],
                           ClipResult.UV[1],
                           ClipResult.UV[2],
                           Entry->Bitmap,
                           Entry->Color,
                           ClipRect,
                           BitmapShader);
            }
            if(ClipResult.TriCount > 1)
            {
              DrawTriangle(OutputTarget, 
                           &RenderGroup->CamParam, &ScreenVars, 
                           ClipResult.Verts[0], 
                           ClipResult.Verts[2], 
                           ClipResult.Verts[3], 
                           ClipResult.UV[0],
                           ClipResult.UV[2],
                           ClipResult.UV[3],
                           Entry->Bitmap,
                           Entry->Color,
                           ClipRect,
                           BitmapShader);
            }
          }

#if 0
          v2 PixVerts[4] = {};
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = Quad.Verts[VertIndex];
            v3 V1 = Quad.Verts[(VertIndex + 1)%4];

            pixel_line_segment_result LineSegment = 
              ProjectSegmentToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                     &ScreenVars, V0, V1);
            if(LineSegment.PartOfSegmentInView)
            {
              PixVerts[VertIndex] = LineSegment.A;
            }
          }

          DrawLine(OutputTarget, PixVerts[0], PixVerts[2], {1.0f, 0.25f, 1.0f});
          DrawCircle(OutputTarget, (PixVerts[0] + PixVerts[2]) * 0.5f, 3.0f, {1.0f, 1.0f, 0.0f, 1.0f});

          pixel_pos_result PixPos = 
            ProjectPointToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                 &ScreenVars, (Quad.Verts[0]+Quad.Verts[2])*0.5f);
          if(PixPos.PointIsInView)
          {
            DrawCircle(OutputTarget, PixPos.P, 3.0f, {0.0f, 1.0f, 1.0f, 1.0f});
          }
          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            DrawLine(OutputTarget, PixVerts[VertIndex], PixVerts[(VertIndex + 1)%4], 
                     {1.0f, VertIndex*0.25f, 1.0f});
          }
#endif
        } break;
      case RenderGroupEntryType_render_entry_triangle_fill_flip
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_triangle_fill_flip);

          DrawTriangle(OutputTarget, 
                       &RenderGroup->CamParam, &ScreenVars, 
                       Entry->Sta, 
                       Entry->Mid, 
                       Entry->End, 
                       {1,0},
                       {1,1},
                       {0,1},
                       0,
                       {},
                       ClipRect,
                       PlainFillFlipShader);
        } break;
      case RenderGroupEntryType_render_entry_quad_bezier_fill_flip
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad_bezier_fill_flip);

          DrawTriangle(OutputTarget, 
                       &RenderGroup->CamParam, &ScreenVars, 
                       Entry->Sta, 
                       Entry->Mid, 
                       Entry->End, 
                       {1,0},
                       {1,1},
                       {0,1},
                       0,
                       {},
                       ClipRect,
                       QuadBezierFlipShader);
        } break;
      case RenderGroupEntryType_render_entry_sphere
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_sphere);

          v3 P0 = Entry->Tran * v3{0,0,0};
          v3 P1 = Entry->Tran * v3{0.5f,0,0};

          pixel_pos_result PixPos = 
            ProjectPointToScreen(RenderGroup->WorldToCamera, &RenderGroup->CamParam, 
                                 &ScreenVars, P0);
          if(PixPos.PointIsInView)
          {
            f32 PixR = (ScreenVars.MeterToPixel.A * 
                        Magnitude(P1 - P0) * 
                        PixPos.PerspectiveCorrection);

            DrawCircle(OutputTarget, PixPos.P, PixR, Entry->Color, ClipRect);
          }
        } break;
      InvalidDefaultCase;
    }
  }
#endif
}

#define OPENGL_RENDERER_H
#endif
