// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "SHMidiNoteAction.h"

#include "SHVirtualInstrumentLog.h"

#define MAX_FLOAT7 (127.0f)
#define MAX_FLOAT14 (16383.0f)

namespace LBSVirtualInstrument
{
	using namespace Harmonix::Midi::Constants;
	
	FMidiEventParser::FMidiEventParser(const int32 InMinMidiNote, const int32 InNumKeys)
	{
		MinMidiNote = FMath::Max<int32>(GMinNote, InMinMidiNote);
		MaxMidiNote = FMath::Min<int32>(GMaxNote, InMinMidiNote + InNumKeys - 1);
		ResetState();
	}

	void FMidiEventParser::ResetState()
	{
		FMemory::Memset(LastCcVals, -1, 16 * 128);
		
		PendingNoteActions.Empty(16);
		Speed = 1.f;
		CurrentQuarterNote = 0.f;
		CurrentTempoBPM = 120.f;
		
		Volume = 1.f;
		Expression = 1.f;
		bIsSusPedalOn = false;
		SosPedalState = EPedalState::NoChange;
	}

	void FMidiEventParser::NoteOff(FMidiVoiceId InVoiceId, uint8 InMidiNoteNumber, uint8 InMidiChannel)
	{
		NoteOn(InVoiceId, InMidiNoteNumber, FMidiNoteAction::VelocityNoteOff, InMidiChannel);
	}

	void FMidiEventParser::NoteOn(FMidiVoiceId InVoiceId, uint8 InMidiNoteNumber, uint8 InVelocity, uint8 InMidiChannel,
								 int32 InEventTick, int32 InCurrentTick, float InOffsetMs)
	{
		if (InMidiNoteNumber < MinMidiNote || InMidiNoteNumber > MaxMidiNote || InVelocity >= FMidiNoteAction::VelocityIgnore)
			return;
	
		FMidiNoteAction* ExistingAction = PendingNoteActions.FindByPredicate([=](const FMidiNoteAction& Action){ return Action.VoiceId == InVoiceId; });
		
		int8 PendingVelocity = ExistingAction ? ExistingAction->Velocity : 0;
		if (InVelocity > PendingVelocity || InVelocity == FMidiNoteAction::VelocityNoteOff)
		{
			PendingNoteActions.Add(
				{ 
						InMidiNoteNumber,
						InVelocity,
						InEventTick,
						InCurrentTick,
						InOffsetMs,
						0,
						InVoiceId
					  });
		}
	}
	
	void FMidiEventParser::SetHighOrLowControllerByte(EControllerID InController, int8 InValue, int32 InCurrentTick, int8 InMidiChannel)
	{
		LastCcVals[InMidiChannel][static_cast<uint8>(InController)] = InValue;
		int32 HighByteIdx;
		int32 LowByteIdx;
		if (GetMsbLsbIndexes(InController, HighByteIdx, LowByteIdx))
		{
			if (LastCcVals[InMidiChannel][LowByteIdx] == -1)
			{
				Set7BitController(InController, InValue, InMidiChannel);
			}
			else if (LastCcVals[InMidiChannel][HighByteIdx] != -1)
			{
				Set14BitController(static_cast<EControllerID>(HighByteIdx), (LastCcVals[InMidiChannel][HighByteIdx] << 7 | LastCcVals[InMidiChannel][LowByteIdx]), InMidiChannel);
			}
		}
		else
		{
			Set7BitController(InController, InValue, InMidiChannel);
		}
		
	}

	
	bool FMidiEventParser::GetMsbLsbIndexes(EControllerID InControllerId, int& InMsb, int& InLsb)
	{
		uint8 Id = (uint8)InControllerId;
		if (Id < 32)
		{
			InMsb = Id; InLsb = Id + 32;
			return true;
		}
		if (Id < 64)
		{
			InMsb = Id - 32; InLsb = Id;
			return true;
		}
		if (Id == 98 || Id == 100)
		{
			InMsb = Id + 1; InLsb = Id;
			return true;
		}
		if (Id == 99 || Id == 101)
		{
			InMsb = Id; InLsb = Id - 1;
			return true;
		}
		return false;
	}

	uint8 FMidiEventParser::GetClamp7BitValue(int32 InValue)
	{
		return static_cast<uint8>(FMath::Clamp(InValue, 0, 127));
	}

	void FMidiEventParser::SetSpeed(float InSpeed)
	{
		Speed = FMath::Max(0.0001f, InSpeed);
	}

	void FMidiEventParser::SetTempo(float InBPM)
	{
		if (CurrentTempoBPM == InBPM)		
			return;		

		CurrentTempoBPM = InBPM;
	}

	void FMidiEventParser::SetQuarterNote(float QuarterNote)
	{
		CurrentQuarterNote = QuarterNote;
	}

	EPedalState FMidiEventParser::GetAndValidateSosPedal()
	{
		EPedalState PedalState = SosPedalState;
		SosPedalState = EPedalState::NoChange;
		return PedalState;
	}

	void FMidiEventParser::Set7BitController(EControllerID InController, int8 InValue, int8 InMidiChannel)
	{
		LastCcVals[InMidiChannel][static_cast<uint8>(InController)] = InValue;
		float ValueP = Get7BitControllerValue(InController, InValue, InMidiChannel);
		SetController(InController, ValueP);
	}

