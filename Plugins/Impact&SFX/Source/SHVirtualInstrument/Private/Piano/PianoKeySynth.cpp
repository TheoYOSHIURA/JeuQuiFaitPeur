// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "Piano/PianoKeySynth.h"

#include "SHVirtualInstrumentLog.h"
#include "DSP/AlignedBuffer.h"
#include "DSP/FloatArrayMath.h"

#define STRENGTH_MIN (5e-7f)
#define RESTRIKE_DECAY_SCALE (0.99f)
#define RESTRIKE_DECAY_SCALE_SQ (RESTRIKE_DECAY_SCALE * RESTRIKE_DECAY_SCALE)

namespace LBSVirtualInstrument
{
	FPianoKeySynth::FPianoKeySynth(const uint8 InMidiNote, UPianoKeyData* InKeyData, const float InSamplingRate,
	                               const float InVelocity, const bool IsSusPedalOn, const float InSymResonScale,
	                               const float InVelocityStandard, const float InInitDelay, const float InKeyGain, 
	                               const TArrayView<const float>& InAttackView, const float InNoteOffDecayDelta) :
	MidiNote(InMidiNote),
	NoteOffDecayDelta(InNoteOffDecayDelta),
    SamplingRate(InSamplingRate),
	bIsSusPedalOn(IsSusPedalOn),
	bIsInitSusPedalOn(IsSusPedalOn),
	SymResonScale(InSymResonScale),
	VelocityStandard(InVelocityStandard)
	{
		TimeStep = 1.f / InSamplingRate;
		CurrentBufferStartSample = 0;
		bHasDamper = InKeyData->HasDamper();
		NumInitDelay = FMath::Max(0, FMath::CeilToInt32(SamplingRate * InInitDelay));
		Velocity = 0.f; //Always make sure velocity is init

		AttackView = InAttackView;
		bIsReStrikeFadeOut = false;
		OldVelocity = 0.f;
		ReStrikeAtSampleIndex = 0.f;
		NumReStrikeFadeOutModal = 0;
		KeyData = InKeyData;
		ReStrikeVelocityScale = 1.f;
		KeyGain = InKeyGain;
		AllFreqAmpAbs = 0.f;
		AvgFreqDecay = 0.f;
		
		SetBufferSize();
		ResetState(InVelocity);
		InitBuffers();
	}

	void FPianoKeySynth::ReStrike(const float InVelocity, const float InKeyGain, const bool IsSusPedalOn, const float InSymResonScale)
	{
		bIsSusPedalOn = IsSusPedalOn;
		bIsInitSusPedalOn = IsSusPedalOn;
		KeyGain = InKeyGain;
		SymResonScale = InSymResonScale;
		
		InitReStrikeFadeOutIfNeeded(InVelocity);
		ResetState(InVelocity);
		InitBuffers();		
	}

	float FPianoKeySynth::EstimateCurrentEnergy() const
	{
		return AllFreqAmpAbs * FMath::Exp(-AvgFreqDecay * CurrentSampleIndex * TimeStep);
	}

	void FPianoKeySynth::AdjustEnergy(const float InValue)
	{
		Audio::ArrayMultiplyByConstantInPlace(D1Buffer1, InValue);
		Audio::ArrayMultiplyByConstantInPlace(D2Buffer1, InValue);
		Audio::ArrayMultiplyByConstantInPlace(D1Buffer2, InValue);
		Audio::ArrayMultiplyByConstantInPlace(D2Buffer2, InValue);
		AllFreqAmpAbs = AllFreqAmpAbs * InValue;
	}

	void FPianoKeySynth::ResetState(const float InVelocity)
	{
		Velocity = InVelocity;
		CurrentSampleIndex = 0;
		bIsSecondDecay = false;
		bIsNoteOn = true;

		CurrentNumModalStage1 = TwoDecayCosBuffer1.Num();
		CurrentNumModalStage2 = TwoDecayCosBuffer2.Num();
	}

