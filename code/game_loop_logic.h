#if !defined(GAME_LOOP_LOGIC_H)
#define GAME_LOOP_LOGIC_H

inline void
EntityPairUpdate(entity* Entity, entity* OtherEntity, trigger_result TriggerResult, f32 dT)
{
  //TODO STUDY Doublecheck HMH 69 to see what this was for again.
#if 0
  entity* A = Entity;
  entity* B = OtherEntity;
  if(Entity->StorageIndex > OtherEntity->StorageIndex)
  {
    Swap(A, B, entity*);
  }
#endif

  //TODO (bjorn): Both ForceFieldLogic and FloorLogic needs to be moved
  //to a trigger handling and a collision handling system respectively. 
  //NOTE(bjorn): CollisionFiltering makes this call now non-global.
  ForceFieldLogic(Entity, OtherEntity);
  //FloorLogic(Entity, OtherEntity);

  if(TriggerResult.OnEnter)
  {
    ApplyDamage(Entity, OtherEntity);
  }

  if(TriggerResult.OnLeave)
  {
    Bounce(Entity, OtherEntity);
  }
}

  inline void
PrePhysicsFisrtStepOnlyEntityUpdate(entity* Entity, entity* MainCamera, f32 SecondsToUpdate)
{
  if(Entity->Keyboard && 
     Entity->Keyboard->IsConnected)
  {
    v3 WASDKeysDirection = {};
    if(Held(Entity->Keyboard, S))
    {
      if(Held(Entity->Keyboard, Shift))
      {
        WASDKeysDirection.Z += -1;
      }
      else
      {
        WASDKeysDirection.Y += -1;
      }
    }
    if(Held(Entity->Keyboard, A))
    {
      WASDKeysDirection.X += -1;
    }
    if(Held(Entity->Keyboard, W))
    {
      if(Held(Entity->Keyboard, Shift))
      {
        WASDKeysDirection.Z += 1;
      }
      else
      {
        WASDKeysDirection.Y += 1;
      }
    }
    if(Held(Entity->Keyboard, D))
    {
      WASDKeysDirection.X += 1;
    }
    if(Clicked(Entity->Keyboard, Space))
    {
      if(!Entity->IsCamera &&
         Entity != MainCamera->FreeMover)
      {
        WASDKeysDirection.Z += 1;
      }
    }
    if(LengthSquared(WASDKeysDirection) > 1.0f)
    {
      WASDKeysDirection *= inv_root2;
    }

    Entity->MoveSpec.MovingDirection = 
      Entity->MoveSpec.MoveOnInput ? WASDKeysDirection : v3{};

    if(Entity->Sword &&
       Entity->MoveSpec.MovingDirection.Z > 0)
    {
      Entity->dP += v3{0,0,10.0f};
    }

    if(Entity->ForceFieldRadiusSquared)
    {
      if(Held(Entity->Keyboard, Space))
      {
        Entity->ForceFieldStrenght = 100.0f;
      }
      else
      {
        Entity->ForceFieldStrenght = 0.0f;
      }
    }

    v2 ArrowKeysDirection = {};
    if(Held(Entity->Keyboard, Down))                 { ArrowKeysDirection.Y += -1; }
    if(Held(Entity->Keyboard, Left))                 { ArrowKeysDirection.X += -1; }
    if(Held(Entity->Keyboard, Up))                   { ArrowKeysDirection.Y +=  1; }
    if(Held(Entity->Keyboard, Right))                { ArrowKeysDirection.X +=  1; }
    if(ArrowKeysDirection.X && ArrowKeysDirection.Y) { ArrowKeysDirection *= inv_root2; }

    if((ArrowKeysDirection.X || ArrowKeysDirection.Y) &&
       Clicked(Entity->Keyboard, Q) &&
       Entity->Sword)
    {
      entity* Sword = Entity->Sword;

      if(!Sword->IsSpacial) { Sword->DistanceRemaining = 20.0f; }

      //TODO(bjorn): Use the relevant body primitive scale instead.
      v3 P = (Sword->IsSpacial ?  
              Sword->P : (Entity->P + ArrowKeysDirection * Sword->Body.Primitives[1].S.Y));

      MakeEntitySpacial(Sword, P, ArrowKeysDirection * 1.0f, 
                        AngleAxisToQuaternion(ArrowKeysDirection, Normalize(v3{0,0.8f,-1})),
                        Normalize({2,3,1})*tau32*0.2f);
    }

    if(Entity->IsCamera)
    {
      if(Clicked(Entity->Keyboard, C))
      {
        if(Entity->Prey == Entity->Player)
        {
          DetachToMoveFreely(Entity);
        }
        else if(Entity->Prey == Entity->FreeMover)
        {
          AttachToPlayer(Entity);
        }
      }

      //TODO Smoother rotation.
      //TODO(bjorn): Remove this hack as soon as I have full 3D rotations
      //going on for all entites!!!
      v2 RotSpeed = v2{1, -1} * (tau32 * SecondsToUpdate * 20.0f);
      f32 ZoomSpeed = (SecondsToUpdate * 30.0f);

      f32 dZoom = 0;
      v2 MouseDir = {};
      if(Entity->Mouse && 
         Entity->Mouse->IsConnected)
      {
        if(Held(Entity->Mouse, Left))
        {
          MouseDir = Entity->Mouse->dP;
          if(MouseDir.X && MouseDir.Y)
          {
            MouseDir *= inv_root2;
          }
        }
        dZoom = Entity->Mouse->Wheel * ZoomSpeed;
      }

      v2 dCamRot = Hadamard(MouseDir, RotSpeed);

      Entity->CamZoom = Entity->CamZoom + dZoom;
      Entity->CamRot = Modulate0(Entity->CamRot + dCamRot, tau32);
    }
  }
}

  inline void
