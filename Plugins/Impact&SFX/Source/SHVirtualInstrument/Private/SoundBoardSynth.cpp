// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "SoundBoardSynth.h"

#include "Piano/MetasoundPianoModel.h"

namespace LBSVirtualInstrument
{
	FSoundBoardSynth::FSoundBoardSynth(const float InSamplingRate, const FSoundboardObjAssetProxyPtr& SbObjectPtr,
								       const float InGain, const float InFreqScatter, const int32 InQualityScaleDown)
	: SamplingRate(InSamplingRate), FreqScatter(InFreqScatter), LastInAudioSample(0.f)
	{
		QualityScale = FMath::Clamp(InQualityScaleDown, 1, 4);
		
		if(InGain > 0.f)
			InitBuffers(SbObjectPtr);

		// Set this to zero instead of buffer length.
		// So soundboard synth will only start to run when at least a note has been played
		// This is just a small optimization to reduce the workload on starting MIDI
		CurrentNumModals = 0;
	}

	void FSoundBoardSynth::InitBuffers(const FSoundboardObjAssetProxyPtr& SbObjectPtr)
	{
		const int32 NumModal = SbObjectPtr->GetNumModals() / QualityScale;
		CurrentNumModals = LBSImpactSFXSynth::FitToAudioRegister(NumModal);
		
		const int32 ClearSize = CurrentNumModals * sizeof(float);
		OutD1Buffer.SetNumUninitialized(CurrentNumModals);
		FMemory::Memzero(OutD1Buffer.GetData(), ClearSize);
		OutD2Buffer.SetNumUninitialized(CurrentNumModals);
		FMemory::Memzero(OutD2Buffer.GetData(), ClearSize);
		
		TwoDecayCosBuffer.SetNumUninitialized(CurrentNumModals);
		FMemory::Memzero(TwoDecayCosBuffer.GetData(), ClearSize);
		R2Buffer.SetNumUninitialized(CurrentNumModals);
		FMemory::Memzero(R2Buffer.GetData(), ClearSize);
		Activation1DBuffer.SetNumUninitialized(CurrentNumModals);
		FMemory::Memzero(Activation1DBuffer.GetData(), ClearSize);
		ActivationBuffer.SetNumUninitialized(CurrentNumModals);
		FMemory::Memzero(ActivationBuffer.GetData(), ClearSize);

		bIsInit = true;
	}

	void FSoundBoardSynth::SetupParams(const FSoundboardObjAssetProxyPtr& SbObjectPtr, const float InGain)
	{
		TArrayView<const float> ModalParams = SbObjectPtr->GetParams();
		const float PiTime = UE_TWO_PI / SamplingRate;
		const int32 Step = (QualityScale - 1) * USoundboardObj::NumParamPerModal + 1;
		const int32 NumParams = ModalParams.Num();
		for(int i = 0, j = 0; i < NumParams; i += Step, j++)
		{
			const float Amp = ModalParams[i] * InGain;
			i++;

			const float Decay = ModalParams[i];
			i++;
			
			const float Freq = ModalParams[i] * (1.0f + (FMath::FRand() - 0.5f) * FreqScatter);
			i++;
			
			const float Phi = ModalParams[i];
			const float Angle = PiTime * Freq;
			
			const float DecayRate = FMath::Exp(-Decay / SamplingRate);
			TwoDecayCosBuffer[j] = 2.f * DecayRate * FMath::Cos(Angle);
			R2Buffer[j] = DecayRate * DecayRate;
			Activation1DBuffer[j] = Amp * DecayRate * FMath::Sin(Angle - Phi);
			ActivationBuffer[j] = Amp * FMath::Sin(Phi);
		}
	}

