#if !defined(ATTACHMENT_H)
#if !defined(ATTACHMENT_H_FIRST_PASS)

enum attachment_type
{
	AttachmentType_Spring,
	AttachmentType_Bungee,
	AttachmentType_Rod,
	AttachmentType_Cable,
};
struct attachment_info
{
	attachment_type Type;

	v3 AP1;
	v3 AP2;
	union
	{
		f32 RestLength;
		f32 SlackLength;
		f32 Length;
	};
	union
	{
		f32 SpringConstant;
		f32 Restitution;
	};
};

#define ATTACHMENT_H_FIRST_PASS
#else

//TODO(bjorn): If no attachments just do nothing but also put in asserts.
internal_function void
RemoveOneWayAttachment(entity* Subject, entity* EndPoint);
internal_function void
RemoveTwoWayAttachment(entity* A, entity* B);

	internal_function attachment_info*
AddAttachmentRaw(entity* Subject, entity* EndPoint, 
								 v3 SubjectAttachmentPoint = {}, v3 EndPointAttachmentPoint = {})
{
	attachment_info* Result = 0;

	Assert(Subject && EndPoint);
	Assert(Subject != EndPoint);
	Assert(Subject->AttachmentCount < ArrayCount(Subject->Attachments));
	u32 AttachmentIndex = Subject->AttachmentCount++;

	Subject->Attachments[AttachmentIndex] = EndPoint;
	Result = Subject->AttachmentInfo + AttachmentIndex;

	*Result = {};
	Result->AP1 = SubjectAttachmentPoint;
	Result->AP2 = EndPointAttachmentPoint;

	return Result;
}

	internal_function void
AddOneWaySpringAttachment(entity* Subject, entity* EndPoint, f32 SpringConstant, f32 RestLength,
													v3 Subject_AP = {}, v3 EndPoint_AP = {})
{
	attachment_info* Info = AddAttachmentRaw(Subject, EndPoint, Subject_AP, EndPoint_AP);
	Info->Type = AttachmentType_Spring;
	Info->SpringConstant = SpringConstant;
	Info->Length = RestLength;
}
internal_function void
AddTwoWaySpringAttachment(entity* A, entity* B, f32 SpringConstant, f32 RestLength,
													v3 A_AP = {}, v3 B_AP = {})
{
	AddOneWaySpringAttachment(A, B, SpringConstant, RestLength, A_AP, B_AP);
	AddOneWaySpringAttachment(B, A, SpringConstant, RestLength, B_AP, A_AP);
}

internal_function void
AddOneWayBungeeAttachment(entity* Subject, entity* EndPoint, f32 SpringConstant, f32 RestLength,
													v3 Subject_AP = {}, v3 EndPoint_AP = {})
{
	attachment_info* Info = AddAttachmentRaw(Subject, EndPoint, Subject_AP, EndPoint_AP);
	Info->Type = AttachmentType_Bungee;
	Info->SpringConstant = SpringConstant;
	Info->RestLength = RestLength;
}
internal_function void
AddTwoWayBungeeAttachment(entity* A, entity* B, f32 SpringConstant, f32 RestLength,
													v3 A_AP = {}, v3 B_AP = {})
{
	AddOneWayBungeeAttachment(A, B, SpringConstant, RestLength, A_AP, B_AP);
	AddOneWayBungeeAttachment(B, A, SpringConstant, RestLength, B_AP, A_AP);
}

internal_function void
AddOneWayRodAttachment(entity* Subject, entity* EndPoint, f32 Length)
{
	attachment_info* Info = AddAttachmentRaw(Subject, EndPoint);
	Info->Type = AttachmentType_Rod;
	Info->Length = Length;
}
internal_function void
AddTwoWayRodAttachment(entity* A, entity* B, f32 Length)
{
	AddOneWayRodAttachment(A, B, Length);
	AddOneWayRodAttachment(B, A, Length);
}

internal_function void
AddOneWayCableAttachment(entity* Subject, entity* EndPoint, f32 Restitution, f32 SlackLength)
{
	Assert(0 <= Restitution && Restitution <= 1);
	attachment_info* Info = AddAttachmentRaw(Subject, EndPoint);
	Info->Type = AttachmentType_Cable;
	Info->Restitution = Restitution;
	Info->SlackLength = SlackLength;
}
internal_function void
AddTwoWayCableAttachment(entity* A, entity* B, f32 Restitution, f32 SlackLength)
{
	AddOneWayCableAttachment(A, B, Restitution, SlackLength);
	AddOneWayCableAttachment(B, A, Restitution, SlackLength);
}