	void FPianoKeySynth::SetBufferSize()
	{
		CurrentNumModalStage1 = LBSImpactSFXSynth::FitToAudioRegister(KeyData->GetNumModals());
		const int32 ClearSize = CurrentNumModalStage1 * sizeof(float);
		SymResonAmpBuffer.SetNumUninitialized(CurrentNumModalStage1);
		FMemory::Memzero(SymResonAmpBuffer.GetData(), ClearSize);
		TwoDecayCosBuffer1.SetNumUninitialized(CurrentNumModalStage1);
		FMemory::Memzero(TwoDecayCosBuffer1.GetData(), ClearSize);
		RSqBuffer1.SetNumUninitialized(CurrentNumModalStage1);
		FMemory::Memzero(RSqBuffer1.GetData(), ClearSize);
		D1Buffer1.SetNumUninitialized(CurrentNumModalStage1);
		FMemory::Memzero(D1Buffer1.GetData(), ClearSize);
		D2Buffer1.SetNumUninitialized(CurrentNumModalStage1);
		FMemory::Memzero(D2Buffer1.GetData(), ClearSize);

		CurrentNumModalStage2 = LBSImpactSFXSynth::FitToAudioRegister(KeyData->GetNumBeats());
		const int32 ClearSize2 = CurrentNumModalStage2 * sizeof(float);
		TwoDecayCosBuffer2.SetNumUninitialized(CurrentNumModalStage2);
		FMemory::Memzero(TwoDecayCosBuffer2.GetData(), ClearSize2);
		RSqBuffer2.SetNumUninitialized(CurrentNumModalStage2);
		FMemory::Memzero(RSqBuffer2.GetData(), ClearSize2);
		SymResonGainBuffer2.SetNumUninitialized(CurrentNumModalStage2);
		FMemory::Memzero(SymResonGainBuffer2.GetData(), ClearSize2);
		D1Buffer2.SetNumUninitialized(CurrentNumModalStage2);
		FMemory::Memzero(D1Buffer2.GetData(), ClearSize2);
		D2Buffer2.SetNumUninitialized(CurrentNumModalStage2);
		FMemory::Memzero(D2Buffer2.GetData(), ClearSize2);
	}
	