PrePhysicsStepEntityUpdate(entity* Entity, entity* MainCamera, f32 dT)
{
  //TODO IMPORTANT(bjorn): Is all MainCamera stuff really just a pairing update
  //for bodyless entities? Maybe there should be a more general flag like
  //"InteractWithAllUpdatingEntities" so the MainCamera does not have to be
  //found beforehand.
  f32 DistanceToGlyphSquared = LengthSquared(Entity->P - MainCamera->P);
  b32 SwitchToOtherGlyph = 
    (Entity->Character &&
     Entity != MainCamera &&
     MainCamera->GlyphSwitchCooldown == 0 &&
     Entity != MainCamera->CurrentGlyph && 
     MainCamera->Keyboard->UnicodeInCount > 0 &&
     MainCamera->Keyboard->UnicodeIn[0] == Entity->Character &&
     DistanceToGlyphSquared < MainCamera->BestDistanceToGlyphSquared);
  if(SwitchToOtherGlyph)
  {
    MainCamera->BestDistanceToGlyphSquared = DistanceToGlyphSquared;

    MainCamera->PotentialGlyph = Entity;
  }

  HunterLogic(Entity);
  v3 OldP = Entity->P;
}

inline void
PostPhysicsStepEntityUpdate(entity* Entity, entity* OldEntity, f32 dT)
{
  v3 OldP = OldEntity->P;
  v3 NewP = Entity->P;
  f32 dP = Length(NewP - OldP);

  if(Entity->DistanceRemaining > 0)
  {
    Entity->DistanceRemaining -= dP;
    if(Entity->DistanceRemaining <= 0)
    {
      Entity->DistanceRemaining = 0;

      MakeEntityNonSpacial(Entity);
    }
  }

  if(Entity->HunterSearchRadius)
  {
    Entity->BestDistanceToPreySquared = Square(Entity->HunterSearchRadius);
    Entity->MoveSpec.MovingDirection = {};
  }
}

  inline void