//TODO STUDY(bjorn): Multiple attachment can still totally glicth out sometimes. 
// * Make attachments have breaking points and failstates (more realistic)
// * Special case handling of generated speeds over sim_region maximum.
// * Maybe solve it through some sort of ordererd resolution scheme.
internal_function void
ApplyAttachmentForcesAndImpulses(entity* Entity)
{
	for(u32 AttachmentIndex = 0;
			AttachmentIndex < ArrayCount(Entity->Attachments);
			AttachmentIndex++)
	{
		entity* EndPoint = Entity->Attachments[AttachmentIndex];
		if(EndPoint)
		{
			attachment_info Info = Entity->AttachmentInfo[AttachmentIndex];

			f32 PointSeparationEpsilon = 0.0001f;

			v3 AP1 = Entity->ObjToWorldTransform   * Info.AP1;
			v3 AP2 = EndPoint->ObjToWorldTransform * Info.AP2;

			if(Info.Type == AttachmentType_Spring || Info.Type == AttachmentType_Bungee)
			{
				v3 PointSeparationVector = AP1 - AP2;
				f32 PointSeparation = Length(PointSeparationVector);
				f32 RestLength = Info.Length;
				f32 Separation = (PointSeparation - RestLength);

				if(Info.Type == AttachmentType_Spring || 
					 Separation > 0)
				{
					f32 SpringConstant = Info.SpringConstant;

					v3 PointSeparationNormal = Normalize(PointSeparationVector);
					v3 ForceVector = (-SpringConstant*Separation) * PointSeparationNormal;
					//TODO STUDY(bjorn): Why does the springs go out of control without drag?
					//TODO IMPORTANT(bjorn): Debug this tourqe addition!!!
					if(IsZeroVector(Info.AP1))
					{
						Entity->F += ForceVector;
					}
					else
					{
						v3 AttachmentPointVector = (AP1 - Entity->P);
						v3 AttachmentPointNormal = Normalize(AttachmentPointVector);

						//TODO STUDY(bjorn): Why do the author in (Game Physics Millington
						//2010) apply the force through the center even when parallel??
						Entity->F += ForceVector * Dot(PointSeparationNormal, -AttachmentPointNormal);
						v3 Torque = Cross(AttachmentPointVector, -ForceVector);

						Entity->T += Torque;
					}
				}
			}

			if(Info.Type == AttachmentType_Rod || Info.Type == AttachmentType_Cable)
			{
				f32 iM_iSum = SafeRatio0(1.0f, Entity->Body.iM + EndPoint->Body.iM);

				if(iM_iSum)
				{
					f32 AttachmentLength = Info.Length;
					f32 Restitution = 0;
					if(Info.Type == AttachmentType_Cable)
					{
						Restitution = (Info.Restitution + Entity->Restitution) * 0.5f;
					}

					v3 PointSeparationVector = AP1 - AP2;

					f32 PointSeparation = Length(PointSeparationVector);
					f32 Penetration = (AttachmentLength - PointSeparation);

					v3 ContactNormal = Normalize(PointSeparationVector);
					f32 SeparatingVelocity = Dot(ContactNormal, Entity->dP - EndPoint->dP);

					f32 Impulse = -(Restitution + 1) * SeparatingVelocity * iM_iSum;
					//TODO STUDY(bjorn): Why is the solution to always do normal inpulse creation.
#if 0
					if(Penetration < 0)
					{
						Impulse = -Impulse;
					}
#endif

					if(Info.Type == AttachmentType_Rod ||
						 (Info.Type == AttachmentType_Cable && Penetration < 0))
					{
						Entity->dP   += ContactNormal * (Entity->Body.iM   * Impulse);
						EndPoint->dP -= ContactNormal * (EndPoint->Body.iM * Impulse);

						f32 Entity_MassRatio   = (Entity->Body.iM   * iM_iSum);
						f32 EndPoint_MassRatio = (EndPoint->Body.iM * iM_iSum);
						Entity->P   += (Entity_MassRatio   * Penetration) * ContactNormal;
						EndPoint->P -= (EndPoint_MassRatio * Penetration) * ContactNormal;
					}
				}
			}
		}
	}
}

#define ATTACHMENT_H
#endif //ATTACHMENT_H_FIRST_PASS
#endif //ATTACHMENT_H