	void FPianoKeySynth::InitBuffers()
	{
		TArrayView<const float> ModalsParams;
		float BeatingScale;
		if(bIsInitSusPedalOn)
		{
			ModalsParams = KeyData->GetPedalOnParams();
			BeatingScale = KeyData->GetBeatingScalePedalOn();
		}
		else
		{
			ModalsParams = KeyData->GetPedalOffParams();
			BeatingScale = KeyData->GetBeatingScalePedalOff();
		}
		
		NumUsedParams = UPianoKeyObj::NumParamPerModal * KeyData->GetNumModals();
		check(ModalsParams.Num() == NumUsedParams)
		
		const float BaseFreq = KeyData->GetBaseFreq();
		const float BaseFreqScale = BaseFreq / FreqDiv;
		
		const float VelScaleDelta = GetVelocityScaleDelta(BaseFreqScale);
		const float VelScale = Velocity / VelocityStandard;
		
		float BaseFreqVelScale = FMath::Max(Velocity / 500.f, VelScale * FMath::Pow(10.f, BaseFreqScale * VelScaleDelta));
		BaseFreqVelScale = ClampFreqVelScale(BaseFreqVelScale);
		
		const float FreqScaleThreshold = BaseFreq + 1.f;
		const float SecondDecay = KeyData->GetSecondDecay();
		const float PhiTime = UE_TWO_PI * TimeStep;
		
		ReStrikeVelocityScale = 1.f;
		if(bIsReStrikeFadeOut && OldVelocity > 0.f && Velocity > 0.f)
		{
			const float DecayVelocity = FMath::Exp(-ModalsParams[1] * ReStrikeAtSampleIndex * TimeStep) * OldVelocity;
			const float MaxVelocity = FMath::Max(DecayVelocity, Velocity);
			const float MaxVelocityIncrement = FMath::Min(1.5f, (DecayVelocity + Velocity) / MaxVelocity);
			ReStrikeVelocityScale = (MaxVelocityIncrement * MaxVelocity - DecayVelocity) / Velocity;
			OldVelocity = FMath::Max(DecayVelocity, Velocity);
		}
		else
			OldVelocity = Velocity;
		
		HammerFreqVelScale = Velocity < VelocityStandard ? FMath::Max(0.2f, BaseFreqVelScale * ReStrikeVelocityScale) : 1.f;
		
		AllFreqAmpAbs = 0.f;
		AvgFreqDecay = 0.f;
		for(int i = 0, j = 0, k = 0; i < NumUsedParams; i++, j++)
		{
			float Amp = KeyGain * ModalsParams[i];
			i++;
			const float DecayBase = ModalsParams[i];
			const float Decay = DecayBase + SecondDecay;
			i++;
			const float Freq =  ModalsParams[i];
			i++;

			SymResonAmpBuffer[j] = Amp; //Sym Reson gain doesn't depend on the key velocity
			if(Freq > FreqScaleThreshold)
			{
				const float FreqVelScale = (VelScale * FMath::Pow(10.f, Freq / FreqDiv * VelScaleDelta));
				Amp = Amp * ClampFreqVelScale(FreqVelScale);
			}
			else
				Amp = Amp * BaseFreqVelScale; 

			if(DecayBase < 20.f)
			{
				const float AbsAmp = FMath::Abs(Amp); 
				AllFreqAmpAbs += FMath::Abs(Amp);
				AvgFreqDecay += AbsAmp * Decay;
			}
			
			const float Angle1 = PhiTime * Freq;
			const float DecayRate1 = FMath::Exp(-Decay * TimeStep);
			TwoDecayCosBuffer1[j] = 2.f * DecayRate1 * FMath::Cos(Angle1);
			RSqBuffer1[j] = DecayRate1 * DecayRate1;
			const float NewValue = Amp * DecayRate1 * FMath::Sin(Angle1);
			
			const float OldValue1 = D1Buffer1[j];
			D1Buffer1[j] = OldValue1 * TwoDecayCosBuffer1[j] - RSqBuffer1[j] * D2Buffer1[j] + NewValue * ReStrikeVelocityScale;
			D2Buffer1[j] = OldValue1;

			const float DeltaBeatFreq = ModalsParams[i];
			if(DeltaBeatFreq != 0)
			{
				const float BeatFreq = Freq + DeltaBeatFreq;
				const float Angle2 = PhiTime * BeatFreq;
				const float DecayRate2 = FMath::Exp(-Decay * TimeStep / BeatingScale);
				TwoDecayCosBuffer2[k] = 2.f * DecayRate2 * FMath::Cos(Angle2);
				RSqBuffer2[k] = DecayRate2 * DecayRate2;
				const float DecaySin = DecayRate2 * FMath::Sin(Angle2);
				SymResonGainBuffer2[k] = SymResonAmpBuffer[j] * DecaySin * SymResonScale;
			
				const float ForceGain = Amp * DecaySin / BeatingScale;
				const float OldValue2 = D1Buffer2[k];
				D1Buffer2[k] = OldValue2 * TwoDecayCosBuffer2[k] - RSqBuffer2[k] * D2Buffer2[k] + ForceGain * ReStrikeVelocityScale;
				D2Buffer2[k] = OldValue2;

				k++;
			}
		}
		
		AvgFreqDecay = AvgFreqDecay / AllFreqAmpAbs;
	}

