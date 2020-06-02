#if !defined(OPENGL_RENDERER_H)

#include "platform.h"
#include "render_group.h"
#include <gl/gl.h>

internal_function void
OGLDrawQuad(v3* Verts)
{
  glTexCoord2f(0,0);
    glVertex3f(Verts[0].X, Verts[0].Y, Verts[0].Z);
  glTexCoord2f(0,1);
    glVertex3f(Verts[1].X, Verts[1].Y, Verts[1].Z);
  glTexCoord2f(1,1);
    glVertex3f(Verts[2].X, Verts[2].Y, Verts[2].Z);

  glTexCoord2f(0,0);
    glVertex3f(Verts[0].X, Verts[0].Y, Verts[0].Z);
  glTexCoord2f(1,1);
    glVertex3f(Verts[2].X, Verts[2].Y, Verts[2].Z);
  glTexCoord2f(1,0);
    glVertex3f(Verts[3].X, Verts[3].Y, Verts[3].Z);
}

global_variable s32 GLOBAL_NextTextureHandle = 0;
RENDER_GROUP_TO_OUTPUT(OpenGLRenderGroupToOutput)
{
  BEGIN_TIMED_BLOCK(RenderGroupToOutput);

  m44 WorldToCamera = {};

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
      case RenderGroupEntryType_render_entry_camera
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_camera);

          f32 ClipSize = (Entry->FarClipPoint - Entry->NearClipPoint);
          f32 a = SafeRatio0(1.0f, Entry->ScreenWidthInMeters * 0.5f);
          f32 b = SafeRatio0(1.0f, Entry->ScreenHeightInMeters * 0.5f);
          f32 c = SafeRatio0(1.0f, ClipSize * 0.5f);
          f32 d = 1.0f + SafeRatio0(Entry->NearClipPoint, ClipSize * 0.5f);
          f32 f = SafeRatio0(1.0f, Entry->LensChamberSize);

          f32 ProjMat[16] = { a, 0, 0, 0,
                              0, b, 0, 0,
                              0, 0, c,-f,
                              0, 0, d, 1, };

          glMatrixMode(GL_PROJECTION);
          glLoadMatrixf((const GLfloat*)(&ProjMat));

          //TODO(bjorn): Combine this with the projection matrix.
          WorldToCamera = Entry->WorldToCamera;
        } break;
      case RenderGroupEntryType_render_entry_clear_screen
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_clear_screen);

          v4 Color = Entry->Color;
          glClearColor(Color.R, Color.G, Color.B, Color.A);
          glClear(GL_COLOR_BUFFER_BIT);
        } break;
      case RenderGroupEntryType_render_entry_vector
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_vector);

          glBegin(GL_LINES);

          v3 V0 = WorldToCamera * Entry->A;
          v3 V1 = WorldToCamera * Entry->B;

          glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

          glVertex3f(V0.X, V0.Y, V0.Z);
          glVertex3f(V1.X, V1.Y, V1.Z);

          glEnd();
        } break;
      case RenderGroupEntryType_render_entry_coordinate_system
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_coordinate_system);

          glBegin(GL_LINES);

          m44 Tran = WorldToCamera * Entry->Tran;
          {
            v3 V0 = Tran*v3{0,0,0};
            v3 V1 = Tran*v3{1,0,0};

            glColor3f(1,0,0);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          {
            v3 V0 = Tran*v3{0,0,0};
            v3 V1 = Tran*v3{0,1,0};

            glColor3f(0,1,0);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          {
            v3 V0 = Tran*v3{0,0,0};
            v3 V1 = Tran*v3{0,0,1};

            glColor3f(0,0,1);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          glEnd();
        } break;
      case RenderGroupEntryType_render_entry_wire_cube
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_wire_cube);

          m44 Tran = WorldToCamera * Entry->Tran;
          aabb_verts_result AABB = GetAABBVertices(&Tran);

          glBegin(GL_LINES);

          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[(VertIndex+0)%4];
            v3 V1 = AABB.Verts[(VertIndex+1)%4];

            glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[(VertIndex+0)%4 + 4];
            v3 V1 = AABB.Verts[(VertIndex+1)%4 + 4];

            glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          for(u32 VertIndex = 0; 
              VertIndex < 4; 
              VertIndex++)
          {
            v3 V0 = AABB.Verts[VertIndex];
            v3 V1 = AABB.Verts[VertIndex + 4];

            glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          glEnd();
        } break;
      case RenderGroupEntryType_render_entry_blank_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_blank_quad);

          quad_verts_result Quad = GetQuadVertices(WorldToCamera * Entry->Tran);

          glBegin(GL_TRIANGLES);

          glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

          OGLDrawQuad(Quad.Verts);

          glEnd();
        } break;
      case RenderGroupEntryType_render_entry_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad);

          quad_verts_result Quad = GetQuadVertices(WorldToCamera * Entry->Tran);

          glEnable(GL_TEXTURE_2D);

          if(!Entry->Bitmap->GPUHandle)
          {
            Entry->Bitmap->GPUHandle = GLOBAL_NextTextureHandle++;
            glBindTexture(GL_TEXTURE_2D, Entry->Bitmap->GPUHandle);

            glPixelStorei(GL_UNPACK_ROW_LENGTH, Entry->Bitmap->Pitch);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Entry->Bitmap->Width, Entry->Bitmap->Height, 0,
                         GL_BGRA_EXT, GL_UNSIGNED_BYTE, Entry->Bitmap->Memory);
          }
          else
          {
            glBindTexture(GL_TEXTURE_2D, Entry->Bitmap->GPUHandle);
          }

          glEnable(GL_BLEND);
          glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

          glBegin(GL_TRIANGLES);

          glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

          OGLDrawQuad(Quad.Verts);

          glEnd();
          glDisable(GL_TEXTURE_2D);
        } break;
      case RenderGroupEntryType_render_entry_triangle_fill_flip
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_triangle_fill_flip);

          //TODO(Bjorn): Shaders
#if 0
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
#endif
        } break;
      case RenderGroupEntryType_render_entry_quad_bezier_fill_flip
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad_bezier_fill_flip);

          //TODO(Bjorn): Shaders
#if 0
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
#endif
        } break;
      case RenderGroupEntryType_render_entry_sphere
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_sphere);

          //TODO(Bjorn): Shaders
#if 0
          v3 P0 = Entry->Tran * v3{0,0,0};
          v3 P1 = Entry->Tran * v3{0.5f,0,0};

          pixel_pos_result PixPos = 
            ProjectPointToScreen(WorldToCamera, &RenderGroup->CamParam, 
                                 &ScreenVars, P0);
          if(PixPos.PointIsInView)
          {
            f32 PixR = (ScreenVars.MeterToPixel.A * 
                        Magnitude(P1 - P0) * 
                        PixPos.PerspectiveCorrection);

            DrawCircle(OutputTarget, PixPos.P, PixR, Entry->Color, ClipRect);
          }
#endif
        } break;
      InvalidDefaultCase;
    }
  }

  END_TIMED_BLOCK(RenderGroupToOutput);
}

#define OPENGL_RENDERER_H
#endif
