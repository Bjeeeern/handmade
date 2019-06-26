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
	union
	{
		struct
		{
			f32 Constant;
			f32 RestLenght;
		}Spring;
		struct
		{
			f32 Lenght;
		}Rod;
		struct
		{
			f32 Restitution;
			f32 SlackLenght;
		}Cable;
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
AddAttachmentRaw(entity* Subject, entity* EndPoint)
{
	attachment_info* Result = 0;

	Assert(Subject && EndPoint);
	Assert(Subject->AttachmentCount < ArrayCount(Subject->Attachments));
	u32 AttachmentIndex = Subject->AttachmentCount++;

	Subject->Attachments[AttachmentIndex] = EndPoint;
	Result = Subject->AttachmentInfo + AttachmentIndex;

	return Result;
}

internal_function void
AddOneWaySpringAttachment(entity* Subject, entity* EndPoint, f32 Constant, f32 RestLenght)
{
	attachment_info* Info = AddAttachmentRaw(Subject, EndPoint);
	*Info = {};
	Info->Type = AttachmentType_Spring;
	Info->Spring.Constant = Constant;
	Info->Spring.RestLenght = RestLenght;
}
internal_function void
AddTwoWaySpringAttachment(entity* A, entity* B, f32 Constant, f32 RestLenght)
{
	AddOneWaySpringAttachment(A, B, Constant, RestLenght);
	AddOneWaySpringAttachment(B, A, Constant, RestLenght);
}

internal_function void
AddOneWayBungeeAttachment(entity* Subject, entity* EndPoint, f32 Constant, f32 RestLenght)
{
	attachment_info* Info = AddAttachmentRaw(Subject, EndPoint);
	*Info = {};
	Info->Type = AttachmentType_Bungee;
	Info->Spring.Constant = Constant;
	Info->Spring.RestLenght = RestLenght;
}
internal_function void
AddTwoWayBungeeAttachment(entity* A, entity* B, f32 Constant, f32 RestLenght)
{
	AddOneWayBungeeAttachment(A, B, Constant, RestLenght);
	AddOneWayBungeeAttachment(B, A, Constant, RestLenght);
}

internal_function void
AddOneWayRodAttachment(entity* Subject, entity* EndPoint, f32 Lenght)
{
	attachment_info* Info = AddAttachmentRaw(Subject, EndPoint);
	*Info = {};
	Info->Type = AttachmentType_Rod;
	Info->Rod.Lenght = Lenght;
}
internal_function void
AddTwoWayRodAttachment(entity* A, entity* B, f32 Lenght)
{
	AddOneWayRodAttachment(A, B, Lenght);
	AddOneWayRodAttachment(B, A, Lenght);
}

internal_function void
AddOneWayCableAttachment(entity* Subject, entity* EndPoint, f32 Restitution, f32 SlackLenght)
{
	Assert(0 <= Restitution && Restitution <= 1);
	attachment_info* Info = AddAttachmentRaw(Subject, EndPoint);
	*Info = {};
	Info->Type = AttachmentType_Cable;
	Info->Cable.Restitution = Restitution;
	Info->Cable.SlackLenght = SlackLenght;
}
internal_function void
AddTwoWayCableAttachment(entity* A, entity* B, f32 Restitution, f32 SlackLenght)
{
	AddOneWayCableAttachment(A, B, Restitution, SlackLenght);
	AddOneWayCableAttachment(B, A, Restitution, SlackLenght);
}

internal_function void
ApplySpringyForces(entity* Entity)
{
	for(u32 AttachmentIndex = 0;
			AttachmentIndex < ArrayCount(Entity->Attachments);
			AttachmentIndex++)
	{
		entity* EndPoint = Entity->Attachments[AttachmentIndex];
		if(EndPoint)
		{
			attachment_info Info = Entity->AttachmentInfo[AttachmentIndex];

			if(Info.Type == AttachmentType_Spring || Info.Type == AttachmentType_Bungee)
			{
				v3 A_P = Entity->P;
				v3 B_P = EndPoint->P;

				v3 d = A_P - B_P;
				f32 d_m = Lenght(d);
				f32 l0 = Info.Spring.RestLenght;
				f32 dl = (d_m - l0);

				if(Info.Type == AttachmentType_Spring || 
					 dl > 0.0f)
				{
					f32 k = Info.Spring.Constant;
					v3 d_n = Normalize(d);

					//TODO STUDY(bjorn): Why does the springs go out of control without drag?
					Entity->F += (-k*dl) * d_n;
				}
			}

			//TODO IMPORTANT(bjorn): Debug!!!
			if(Info.Type == AttachmentType_Rod || Info.Type == AttachmentType_Cable)
			{
				v3 d = Entity->P - EndPoint->P;
				v3 Vs = Entity->dP - EndPoint->dP;
				f32 d_m = Lenght(d);

				f32 l0 = Info.Cable.SlackLenght;
				f32 c = Info.Cable.Restitution;
				if(Info.Type == AttachmentType_Rod)
				{
					l0 = Info.Rod.Lenght;
					c = 0.0f;
				}

				f32 dl = (d_m - l0);
				v3 d_n = Normalize(d);

				f32 iM_sum = (Entity->iM + EndPoint->iM);
				f32 A_rat = (Entity->iM   / iM_sum);
				f32 B_rat = (EndPoint->iM / iM_sum);
				f32 g = (dl>0? 1 : -1) * (1+c) * Dot(d_n, Vs);

				if(Info.Type == AttachmentType_Rod ||
					 Info.Type == AttachmentType_Cable && dl > 0)
				{
					Entity->dP   += (A_rat * g) * d_n;
					EndPoint->dP -= (B_rat * g) * d_n;
					Entity->P   -= (A_rat * dl) * d_n;
					EndPoint->P += (B_rat * dl) * d_n;
				}
			}
		}
	}
}

#define ATTACHMENT_H
#endif //ATTACHMENT_H_FIRST_PASS
#endif //ATTACHMENT_H
