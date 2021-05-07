#if !defined(OPENGL_RENDERER_H)

#include "platform.h"
#include "render_group.h"
#include <gl/gl.h>

global_variable s32 GLOBAL_NextTextureHandle = 0;
RENDER_GROUP_TO_OUTPUT(OpenGLRenderGroupToOutput)
{
  BEGIN_TIMED_BLOCK(RenderGroupToOutput);

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

          m44 CameraToScreen = { a, 0, 0, 0,
                                 0, b, 0, 0,
                                 0, 0, c, d,
                                 0, 0, -f, 1, };

          m44 WorldToScreen = CameraToScreen * Entry->WorldToCamera;

          glMatrixMode(GL_PROJECTION);
          glLoadMatrixf((const GLfloat*)Transpose(WorldToScreen).E_);
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

          v3 V0 = Entry->A;
          v3 V1 = Entry->B;

          glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

          glVertex3f(V0.X, V0.Y, V0.Z);
          glVertex3f(V1.X, V1.Y, V1.Z);

          glEnd();
        } break;
      case RenderGroupEntryType_render_entry_coordinate_system
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_coordinate_system);

          glMatrixMode(GL_MODELVIEW);
          glLoadMatrixf((const GLfloat*)Transpose(Entry->Tran).E_);

          glBegin(GL_LINES);

          {
            v3 V0 = v3{0,0,0};
            v3 V1 = v3{1,0,0};

            glColor3f(1,0,0);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          {
            v3 V0 = v3{0,0,0};
            v3 V1 = v3{0,1,0};

            glColor3f(0,1,0);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          {
            v3 V0 = v3{0,0,0};
            v3 V1 = v3{0,0,1};

            glColor3f(0,0,1);

            glVertex3f(V0.X, V0.Y, V0.Z);
            glVertex3f(V1.X, V1.Y, V1.Z);
          }

          glEnd();
        } break;
      case RenderGroupEntryType_render_entry_wire_cube
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_wire_cube);

          glMatrixMode(GL_MODELVIEW);
          glLoadMatrixf((const GLfloat*)Transpose(Entry->Tran).E_);

          //TODO(bjorn): Replace with a line mesh with colors
          m44 Identity = M44Identity();
          aabb_verts_result AABB = GetAABBVertices(&Identity);

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

          glMatrixMode(GL_MODELVIEW);
          glLoadMatrixf((const GLfloat*)Transpose(Entry->Tran).E_);

          //TODO(bjorn): Replace with a line mesh with colors
          m44 Identity = M44Identity();
          quad_verts_result Quad = GetQuadVertices(Identity);

          glBegin(GL_TRIANGLES);

          glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

          v3* Verts = Quad.Verts;
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

          glEnd();
        } break;
      case RenderGroupEntryType_render_entry_quad
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_quad);

          glMatrixMode(GL_MODELVIEW);
          glLoadMatrixf((const GLfloat*)Transpose(Entry->Tran).E_);

          //TODO(bjorn): Replace with a line mesh with colors
          m44 Identity = M44Identity();
          quad_verts_result Quad = GetQuadVertices(Identity);

          glEnable(GL_TEXTURE_2D);

          game_bitmap* Bitmap = GetBitmap(Assets, Entry->AssetID);
          if(Bitmap)
          {
            if(!Bitmap->GPUHandle)
            {
              Bitmap->GPUHandle = GLOBAL_NextTextureHandle++;
              glBindTexture(GL_TEXTURE_2D, Bitmap->GPUHandle);

              glPixelStorei(GL_UNPACK_ROW_LENGTH, Bitmap->Pitch);

              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

              glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

              glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Bitmap->Width, Bitmap->Height, 0,
                           GL_BGRA_EXT, GL_UNSIGNED_BYTE, Bitmap->Memory);
            }
            else
            {
              glBindTexture(GL_TEXTURE_2D, Bitmap->GPUHandle);
            }
          }

          glEnable(GL_BLEND);
          glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

          glBegin(GL_TRIANGLES);

          glColor3f(Entry->Color.R, Entry->Color.G, Entry->Color.B);

          v3* Verts = Quad.Verts;
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

          glEnd();
          glDisable(GL_TEXTURE_2D);
        } break;
      case RenderGroupEntryType_render_entry_sphere
        : {
          RenderGroup_DefineEntryAndAdvanceByteOffset(render_entry_sphere);

          glMatrixMode(GL_MODELVIEW);
          glLoadMatrixf((const GLfloat*)Transpose(Entry->Tran).E_);

          game_mesh* Mesh = GetMesh(Assets, GAI_SphereMesh);
          if(Mesh)
          {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

            glColor4f(Entry->Color.R, Entry->Color.G, Entry->Color.B, Entry->Color.A);

            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(Mesh->Dimensions, GL_FLOAT,
                            0, //TODO(bjorn): Measure if stride matter here.
                            Mesh->Vertices);

            glDrawElements(GL_TRIANGLES, Mesh->FaceCount * Mesh->EdgesPerFace,
                           GL_UNSIGNED_INT, Mesh->Indicies);

            glColor4f(Entry->Color.R*0.8f, 
                      Entry->Color.G*0.8f, 
                      Entry->Color.B*0.8f, 
                      Entry->Color.A);
            glDrawElements(GL_LINES, Mesh->FaceCount * Mesh->EdgesPerFace,
                           GL_UNSIGNED_INT, Mesh->Indicies);
          }
        } break;
      InvalidDefaultCase;
    }
  }

  END_TIMED_BLOCK(RenderGroupToOutput);
}

#define OPENGL_RENDERER_H
#endif
