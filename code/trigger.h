#if !defined(TRIGGER_H)

#if !defined(TRIGGER_H_FIRST_PASS)
struct trigger_state
{
	f32 TimePassed;
	//TODO Maybe Buddy implies WasTriggerd.
	b32 WasTriggerd;
	entity_reference Buddy;
};

#define TRIGGER_H_FIRST_PASS
#else

struct trigger_state_result
{
	b32 OnEnter;
	b32 OnLeave;
};

internal_function void
SetTriggerStateRaw(trigger_state* TrigState, trigger_state* Ref)
{
	Assert(Ref);
	if(TrigState && TrigState != Ref)
	{
		TrigState->WasTriggerd = Ref->WasTriggerd;
		TrigState->TimePassed = Ref->TimePassed;
		TrigState->WasTriggerd = Ref->WasTriggerd;
		if(!Ref->Buddy.Ptr) { TrigState->Buddy.Ptr = 0; }
	}
}

internal_function trigger_state*
GetTriggerStateRaw(entity* Entity, entity* Buddy)
{
	Assert(Entity);
	Assert(Buddy);
	Assert(Entity != Buddy);
	trigger_state* Result = 0;

	trigger_state* TrigState = Entity->TrigStates;
	for(u32 TrigStateIndex = 0;
			TrigStateIndex < ArrayCount(Entity->TrigStates);
			TrigStateIndex++, TrigState++)
	{
		if(TrigState->Buddy.Ptr == Buddy)
		{
			Result = TrigState;
		}
	}

	return Result;
}

internal_function trigger_state* 
GetTriggerStateSlotForOverwrite(entity* Entity)
{
	trigger_state* Result = Entity->TrigStates + Entity->OldestTrigStateIndex;

	Entity->OldestTrigStateIndex = ((Entity->OldestTrigStateIndex + 1) & 
																	(ArrayCount(Entity->TrigStates) - 1));

	return Result;
}

	internal_function trigger_state_result
UpdateAndGetCurrentTriggerState(entity* A, entity* B, 
																f32 dT, b32 IsInside, f32 TimeUntilRetrigger = 0.0f)
{
	Assert(A);
	Assert(B);
	trigger_state_result Result = {};

	trigger_state* TrigStateA = GetTriggerStateRaw(A, B);
	trigger_state* TrigStateB = GetTriggerStateRaw(B, A);

	if(TrigStateA || TrigStateB)
	{
		trigger_state* TrigState = TrigStateA ? TrigStateA : TrigStateB;

		b32 Exited = TrigState->WasTriggerd && !IsInside;
		TrigState->WasTriggerd = IsInside;

		f32 PrevTimePassed = TrigState->TimePassed;
		if(PrevTimePassed > 0) { TrigState->TimePassed -= dT; }

		if(TrigState->TimePassed <= 0 && 
			 (
				(PrevTimePassed <= 0 && Exited) ||
				(PrevTimePassed > 0)
			 )
			)
		{
			TrigState->Buddy.Ptr = 0;
		}

		if(Exited)
		{
			Result.OnLeave = true;
		}

		SetTriggerStateRaw(TrigStateA, TrigState);
		SetTriggerStateRaw(TrigStateA, TrigState);
	}
	else if(IsInside)
	{
		Result.OnEnter = true;

		trigger_state* EmptyASlot = GetTriggerStateSlotForOverwrite(A);
		Assert(EmptyASlot);
		EmptyASlot->WasTriggerd = true;
		EmptyASlot->TimePassed = TimeUntilRetrigger;
		EmptyASlot->Buddy.Ptr = B;

		trigger_state* EmptyBSlot = GetTriggerStateSlotForOverwrite(B);
		Assert(EmptyBSlot);
		EmptyBSlot->TimePassed = TimeUntilRetrigger;
		EmptyBSlot->WasTriggerd = true;
		EmptyBSlot->Buddy.Ptr = A;
	}

	return Result;
}
#define TRIGGER_H
#endif

#endif