	float FMidiEventParser::Get7BitControllerValue(EControllerID InController, int8 InValue, int8 InMidiChannel)
	{
		float ValueP = 0.0f;
		
		switch (InController)
		{
		case EControllerID::Volume:
		case EControllerID::Expression: 
			{ 
				ValueP = Midi7BitToFloat(InValue); 
				break; 
			}
		case EControllerID::Hold:
		case EControllerID::Sustenuto:
		case EControllerID::SoftPedal:
			{
				ValueP = InValue;
				break;
			}
		default:
			break;
		}
		
		return ValueP;
	}

	void FMidiEventParser::Set14BitController(EControllerID InController, int16 InValue, int8 InMidiChannel)
	{
		int32 LsbIdx;
		int32 MsbIdx;
		if (GetMsbLsbIndexes(InController, MsbIdx, LsbIdx))
		{
			LastCcVals[InMidiChannel][MsbIdx] = (InValue >> 7) & 0x7F;
			LastCcVals[InMidiChannel][LsbIdx] = InValue & 0x7F;
			float ValueP = Get14BitControllerValue(InController, InValue, InMidiChannel);
			SetController(InController, ValueP);
		}
	}

	float FMidiEventParser::Get14BitControllerValue(EControllerID InController, int16 InValue, int8 InMidiChannel)
	{
		float ValueP = 0.f;
		
		switch (InController)
		{
		case EControllerID::Volume: 
		case EControllerID::Expression: 
			{ 
				ValueP = Midi14BitToFloat(InValue);
				break; 
			}
		case EControllerID::Hold:
		case EControllerID::Sustenuto:
		case EControllerID::SoftPedal:
			{
				ValueP = InValue;
				break;
			}
			
		default:
			break;
		}
		
		return ValueP;
	}

	void FMidiEventParser::SetController(EControllerID InController, float InValue)
	{
		switch (InController)
		{
		case EControllerID::Volume: 
			{ 
				Volume = InValue; 
				break;
			}
		case EControllerID::Expression: 
			{ 
				Expression = InValue; 
				break;
			}
		case EControllerID::Hold: 
			{
				bIsSusPedalOn = MidiControlToBool(InValue);
				break; 
			}
		case EControllerID::Sustenuto: 
			{
				if(MidiControlToBool(InValue))
					SosPedalState = EPedalState::TriggerOn;
				else
					SosPedalState = EPedalState::TriggerOff;
				break; 
			}
		default: 
			break;
		}
	}

	void FMidiEventParser::ProcessNoteActions(int32 InNumFrames, TMap<FMidiVoiceId, FMidiNoteAction>& OutNotesOn, TArray<FMidiVoiceId>& OutNotesOff)
	{
		const int32 NumPending = PendingNoteActions.Num();
		if (NumPending == 0)
			return;
		
		OutNotesOn.Empty(NumPending);
		OutNotesOff.Empty(NumPending);
		int32 CurrentNoteIndex = 0;
		for (FMidiNoteAction& Action : PendingNoteActions)
		{
			// UE v5.4.3 + Harmonix v1.0: Frame offset isn't used in Harmonix API so this code isn't needed
			// Should recheck this in later version of Harmonix
			// if (Action.FrameOffset > 0)
			// {
			// 	Action.FrameOffset -= InNumFrames;
			// 	if (Action.FrameOffset < 0)
			// 	{
			// 		Action.FrameOffset = 0;
			// 	}
			// }
			
			if(Action.Velocity > FMidiNoteAction::VelocityNoteOff)
			{
				// If this note's ON event is cancelled by its subsequent Off event, ignore this event 
				bool bIsNotCancel = true;
				for(int i = NumPending - 1; i > CurrentNoteIndex; i--)
				{
					if(PendingNoteActions[i].VoiceId == Action.VoiceId && PendingNoteActions[i].Velocity == FMidiNoteAction::VelocityNoteOff)
					{
						bIsNotCancel = false;
						break;
					}
				}
				if(bIsNotCancel)
					OutNotesOn.Emplace(Action.VoiceId, Action);
			}
			else
			{
				OutNotesOff.AddUnique(Action.VoiceId);
			}
				
			CurrentNoteIndex++;
		}

		// Use this if use Frame Offset
		// ResetNoteActions(false);
		PendingNoteActions.Reset();
	}

	void FMidiEventParser::ResetNoteActions(bool ClearNotes)
	{
		if (ClearNotes)
		{
			PendingNoteActions.Reset();
			return;
		}

		PendingNoteActions.RemoveAll([](const FMidiNoteAction& Action){ return Action.FrameOffset <= 0; });
	}
	
	float FMidiEventParser::Midi7BitToFloat(int8 Level)
	{
		return Level / MAX_FLOAT7;
	}

	float FMidiEventParser::Midi14BitToFloat(int32 Level)
	{
		return Level / MAX_FLOAT14;
	}

	bool FMidiEventParser::MidiControlToBool(float InValue)
	{
		return InValue > 63.f;
	}
	
}

#undef MAX_FLOAT7
#undef MAX_FLOAT14 