	void FSoundBoardSynth::Synthesize(TArrayView<float>& OutAudio, const FSoundboardObjAssetProxyPtr& SbObjectPtr, const bool bIsNewNoteOnEvent, const float InGain)
	{
		if(OutAudio.Num() == 0 || (InGain == 0 && !bIsInit))
			return;

		if(InGain == 0)
		{
			bIsInit = false;			
			LastInAudioSample = 0.f;
			CurrentNumModals = 0;
			return;
		}

		if(!bIsInit)
		{
			InitBuffers(SbObjectPtr);
		}
		else if(bIsNewNoteOnEvent)
		{
			CurrentNumModals = TwoDecayCosBuffer.Num();
		}
		else
		{
			const int32 NewNumModals = LBSImpactSFXSynth::GetNumUsedModals(CurrentNumModals, OutD1Buffer, OutD2Buffer, 1e-6f);
			LBSImpactSFXSynth::ResetBuffersToZero(NewNumModals, CurrentNumModals, OutD1Buffer.GetData(), OutD2Buffer.GetData());
			CurrentNumModals = NewNumModals;
			if(CurrentNumModals <= 0)
			{
				LastInAudioSample = 0.f;
				return;
			}
		}
		
		SetupParams(SbObjectPtr, InGain);		
		ProcessAudio(OutAudio, OutAudio.Num());
	}
	
	void FSoundBoardSynth::ProcessAudio(TArrayView<float>& OutAudio, const int32 NumOutputFrames)
	{
		const float* TwoRCosData = TwoDecayCosBuffer.GetData();
		const float* R2Data = R2Buffer.GetData();
		const float* GainFData = Activation1DBuffer.GetData();
		const float* GainCData = ActivationBuffer.GetData();
		float* OutL1BufferPtr = OutD1Buffer.GetData();
		float* OutL2BufferPtr = OutD2Buffer.GetData();
		
		const float NewLastSample = OutAudio[NumOutputFrames - 1];

		float PrevSample = OutAudio[0];
		{ // First frame need to use the last in audio sample
			const VectorRegister4Float InAudio1DReg = VectorLoadFloat1(&LastInAudioSample);
			const VectorRegister4Float InAudioReg = VectorLoadFloat1(&PrevSample);
			OutAudio[0] += ProcessOneSample(TwoRCosData, R2Data, GainFData, GainCData,
											OutL1BufferPtr, OutL2BufferPtr, 
											InAudio1DReg, InAudioReg);
		}
		
		for(int i = 1; i < NumOutputFrames; i++)
		{
			float CurrentSample = OutAudio[i];
			const VectorRegister4Float InAudio1DReg = VectorLoadFloat1(&PrevSample);
			const VectorRegister4Float InAudioReg = VectorLoadFloat1(&CurrentSample);
			OutAudio[i] += ProcessOneSample(TwoRCosData, R2Data, GainFData, GainCData,
											OutL1BufferPtr, OutL2BufferPtr, 
											InAudio1DReg, InAudioReg);
			PrevSample = CurrentSample;
		}
		
		LastInAudioSample = NewLastSample;
	}

	float FSoundBoardSynth::ProcessOneSample(const float* TwoRCosData, const float* R2Data, const float* GainFData,
										const float* GainCData, float* OutL1BufferPtr, float* OutL2BufferPtr,
										const VectorRegister4Float& InAudio1DReg,
										const VectorRegister4Float& InAudioReg) const
	{
		VectorRegister4Float SumModalVector = VectorZeroFloat();
		for(int j = 0; j < CurrentNumModals; j += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
		{
			VectorRegister4Float y1 = VectorLoadAligned(&OutL1BufferPtr[j]);
			VectorRegister4Float y2 = VectorLoadAligned(&OutL2BufferPtr[j]);

			VectorRegister4Float TwoRCos = VectorMultiply(VectorLoadAligned(&TwoRCosData[j]), y1);
			VectorRegister4Float R2 = VectorMultiply(VectorLoadAligned(&R2Data[j]), y2);
			
			VectorStore(y1, &OutL2BufferPtr[j]);
			y1 = VectorSubtract(TwoRCos, R2);
			y1 = VectorMultiplyAdd(VectorLoadAligned(&GainFData[j]), InAudio1DReg, y1);
			y1 = VectorMultiplyAdd(VectorLoadAligned(&GainCData[j]), InAudioReg, y1);
				
			SumModalVector = VectorAdd(SumModalVector, y1);
			VectorStore(y1, &OutL1BufferPtr[j]);
		}
			
		float SumVal[4];
		VectorStore(SumModalVector, SumVal);
		
		return (SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3]);
	}

}
