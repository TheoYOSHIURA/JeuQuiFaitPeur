// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once

#include "ModalSynth.h"
#include "PianoKeySynth.h"
#include "PianoModel.h"
#include "Extend\RCurveExtend.h"
#include "SoundBoardSynth.h"
#include "SHMidiNoteAction.h"

namespace LBSVirtualInstrument
{
	using namespace Audio;

	struct FHammerState
	{
		int32 CurrentIndex;
		float Strength;
		
		FHammerState(float InStrength) : Strength(InStrength)
		{
			CurrentIndex = 0;
		}
	};

	struct FPianoSynthParams
	{
	public:
		float SystemGain;
		float VelocityScale;
		float KeyGain;
		float HammerGain;
		float DynamicAdjust;
		float SoundboardGain;
		float SymResonScale;
		bool IsSusPedalOn;
		EPedalState SosPedal;

		FPianoSynthParams(float InSystemGain, float InVelocityScale, float InKeyGain,
						  float InHammerGain, float InDynamicAdjust, float InSoundboardGain,
						  float InSymResonScale, bool InIsSusPedalOn, EPedalState InSosPedal)
		: SystemGain(InSystemGain), VelocityScale(InVelocityScale), KeyGain(InKeyGain),
		  HammerGain(InHammerGain), DynamicAdjust(InDynamicAdjust), SoundboardGain(InSoundboardGain),
		  SymResonScale(InSymResonScale), IsSusPedalOn(InIsSusPedalOn), SosPedal(InSosPedal)
		{
			
		}
	};
	
	class SHVIRTUALINSTRUMENT_API FPianoSynth
	{
		
	public:
		FPianoSynth(const FPianoModelAssetProxyPtr& InPianoModel, const float InSamplingRate,
		            const float SoundboardGain, const int32 SoundBoardQualityDown,
		            const float InKeyInitDelay=.005f, const float HammerDuration=.25f);

		virtual ~FPianoSynth() = default;

		void SetVelocityRemapCurve(const FRCurveExtendAssetProxyPtr& InCurve);
		void SetNoteGainCurve(const FRCurveExtendAssetProxyPtr& InCurve);
		void SetHammerGainCurve(const FRCurveExtendAssetProxyPtr& InCurve);
		
		void Synthesize(TArrayView<float>& OutAudio,
						TMap<FMidiVoiceId, FMidiNoteAction>& NotesOn,
						TArray<FMidiVoiceId>& NotesOff,
						const FPianoSynthParams& SynthParams,
						float InCurrentTimeMs);

		void OffAllNotes();
		void KillAllNotes();

		bool IsNotRunning() const;
		
	protected:
		void InitAttackBuffer();
		
		void UpdateNotesMaps(TMap<FMidiVoiceId, FMidiNoteAction>& NotesOn, TArray<FMidiVoiceId>& NotesOff,
							 const FPianoSynthParams& SynthParams);
		void UpdateAllMapIfSosPedalChanged(TMap<FMidiVoiceId, FMidiNoteAction>& NotesOn, TArray<FMidiVoiceId>& NotesOff, const FPianoSynthParams& SynthParams);
		void UpdateNotesOffMap(TArray<FMidiVoiceId>& NotesOff);
		void UpdateNotesOnMap( TMap<FMidiVoiceId, FMidiNoteAction>& NotesOn, const FPianoSynthParams& SynthParams);
		void AdjustDynamicRangeIfNeeded(const FPianoSynthParams& SynthParams);
		
		void SynthHammerIfNeeded(TArrayView<float>& OutAudio);

		void SynthNotesOff(TArrayView<float>& OutAudio, bool IsSusPedalOn);
		
		void SynthNoteOnWithSusPedalOn(TArrayView<float>& OutAudio);
		void SynthNoteOnWithSusPedalOff(TArrayView<float>& OutAudio,  bool bIsNewNoteOnTrigger);

		FORCEINLINE void SynthNoteOnFull(TArrayView<float>& OutAudio, TSharedPtr<FPianoKeySynth>& KeySynth, bool IsSusPedalOn);
		
		FORCEINLINE float GetKeyHammerVelocity(const TSharedPtr<FPianoKeySynth>& KeySynth) const;

		FORCEINLINE bool IsHammerSynthFinished() const;

		FORCEINLINE float GetNoteGainValueFromCurve(const uint8 MidiNote) const;
		
	private:
		float SamplingRate;
		
		TMap<FMidiVoiceId, TSharedPtr<FPianoKeySynth>> NoteOnMap;
		TMap<FMidiVoiceId, TSharedPtr<FPianoKeySynth>> NoteOffMap;
		TMap<FMidiVoiceId, bool> SosPedalSnapshot;
		
		TArray<FHammerState> HammerQueue;
		TUniquePtr<LBSImpactSFXSynth::FModalSynth> HammerSynth;
		int32 CurrentHammerBufferIndex;
		FAlignedFloatBuffer HammerBuffer;
		float KeyInitDelay;
		
		TUniquePtr<FSoundBoardSynth> SoundboardSynth;

		FAlignedFloatBuffer AttackCurveBuffer;
		static TArray<float> AttackCurvePreSample;
		static constexpr float AttackCurveSamplingRate = 1000.f;
		static constexpr float HammerReScale = 0.002f;

		TArray<FAlignedFloatBuffer> SecondStageBuffers; 
		TArray<TSharedPtr<FPianoKeySynth>> SecondStageSynths;

		float CurrentSecond;

		FPianoModelAssetProxyPtr PianoModel;
		FRCurveExtendAssetProxyPtr VelocityRemapCurve;
		FRCurveExtendAssetProxyPtr NoteGainCurve;
		FRCurveExtendAssetProxyPtr HammerGainCurve;
	};
}
