// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once
#include "HarmonixMidi/MidiVoiceId.h"
#include "HarmonixMidi/MidiConstants.h"

namespace LBSVirtualInstrument
{
	enum class EPedalState : uint8
	{
		TriggerOn,
		TriggerOff,
		NoChange
	};
	
	struct SHVIRTUALINSTRUMENT_API FMidiNoteAction
	{
	public:
		static constexpr uint8 VelocityIgnore = 128;
		static constexpr uint8 VelocityNoteOff = 0;
		
		uint8 MidiNote = 0;
		uint8 Velocity = 0;
		int32 EventTick = 0;
		int32 TriggerTick = 0;
		float OffsetMs = 0.0f;
		int32 FrameOffset = 0;
		FMidiVoiceId VoiceId;

		FMidiNoteAction(uint8 InMidiNoteNumber, uint8 InVelocity, int32 InEventTick,
					    int32 InTriggerTick, float InOffsetMs, int32 InFrameOffset,
					    const FMidiVoiceId& InMidiVoiceId) :
		MidiNote(InMidiNoteNumber),
		Velocity(InVelocity),
		EventTick(InEventTick),
		TriggerTick(InTriggerTick),
		OffsetMs(InOffsetMs),
		FrameOffset(InFrameOffset),
		VoiceId(InMidiVoiceId)
		{
			
		}
	};

	class SHVIRTUALINSTRUMENT_API FMidiEventParser 
	{
	public:
		static bool GetMsbLsbIndexes(Harmonix::Midi::Constants::EControllerID InControllerId, int& InMsb, int& InLsb);
		static uint8 GetClamp7BitValue(int32 InValue);
		
	public:
		FMidiEventParser(const int32 InMinMidiNote, const int32 InNumKeys);
		virtual ~FMidiEventParser() = default;
		
		void ResetState();
		
		void NoteOff(FMidiVoiceId InVoiceId, uint8 InMidiNoteNumber, uint8 InMidiChannel = 0);
		void NoteOn(FMidiVoiceId InVoiceId, uint8 InMidiNoteNumber, uint8 InVelocity, uint8 InMidiChannel = 0, int32 InEventTick = 0, int32 InCurrentTick = 0, float InOffsetMs = 0.0f);
 
		void SetHighOrLowControllerByte(Harmonix::Midi::Constants::EControllerID InController, int8 InValue, int32 InCurrentTick, int8 InMidiChannel);
		
		void ProcessNoteActions(int32 InNumFrames, TMap<FMidiVoiceId, FMidiNoteAction>& OutNotesOn, TArray<FMidiVoiceId>& OutNotesOff);
		void ResetNoteActions(bool ClearNotes);
		
		void SetSpeed(float Speed);                
		void SetTempo(float BPM);
		void SetQuarterNote(float QuarterNote);

		float GetSpeed() const { return Speed; }
		float GetBeat() const { return CurrentQuarterNote; }
		
		FORCEINLINE float Midi7BitToFloat(int8 Level);
		FORCEINLINE float Midi14BitToFloat(int32 Level);
		FORCEINLINE bool MidiControlToBool(float InValue);

		bool GetIsSusPedalOn() const { return bIsSusPedalOn; }

		/// Get the current state of Sostenuto pedal then change it to hold to avoid re triggering again.
		/// @return State of Sostenuto
		EPedalState GetAndValidateSosPedal();

		float GetVolumeWithExpression() const { return Volume * Expression; }
		float GetVolume() const { return Volume; }
		float GetExpression() const { return Expression; }
		int32 GetNumPendingNoteActions() const { return PendingNoteActions.Num(); }
	protected:
		void Set7BitController(Harmonix::Midi::Constants::EControllerID InController, int8 InValue, int8 InMidiChannel = 0);
		virtual float Get7BitControllerValue(Harmonix::Midi::Constants::EControllerID InController, int8 InValue, int8 InMidiChannel = 0);
		
		void Set14BitController(Harmonix::Midi::Constants::EControllerID InController, int16 InValue, int8 InMidiChannel = 0);
		virtual float Get14BitControllerValue(Harmonix::Midi::Constants::EControllerID InController, int16 InValue, int8 InMidiChannel = 0);
		
		virtual void SetController(Harmonix::Midi::Constants::EControllerID InController, float InValue);
		
	protected:
		TArray<FMidiNoteAction> PendingNoteActions;
		int32 MinMidiNote;
		int32 MaxMidiNote;
		
		float Speed;
		float CurrentQuarterNote;
		float CurrentTempoBPM;

		float Volume;
		float Expression;
		bool bIsSusPedalOn;
		EPedalState SosPedalState;
		
		int8 LastCcVals[16][128];
	};
}
