#if !defined(ATTACHMENT_H)
#if !defined(ATTACHMENT_H_FIRST_PASS)

enum attachment_type
{
	AttachmentType_Spring,
	AttachmentType_Bungee,
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

					Entity->F += (-k*dl) * d_n;
				}
			}
		}
	}
}

#define ATTACHMENT_H
#endif //ATTACHMENT_H_FIRST_PASS
#endif //ATTACHMENT_H