RenderEntity(render_group* RenderGroup, transient_state* TransientState, 
             entity* Entity, entity* MainCamera)
{
  m44 T = Entity->Tran; 

  if(LengthSquared(Entity->dP) != 0.0f)
  {
    if(Absolute(Entity->dP.X) > Absolute(Entity->dP.Y))
    {
      Entity->FacingDirection = (Entity->dP.X > 0) ? 3 : 1;
    }
    else
    {
      Entity->FacingDirection = (Entity->dP.Y > 0) ? 2 : 0;
    }
  }

  if(Entity->IsCamera &&
     Entity == MainCamera)
  {
    v3 Offset = {0, 0, MainCamera->CamZoom};
    m33 XRot = XRotationMatrix(MainCamera->CamRot.Y);
    m33 RotMat = AxisRotationMatrix(MainCamera->CamRot.X, GetMatCol(XRot, 2)) * XRot;
    m44 CamTrans = ConstructTransform(Offset, RotMat);

#if 1
    SetCamera(RenderGroup, CamTrans, MainCamera->CamObserverToScreenDistance);
#else
    SetCamera(RenderGroup, CamTrans, positive_infinity32);
#endif
  }

  if(Entity->VisualType == EntityVisualType_Glyph)
  {
    v4 Color = Entity == MainCamera->CurrentGlyph ? v4{1.0f,0.5f,0.5f,1.0f} : v4{1,1,1,1};
    m44 QuadTran = ConstructTransform({0,0,0}, 
                                      Entity->Body.Primitives[1].S);
    u8 Character = (u8)Entity->Character;
    if(Character < 32 || Character > 126)
    {
      Character = '?';
    }
    rectangle2 SubRect = CharacterToFontMapLocation(Character);
    //TODO PushQuad(RenderGroup, T*QuadTran, GAI_GenFontMap, SubRect, Color);
  }
  if(Entity->VisualType == EntityVisualType_Wall)
  {
    m44 QuadTran = ConstructTransform({0,0,0}, 
                                      AngleAxisToQuaternion(tau32 * 0.25f, {1,0,0}), 
                                      Entity->Body.Primitives[1].S);
    PushQuad(RenderGroup, T*QuadTran, GAI_RockWall);
  }
  if(Entity->VisualType == EntityVisualType_Player)
  {
    hero_bitmaps *Hero = &(TransientState->Assets.HeroBitmaps[Entity->FacingDirection]);

    f32 ZAlpha = Clamp01(1.0f - (Entity->P.Z / 2.0f));
    PushQuad(RenderGroup, T, GAI_Shadow, RectMinDim({0,0},{1,1}), {1, 1, 1, ZAlpha});
    PushQuad(RenderGroup, T, &Hero->Torso);
    PushQuad(RenderGroup, T, &Hero->Cape);
    PushQuad(RenderGroup, T, &Hero->Head);
  }
  if(Entity->VisualType == EntityVisualType_Monstar)
  {
    hero_bitmaps *Hero = &(TransientState->Assets.HeroBitmaps[Entity->FacingDirection]);

    f32 ZAlpha = Clamp01(1.0f - (Entity->P.Z / 2.0f));
    PushQuad(RenderGroup, T, GAI_Shadow, RectMinDim({0,0},{1,1}), {1, 1, 1, ZAlpha});
    PushQuad(RenderGroup, T, &Hero->Torso);
  }
  if(Entity->VisualType == EntityVisualType_Familiar)
  {
    hero_bitmaps *Hero = &(TransientState->Assets.HeroBitmaps[Entity->FacingDirection]);

    f32 ZAlpha = Clamp01(1.0f - ((Entity->P.Z + 1.4f) / 2.0f));
    PushQuad(RenderGroup, T, GAI_Shadow, RectMinDim({0,0},{1,1}), {1, 1, 1, ZAlpha});
    PushQuad(RenderGroup, T, &Hero->Head);
  }
  if(Entity->VisualType == EntityVisualType_Sword)
  {
    PushQuad(RenderGroup, T, GAI_Sword);
  }

  if(Entity->VisualType == EntityVisualType_Monstar ||
     Entity->VisualType == EntityVisualType_Player)
  {
    for(u32 HitPointIndex = 0;
        HitPointIndex < Entity->HitPointMax;
        HitPointIndex++)
    {
      v2 HitPointDim = {0.15f, 0.3f};
      v3 HitPointPos = Entity->P;
      HitPointPos.Y -= 0.3f;
      HitPointPos.X += ((HitPointIndex - (Entity->HitPointMax-1) * 0.5f) * 
                        HitPointDim.X * 1.5f);

      m44 HitPointT = ConstructTransform(HitPointPos, Entity->O, HitPointDim);
      hit_point HP = Entity->HitPoints[HitPointIndex];
      if(HP.FilledAmount == HIT_POINT_SUB_COUNT)
      {
        v3 Green = {0.0f, 1.0f, 0.0f};

        PushBlankQuad(RenderGroup, HitPointT, V4(Green, 1.0f));
      }
      else if(HP.FilledAmount < HIT_POINT_SUB_COUNT &&
              HP.FilledAmount > 0)
      {
        v3 Green = {0.0f, 1.0f, 0.0f};
        v3 Red = {1.0f, 0.0f, 0.0f};

        //TODO(bjorn): How to push this relative to the screen?
        //PushRenderBlankPiece(RenderGroup, HitPointT, (v4)Red);
        //PushRenderBlankPiece(RenderGroup, HitPointT, (v4)Green);
      }
      else
      {
        v3 Red = {1.0f, 0.0f, 0.0f};

        PushBlankQuad(RenderGroup, HitPointT, V4(Red, 1.0f));
      }
    }
  }

#if HANDMADE_INTERNAL
  //TODO(bjorn): Do I need to always push these just to enable toggling when paused?
  if(DEBUG_GameState->DEBUG_VisualiseCollisionBox)
  {
    for(u32 PrimitiveIndex = 0;
        PrimitiveIndex < Entity->Body.PrimitiveCount;
        PrimitiveIndex++)
    {
      body_primitive* Primitive = Entity->Body.Primitives + PrimitiveIndex;
      if(Primitive->CollisionShape == CollisionShape_Sphere)
      {
        v4 Color = {0,0,0.8f,0.05f};

        if(PrimitiveIndex == 0 &&
           Entity->StorageIndex != 2)
        {
          if(!Entity->IsCamera &&
             !Entity->IsFloor &&
             Entity != MainCamera->FreeMover)
          {
            if(Entity->DEBUG_EntityCollided)
            {
              Color = {1,0,0,0.1f};
            }
            else
            {
              continue;
            }
          }
          else
          {
            continue;
          }
        }

        PushSphere(RenderGroup, T * Primitive->Tran, Color);
      }
      else if(Primitive->CollisionShape == CollisionShape_AABB)
      {
        PushWireCube(RenderGroup, T * Primitive->Tran);
      }
    }

    if(!Entity->IsCamera &&
       !Entity->IsFloor &&
       Entity != MainCamera->FreeMover)
    {
      m44 TranslationAndRotation = ConstructTransform(Entity->P, Entity->O, {1,1,1});
      PushCoordinateSystem(RenderGroup, Entity->Tran);

      m44 TranslationOnly = ConstructTransform(Entity->P, QuaternionIdentity(), {1,1,1});
      PushVector(RenderGroup, TranslationOnly, v3{0,0,0}, 
                 Entity->DEBUG_MostRecent_F*(1.0f/10.0f), {1,0,1});
      PushVector(RenderGroup, TranslationOnly, v3{0,0,0}, 
                 Entity->DEBUG_MostRecent_T*(1.0f/tau32), {1,1,0});
    }

    for(u32 AttachmentIndex = 0;
        AttachmentIndex < ArrayCount(Entity->Attachments);
        AttachmentIndex++)
    {
      entity* EndPoint = Entity->Attachments[AttachmentIndex];
      if(EndPoint)
      {
        attachment_info Info = Entity->AttachmentInfo[AttachmentIndex];

        v3 AP1 = Entity->Tran   * Info.AP1;
        v3 AP2 = EndPoint->Tran * Info.AP2;

        v3 APSize = 0.08f*v3{1,1,1};

        m44 T1 = ConstructTransform(AP1, QuaternionIdentity(), APSize);
        PushWireCube(RenderGroup, T1, {0.7f,0,0,1});
        v3 n = Normalize(AP2-AP1);
        m44 T2 = ConstructTransform(AP1 + n*Info.Length, QuaternionIdentity(), APSize);
        PushWireCube(RenderGroup, T2, {1,1,1,1});

        PushVector(RenderGroup, M44Identity(), AP1, AP2);

        PushVector(RenderGroup, M44Identity(), Entity->P, AP1, {1,1,1,1});
      }
    }
  }
#endif
}

#endif //GAME_LOOP_LOGIC_H