	void FPianoKeySynth::InitReStrikeFadeOutIfNeeded(float InVelocity)
	{
		bIsReStrikeFadeOut = false;
		ReStrikeAtSampleIndex = CurrentSampleIndex - NumInitDelay;
		
		if(Velocity == 0.f || InVelocity == 0.f)
		{
			UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoKeySynth::InitReStrikeFadeOutIfNeeded:: Note is init with zero velocity!"));
			return;
		}

		const int32 NumModals1 = CurrentNumModalStage1;
		const int32 NumModals2 = CurrentNumModalStage2;
		
		float TotalEnergy = GetCurrentDelayBufferEnergy(NumModals1, D1Buffer1.GetData(), D2Buffer1.GetData());
		TotalEnergy += GetCurrentDelayBufferEnergy(NumModals2, D1Buffer2.GetData(), D2Buffer2.GetData());
		if(TotalEnergy < 1e-4f * InVelocity)
			return;
		
		bIsReStrikeFadeOut = true;
			
		NumReStrikeFadeOutModal = LBSImpactSFXSynth::FitToAudioRegister(NumModals1 + NumModals2);
		ReStrikeTwoDecayCosBuffer.SetNumUninitialized(NumReStrikeFadeOutModal, EAllowShrinking::No);
		ReStrikeRSqBuffer.SetNumUninitialized(NumReStrikeFadeOutModal, EAllowShrinking::No);
		ReStrikeD1Buffer.SetNumUninitialized(NumReStrikeFadeOutModal, EAllowShrinking::No);
		ReStrikeD2Buffer.SetNumUninitialized(NumReStrikeFadeOutModal, EAllowShrinking::No);
				
		const int32 ClearSize = NumReStrikeFadeOutModal * sizeof(float);
		FMemory::Memzero(ReStrikeTwoDecayCosBuffer.GetData(), ClearSize);
		FMemory::Memzero(ReStrikeRSqBuffer.GetData(), ClearSize);
		FMemory::Memzero(ReStrikeD1Buffer.GetData(), ClearSize);
		FMemory::Memzero(ReStrikeD2Buffer.GetData(), ClearSize);
				
		int i = 0;
		CopyToReStrikeBuffers(NumModals1, i, TwoDecayCosBuffer1.GetData(), RSqBuffer1.GetData(),
										 D1Buffer1.GetData(), D2Buffer1.GetData());
			
		CopyToReStrikeBuffers(NumModals2, i, TwoDecayCosBuffer2.GetData(), RSqBuffer2.GetData(),
										 D1Buffer2.GetData(), D2Buffer2.GetData());
		NumReStrikeFadeOutModal = i;
	}

	void FPianoKeySynth::CopyToReStrikeBuffers(const int32 NumModals, int32& OutCurrentIndex,
										       const float* TwoDecayCosBuffer, const float* RSqBuffer,
										       const float* D1Buffer, const float* D2Buffer)
	{
		int32 i = 0;
		for(; i < NumModals; i++, OutCurrentIndex++)
		{
			const float DecaySquare = RSqBuffer[i];
			if(DecaySquare < 1e-2f)
				break;

			ReStrikeTwoDecayCosBuffer[OutCurrentIndex] = TwoDecayCosBuffer[i] * RESTRIKE_DECAY_SCALE;
			ReStrikeRSqBuffer[OutCurrentIndex] = DecaySquare * RESTRIKE_DECAY_SCALE_SQ;
			ReStrikeD1Buffer[OutCurrentIndex] = D1Buffer[i];
			ReStrikeD2Buffer[OutCurrentIndex] = D2Buffer[i];
		}
	}

