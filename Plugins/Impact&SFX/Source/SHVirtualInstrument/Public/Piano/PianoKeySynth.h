// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once

#include "PianoKeyObj.h"
#include "DSP/Dsp.h"
#include "DSP/BufferVectorOperations.h"

namespace LBSVirtualInstrument
{
	using namespace Audio;
	
	class SHVIRTUALINSTRUMENT_API FPianoKeySynth
	{
		
	public:
		static constexpr float FreqDiv = 1000.f;
		  
		FPianoKeySynth(const uint8 InMidiNote, UPianoKeyData* InKeyData, const float InSamplingRate,
		               const float InVelocity, const bool IsSusPedalOn, const float InSymResonScale, const float InVelocityStandard,
		               const float InInitDelay, const float InKeyGain, const TArrayView<const float>& InAttackView,
		               const float InNoteOffDecayDelta);

		/// Use this function when a key is struck again while it's still playing
		/// @param InVelocity Velocity of the MIDI event
		/// @param InKeyGain Key Gain
		/// @param IsSusPedalOn Is sustain pedal on
		/// @param InSymResonScale Sympathetic Resonance Scale
		void ReStrike( const float InVelocity, const float InKeyGain, const bool IsSusPedalOn, const float InSymResonScale);

		void Synthesize(TArrayView<float>& OutAudio, const bool IsNoteOn, const bool IsSusPedalOn, const bool bFullSynth);

		void SynthesizeSecondStageAndSymReson(TArrayView<float>& OutAudio, const TArrayView<const float>& InResonAudio, const bool bNewNoteOnTrigger);
		
		virtual ~FPianoKeySynth() = default;
		
		bool IsNoteOn() const { return bIsNoteOn; }
		float GetVelocity() const { return Velocity; }
		float GetHammerVelScale() const { return HammerFreqVelScale; }
		bool IsFirstStageRunning() const { return CurrentNumModalStage1 > 0; }
		bool IsSecondStageRunning() const { return CurrentNumModalStage2 > 0; }
		float GetCurrentSampleIndex() const { return CurrentSampleIndex; }
		bool HasDamper() const { return bHasDamper; }
		float GetSymResonScale() const { return SymResonScale; }
		uint8 GetMidiNote() const { return MidiNote; }

		/// Rough estimation of energy. Not 100% match to true value when synthesizing.
		/// @return Base energy
		float EstimateCurrentEnergy() const;

		void AdjustEnergy(const float InValue);
		
	protected:
		void ResetState(const float InVelocity);
		void SetBufferSize();
		void InitBuffers();
		
		void InitReStrikeFadeOutIfNeeded(float InVelocity);
		void CopyToReStrikeBuffers(const int32 NumModals, int32& OutCurrentIndex,
								   const float* TwoDecayCosBuffer,const float* RSqBuffer,
								   const float* D1Buffer, const float* D2Buffer);
 
		static float GetCurrentDelayBufferEnergy(const int32 NumModals, const float* L1Buffer, const float* L2Buffer);

		float GetVelocityScaleDelta(const float BaseFreqScale) const;

		void StartSynthesizing(TArrayView<float>& OutAudio, const bool IsNoteOn, const bool IsSusPedalOn, const bool bFullSynth);

		void ChangeParamsIfNeeded(bool IsNoteOn, bool IsSusPedalOn);

		void SynthLastStrikeFadeOut(TArrayView<float>& OutAudio);

		void SynthesizeAllStages(TArrayView<float>& OutBuffer, const int32 EndSample);
		
		void SynthesizeOneStage(TArrayView<float>& OutBuffer, const int32 EndSample, const int32 NumModals,
		                        const float* TwoRCosData, const float* R2Data,
		                        float* OutL1BufferPtr, float* OutL2BufferPtr);
		
		FORCEINLINE static void ProcessVectorModal(int32 NumModals, const float* TwoRCosData, const float* R2Data, float* OutL1BufferPtr,
											       float* OutL2BufferPtr, VectorRegister4Float& SumModalVector);
		 
		FORCEINLINE static void ProcessVectorModalWithForce(int32 NumModals, const float* TwoRCosData, const float* R2Data, const float* GainFData,
						                                    float* OutL1BufferPtr, float* OutL2BufferPtr, VectorRegister4Float ForceReg,
						                                    VectorRegister4Float& SumModalVector);

		void StartSynthesizeSecondStageSymReson(TArrayView<float>& OutBuffer, const TArrayView<const float>& InResonAudio, const int32 EndSample);
		
		FORCEINLINE static float ClampFreqVelScale(float FreqVelScale);
		
	private:
		UPianoKeyData* KeyData;
		
		uint8 MidiNote;
		float NoteOffDecayDelta;
		bool bHasDamper;
		float SamplingRate;
		float TimeStep;
		float Velocity;
		bool bIsSusPedalOn;
		bool bIsInitSusPedalOn;
		float SymResonScale;
		float KeyGain;
		
		float VelocityStandard;
		int32 NumInitDelay;
		float HammerFreqVelScale;
		
		int32 NumUsedParams;
		int32 CurrentNumModalStage1;
		int32 CurrentNumModalStage2;
		int32 CurrentSampleIndex;
		bool bIsSecondDecay;
		bool bIsNoteOn;
		
		bool bIsReStrikeFadeOut;
		float OldVelocity;
		int32 ReStrikeAtSampleIndex;
		float ReStrikeVelocityScale;
		
		int32 CurrentBufferStartSample;
		
		FAlignedFloatBuffer SymResonAmpBuffer;
		FAlignedFloatBuffer TwoDecayCosBuffer1;
		FAlignedFloatBuffer RSqBuffer1;
		FAlignedFloatBuffer D1Buffer1;
		FAlignedFloatBuffer D2Buffer1;
		
		FAlignedFloatBuffer TwoDecayCosBuffer2;
		FAlignedFloatBuffer RSqBuffer2;
		FAlignedFloatBuffer SymResonGainBuffer2;
		FAlignedFloatBuffer D1Buffer2;
		FAlignedFloatBuffer D2Buffer2;

		FAlignedFloatBuffer ReStrikeTwoDecayCosBuffer;
		FAlignedFloatBuffer ReStrikeRSqBuffer;
		FAlignedFloatBuffer ReStrikeD1Buffer;
		FAlignedFloatBuffer ReStrikeD2Buffer;
		int32 NumReStrikeFadeOutModal;

		TArrayView<const float> AttackView;

		float AllFreqAmpAbs;
		float AvgFreqDecay;
	};
}