	float FPianoKeySynth::GetCurrentDelayBufferEnergy(const int32 NumModals, const float* L1Buffer, const float* L2Buffer)
	{
		VectorRegister4Float TotalMagVector = VectorZeroFloat();
		for(int32 j = 0; j < NumModals; j += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
		{
			VectorRegister4Float y1 = VectorLoadAligned(&L1Buffer[j]);
			VectorRegister4Float y2 = VectorLoadAligned(&L2Buffer[j]);
			TotalMagVector = VectorAdd(TotalMagVector, VectorAdd(VectorAbs(y1), VectorAbs(y2)));
		}
		
		float SumVal[4];
		VectorStore(TotalMagVector, SumVal);
		return SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
	}

	void FPianoKeySynth::Synthesize(TArrayView<float>& OutAudio, const bool IsNoteOn, const bool IsSusPedalOn, const bool bFullSynth)
	{
		if(OutAudio.Num() == 0)
		{
			UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoKeySynth::Synthesize: Velocity = %f with number output frames = %d."), Velocity, OutAudio.Num());
			return;
		}
		
		StartSynthesizing(OutAudio, IsNoteOn, IsSusPedalOn, bFullSynth);
		CurrentSampleIndex += OutAudio.Num();
	}

	void FPianoKeySynth::StartSynthesizing(TArrayView<float>& OutAudio, const bool IsNoteOn, const bool IsSusPedalOn, const bool bFullSynth)
	{
		CurrentBufferStartSample = 0;
		const int32 NumOutputFrames = OutAudio.Num();
		
		if(bIsReStrikeFadeOut)
		{
			SynthLastStrikeFadeOut(OutAudio);
		}
		else if(CurrentSampleIndex < NumInitDelay)
		{
			const int32 NumDelay = NumInitDelay - CurrentSampleIndex;
			if(NumDelay >= NumOutputFrames)
			{
				CurrentBufferStartSample = NumOutputFrames + 1; //Set this to outside of output frame to prevent synthesizing
				return;
			}
			
			CurrentBufferStartSample = NumDelay;
		}
		
		const int32 NewNumModals = LBSImpactSFXSynth::GetNumUsedModals(CurrentNumModalStage1, D1Buffer1, D2Buffer1, STRENGTH_MIN);
		LBSImpactSFXSynth::ResetBuffersToZero(NewNumModals, CurrentNumModalStage1, D1Buffer1.GetData(), D2Buffer1.GetData());
		CurrentNumModalStage1 = NewNumModals;
		if(CurrentNumModalStage1 <= 0)
			return;
		
		ChangeParamsIfNeeded(IsNoteOn, IsSusPedalOn);

		if(bFullSynth)
		{
			const int32 NewNumModals2 = LBSImpactSFXSynth::ValidateNumUsedModals(CurrentNumModalStage2, D1Buffer2, D2Buffer2, STRENGTH_MIN);
			LBSImpactSFXSynth::ResetBuffersToZero(NewNumModals2, CurrentNumModalStage2, D1Buffer2.GetData(), D2Buffer2.GetData());
			CurrentNumModalStage2 = NewNumModals2;
			
			SynthesizeAllStages(OutAudio, NumOutputFrames);
		}
		else
		{
			SynthesizeOneStage(OutAudio, NumOutputFrames, CurrentNumModalStage1, TwoDecayCosBuffer1.GetData(), RSqBuffer1.GetData(),
						  D1Buffer1.GetData(), D2Buffer1.GetData());
		}
	}
	
	void FPianoKeySynth::ChangeParamsIfNeeded(const bool IsNoteOn, const bool IsSusPedalOn)
	{
		bool bIsDecayChange = false;
		if(!bIsSecondDecay && (CurrentSampleIndex - NumInitDelay) * TimeStep > KeyData->GetSecondDecayTime())
		{
			bIsSecondDecay = true;
			bIsDecayChange = true;
		}
		
		bool bIsNoteOnOffChange = !IsSusPedalOn && bHasDamper && (bIsNoteOn != IsNoteOn);
		if(!bIsDecayChange && (IsSusPedalOn == bIsSusPedalOn) && !bIsNoteOnOffChange)
			return;
		
		bIsSusPedalOn =  IsSusPedalOn;
		bIsNoteOn = IsNoteOn;
		
		// Allow to change from pedal on to off on playing
		// Changing from off to on might create instability issue when the pedal is switching rapidly
		bIsInitSusPedalOn = bIsInitSusPedalOn && IsSusPedalOn;
			
		TArrayView<const float> ModalsParams;
		float BeatingScale;
		if(bIsInitSusPedalOn)
		{
			ModalsParams = KeyData->GetPedalOnParams();
			BeatingScale = KeyData->GetBeatingScalePedalOn();
		}
		else
		{
			ModalsParams = KeyData->GetPedalOffParams();
			BeatingScale = KeyData->GetBeatingScalePedalOff();
		}

		const float SecondDecay = KeyData->GetSecondDecay();
		float DeltaDecay = SecondDecay;
		if(bIsSecondDecay)
		{
			DeltaDecay = -SecondDecay;
			BeatingScale = !bHasDamper ? (BeatingScale + 0.25f) : 1.f; 				
		}
		
		DeltaDecay = (bHasDamper && !bIsNoteOn && !bIsSusPedalOn) ? NoteOffDecayDelta - SecondDecay : DeltaDecay;

		const float PI_Time = UE_TWO_PI * TimeStep;
		int32 BeatIdx = UPianoKeyObj::BeatFreqIndex;
		for(int i = UPianoKeyObj::DecayIndex, j = 0, k = 0; i < NumUsedParams; i+=UPianoKeyObj::NumParamPerModal, j++)
		{
			const float DecayBase = ModalsParams[i];
			const float Decay = DecayBase + DeltaDecay;
			
			const float OldDecayRate1 = FMath::Sqrt(RSqBuffer1[j]);
			const float DecayRate1 = FMath::Exp(-Decay * TimeStep);
			TwoDecayCosBuffer1[j] = TwoDecayCosBuffer1[j] * DecayRate1 / OldDecayRate1;
			RSqBuffer1[j] = DecayRate1 * DecayRate1;
			
			const float DeltaBeatFreq = ModalsParams[BeatIdx];
			if(DeltaBeatFreq != 0)
			{
				const float BeatFreq = ModalsParams[BeatIdx - 1] + DeltaBeatFreq;
				const float Angle2 = PI_Time * BeatFreq;
				const float DecayRate2 = FMath::Exp(-Decay * TimeStep / BeatingScale);
				TwoDecayCosBuffer2[k] = 2.f * DecayRate2 * FMath::Cos(Angle2);
				RSqBuffer2[k] = DecayRate2 * DecayRate2;
				SymResonGainBuffer2[k] = SymResonAmpBuffer[j] * DecayRate2 * FMath::Sin(Angle2) * SymResonScale;
				k++;
			}
				
			BeatIdx += UPianoKeyObj::NumParamPerModal;
		}
	}

	void FPianoKeySynth::SynthLastStrikeFadeOut(TArrayView<float>& OutAudio)
	{
		const int32 NumFadeSamples = FMath::Min(OutAudio.Num(), AttackView.Num() - CurrentSampleIndex);
		// const int32 NumFadeSamples = OutAudio.Num();
		if(NumFadeSamples <= 0 || NumReStrikeFadeOutModal == 0)
		{
			bIsReStrikeFadeOut = false;
			NumReStrikeFadeOutModal = 0;
			return;
		}

		// NumReStrikeFadeOutModal = LBSImpactSFXSynth::GetNumUsedModals(NumReStrikeFadeOutModal, ReStrikeD1Buffer, ReStrikeD2Buffer);
		
		const float NumOutFrames = OutAudio.Num();
		for(int i = 0; i < NumOutFrames; i++)
		{
			VectorRegister4Float SumModalVector = VectorZeroFloat();
			ProcessVectorModal(NumReStrikeFadeOutModal, ReStrikeTwoDecayCosBuffer.GetData(), ReStrikeRSqBuffer.GetData(),
					ReStrikeD1Buffer.GetData(), ReStrikeD2Buffer.GetData(), SumModalVector);

			float SumVal[4];
			VectorStore(SumModalVector, SumVal);
			const float SampleSum = SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
			OutAudio[i] += SampleSum;
		}
	}

	void FPianoKeySynth::SynthesizeAllStages(TArrayView<float>& OutBuffer, const int32 EndSample)
	{
		const float* TwoRCosData1 =  TwoDecayCosBuffer1.GetData();
		const float* R2Data1 = RSqBuffer1.GetData();
		float* OutL1BufferPtr1 = D1Buffer1.GetData();
		float* OutL2BufferPtr1 = D2Buffer1.GetData();
		
		const float* TwoRCosData2 =  TwoDecayCosBuffer2.GetData();
		const float* R2Data2 = RSqBuffer2.GetData();
		float* OutL1BufferPtr2 = D1Buffer2.GetData();
		float* OutL2BufferPtr2 = D2Buffer2.GetData();

		const int32 NumModal1 = CurrentNumModalStage1;
		const int32 NumModal2 = CurrentNumModalStage2;
		
		if(CurrentSampleIndex < AttackView.Num())
		{
			int32 AttackIndex = CurrentSampleIndex;
			if(!bIsReStrikeFadeOut && CurrentSampleIndex < NumInitDelay)
				AttackIndex = NumInitDelay;
			
			const int32 EndAttackSamples = CurrentBufferStartSample + FMath::Min(EndSample - CurrentBufferStartSample, AttackView.Num() - AttackIndex);
			for(int i = CurrentBufferStartSample; i < EndAttackSamples; i++, AttackIndex++)
			{
				VectorRegister4Float SumModalVector = VectorZeroFloat();
				ProcessVectorModal(NumModal1, TwoRCosData1, R2Data1, OutL1BufferPtr1, OutL2BufferPtr1, SumModalVector);
				ProcessVectorModal(NumModal2, TwoRCosData2, R2Data2, OutL1BufferPtr2, OutL2BufferPtr2, SumModalVector);

				float SumVal[4];
				VectorStore(SumModalVector, SumVal);
				const float SampleSum = SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
				OutBuffer[i] += SampleSum * AttackView[AttackIndex];
			}
			CurrentBufferStartSample = EndAttackSamples;
		}
		
		for(int i = CurrentBufferStartSample; i < EndSample; i++)
		{
			VectorRegister4Float SumModalVector = VectorZeroFloat();
			ProcessVectorModal(NumModal1, TwoRCosData1, R2Data1, OutL1BufferPtr1, OutL2BufferPtr1, SumModalVector);
			ProcessVectorModal(NumModal2, TwoRCosData2, R2Data2, OutL1BufferPtr2, OutL2BufferPtr2, SumModalVector);

			float SumVal[4];
			VectorStore(SumModalVector, SumVal);
			const float SampleSum = SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
			OutBuffer[i] += SampleSum;
		}
	}
	
	void FPianoKeySynth::SynthesizeSecondStageAndSymReson(TArrayView<float>& OutAudio, const TArrayView<const float>& InResonAudio, const bool bNewNoteOnTrigger)
	{
		const int32 NumOutputFrames = OutAudio.Num();
		if(InResonAudio.Num() != NumOutputFrames)
			UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoKeySynth::SynthesizeSecondStageAndSymReson: The number of reson audio frames != the number of output frames!"));
		
		if(bNewNoteOnTrigger)
		{
			CurrentNumModalStage2 = TwoDecayCosBuffer2.Num();
		}
		else
		{
			const int32 NewNumModals = LBSImpactSFXSynth::ValidateNumUsedModals(CurrentNumModalStage2, D1Buffer2, D2Buffer2, STRENGTH_MIN);
			LBSImpactSFXSynth::ResetBuffersToZero(NewNumModals, CurrentNumModalStage2, D1Buffer2.GetData(), D2Buffer2.GetData());
			CurrentNumModalStage2 = NewNumModals;
			if(CurrentNumModalStage2 <= 0)
				return;
		}
		
		StartSynthesizeSecondStageSymReson(OutAudio, InResonAudio, NumOutputFrames);
	}
	
	void FPianoKeySynth::SynthesizeOneStage(TArrayView<float>& OutBuffer, const int32 EndSample, const int32 NumModals,
	                                        const float* TwoRCosData, const float* R2Data,
	                                        float* OutL1BufferPtr, float* OutL2BufferPtr)
	{
		for(int i = CurrentBufferStartSample; i < EndSample; i++)
		{
			VectorRegister4Float SumModalVector = VectorZeroFloat();
			ProcessVectorModal(NumModals, TwoRCosData, R2Data, OutL1BufferPtr, OutL2BufferPtr, SumModalVector);
				
			float SumVal[4];
			VectorStore(SumModalVector, SumVal);
			const float SampleSum = SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
			OutBuffer[i] += SampleSum;
		}
	}

	void FPianoKeySynth::StartSynthesizeSecondStageSymReson(TArrayView<float>& OutBuffer, const TArrayView<const float>& InResonAudio, const int32 EndSample)
	{
		const float* TwoRCosData = TwoDecayCosBuffer2.GetData();
		const float* R2Data = RSqBuffer2.GetData();
		const float* GainFData = SymResonGainBuffer2.GetData();
		float* OutL1BufferPtr = D1Buffer2.GetData();
		float* OutL2BufferPtr = D2Buffer2.GetData();
		
		for(int i = CurrentBufferStartSample; i < EndSample; i++)
		{
			VectorRegister4Float ForceReg = VectorLoadFloat1(&InResonAudio[i]);
			VectorRegister4Float SumModalVector = VectorZeroFloat();
			ProcessVectorModalWithForce(CurrentNumModalStage2, TwoRCosData, R2Data, GainFData,
										OutL1BufferPtr, OutL2BufferPtr, ForceReg, SumModalVector);
				
			float SumVal[4];
			VectorStore(SumModalVector, SumVal);
			OutBuffer[i] += SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
		}
	}

	void FPianoKeySynth::ProcessVectorModal(const int32 NumModals, const float* TwoRCosData, const float* R2Data,
    											float* OutL1BufferPtr, float* OutL2BufferPtr, VectorRegister4Float& SumModalVector)
	{
		for(int j = 0; j < NumModals; j += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
		{
			VectorRegister4Float y1 = VectorLoadAligned(&OutL1BufferPtr[j]);
			VectorRegister4Float y2 = VectorLoadAligned(&OutL2BufferPtr[j]);
			VectorRegister4Float TwoRCos = VectorMultiply(VectorLoadAligned(&TwoRCosData[j]), y1);
			VectorRegister4Float R2 = VectorMultiply(VectorLoadAligned(&R2Data[j]), y2);
    					
			VectorStore(y1, &OutL2BufferPtr[j]);
			y1 = VectorSubtract(TwoRCos, R2);
    					
			SumModalVector = VectorAdd(SumModalVector, y1);
			VectorStore(y1, &OutL1BufferPtr[j]);
		}
		
	}

	void FPianoKeySynth::ProcessVectorModalWithForce(int32 NumModals, const float* TwoRCosData, const float* R2Data,
													 const float* GainFData, float* OutL1BufferPtr, float* OutL2BufferPtr,
													 VectorRegister4Float ForceReg, VectorRegister4Float& SumModalVector)
	{
		for(int j = 0; j < NumModals; j += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
		{
			VectorRegister4Float y1 = VectorLoadAligned(&OutL1BufferPtr[j]);
			VectorRegister4Float y2 = VectorLoadAligned(&OutL2BufferPtr[j]);
					
			VectorRegister4Float TwoRCos = VectorMultiply(VectorLoadAligned(&TwoRCosData[j]), y1);
			VectorRegister4Float R2 = VectorMultiply(VectorLoadAligned(&R2Data[j]), y2);
				
			VectorStore(y1, &OutL2BufferPtr[j]);
			y1 = VectorSubtract(TwoRCos, R2);
			y1 = VectorMultiplyAdd(VectorLoadAligned(&GainFData[j]), ForceReg, y1);
				
			SumModalVector = VectorAdd(SumModalVector, y1);
			VectorStore(y1, &OutL1BufferPtr[j]);
		}
	}

	float FPianoKeySynth::GetVelocityScaleDelta(const float BaseFreqScale) const
	{
		TArrayView<const float> VelScaleParams = KeyData->GetVelocityParams();
		const float A = VelScaleParams[0];
		const float B = VelScaleParams[1];
		const float C = VelScaleParams[2];
		
		const float Scale = BaseFreqScale * Velocity * A + B * Velocity + C * (Velocity * Velocity);
		const float MaxScale = BaseFreqScale * VelocityStandard * A + B * VelocityStandard + C * (VelocityStandard * VelocityStandard);
		return (Scale - MaxScale) / 20.f;
	}

	float FPianoKeySynth::ClampFreqVelScale(const float FreqVelScale)
	{
		return FMath::Clamp(FreqVelScale, 1e-20f, 1.45f);
	}
}

#undef STRENGTH_MIN
#undef RESTRIKE_DECAY
#undef RESTRIKE_DECAY_SCALE