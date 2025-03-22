// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "VehicleSFX/TwoStatesForceGen.h"

#include "DSP/FloatArrayMath.h"
#include "ImpactSFXSynthLog.h"

namespace LBSImpactSFXSynth
{
	FTwoStatesForceGen::FTwoStatesForceGen(const float InSamplingRate, const int32 NumSamplesPerBlock, const int32 InSeed)
		: SamplingRate(InSamplingRate), CurrentCycleIndex(0), LastNumSamplesFirstState(-1), LastNumSamplesSecondState(-1)
	{
		Seed = InSeed > -1 ? InSeed : FMath::Rand();
		RandomStream = FRandomStream(Seed);
		RandomBuffer.SetNumUninitialized(NumSamplesPerBlock);
	}

	void FTwoStatesForceGen::Generate(TArrayView<float>& OutAudio, float InAmp, float InFreq, float FirstStateDutyCycle,
	                                  const FRCurveExtendAssetProxyPtr& InFirstCurve, float FirstFreqScale,
	                                  EForceNoiseMergeMode FirstStateNoiseMode, float FirstStateNoiseAmp,
	                                  const FRCurveExtendAssetProxyPtr& InSecondCurve, float SecondFreqScale,
	                                  EForceNoiseMergeMode SecondStateNoiseMode, float SecondStateNoiseAmp)
	{
		const int32 NumOutFrames = OutAudio.Num();
		if(NumOutFrames == 0 || InAmp == 0.f || InFreq >= SamplingRate || InFreq < 1e-3f)
			return;

		if(NumOutFrames > RandomBuffer.Num())
		{
			UE_LOG(LogImpactSFXSynth, Error, TEXT("FTwoStatesForceGen::Generate: The block rate is changed after initialization!"));
			return;
		}

		if(InFirstCurve == nullptr || InSecondCurve == nullptr || InFirstCurve->GetNumValues() <= 1 || InSecondCurve->GetNumValues() <= 1)
		{
			UE_LOG(LogImpactSFXSynth, Error, TEXT("FTwoStatesForceGen::Generate: Null or empty input curves!"));
			return;
		}

		if(!InFirstCurve->IsXAxisRangeMatch(0.f, 1.f) || !InSecondCurve->IsXAxisRangeMatch(0.f, 1.f))
		{
			UE_LOG(LogImpactSFXSynth, Error, TEXT("FTwoStatesForceGen::Generate: The time of the start and end keys of the input curves must be 0 and 1, respectively!"));
			return;
		}

		FirstStateDutyCycle = FMath::Clamp(FirstStateDutyCycle, 0.f, 1.f);
		const float SecondStateDutyCycle = 1.f - FirstStateDutyCycle;
		const float CyclePercentPerSample = InFreq / SamplingRate;
		const int32 NumSamplesPerCycle = FMath::RoundToInt32(SamplingRate / InFreq);
		const int32 NumSamplesFirstState =  FMath::RoundToInt32(FirstStateDutyCycle * NumSamplesPerCycle);
		const int32 NumSamplesSecondState = FMath::Clamp(NumSamplesPerCycle - NumSamplesFirstState, 0, NumSamplesPerCycle);

		RemapCurrentCycleIndex(NumSamplesPerCycle, NumSamplesFirstState, NumSamplesSecondState);

		const float FirstCurveStep = CyclePercentPerSample / FirstStateDutyCycle * FMath::Max(0.f, FirstFreqScale);
		const float SecondCurveStep = CyclePercentPerSample / SecondStateDutyCycle * FMath::Max(0.f, SecondFreqScale);
		int32 CurrentOutputBufferIdx = 0;
		while(CurrentOutputBufferIdx < NumOutFrames)
		{
			const int32 NumSamplesToGen = NumOutFrames - CurrentOutputBufferIdx;
			int32 NumGenSamples = 0.f;
			if(NumSamplesFirstState > 0 && CurrentCycleIndex < NumSamplesFirstState)
			{
				NumGenSamples = FMath::Min(NumSamplesToGen, NumSamplesFirstState - CurrentCycleIndex);
				TArrayView<float> OutBufferView = OutAudio.Slice(CurrentOutputBufferIdx, NumGenSamples);
				const float FirstCyclePercent = CurrentCycleIndex * FirstFreqScale / NumSamplesFirstState;
				InFirstCurve->GetArrayByTimeCyclicInterp(FirstCyclePercent, FirstCurveStep, OutBufferView);
				MergeNoise(FirstStateNoiseMode, FirstStateNoiseAmp, OutBufferView);
			}
			else if (NumSamplesSecondState > 0)
			{
				NumGenSamples = FMath::Min(NumSamplesToGen, NumSamplesPerCycle - CurrentCycleIndex);
				TArrayView<float> OutBufferView = OutAudio.Slice(CurrentOutputBufferIdx, NumGenSamples);
				const float SecondCyclePercent = (CurrentCycleIndex - NumSamplesFirstState) * SecondFreqScale / NumSamplesSecondState;
				InSecondCurve->GetArrayByTimeCyclicInterp(SecondCyclePercent, SecondCurveStep, OutBufferView);
				MergeNoise(SecondStateNoiseMode, SecondStateNoiseAmp, OutBufferView);
			}
			CurrentCycleIndex = (CurrentCycleIndex + NumGenSamples) % NumSamplesPerCycle;
			CurrentOutputBufferIdx += NumGenSamples;
		}
		
		if(!FMath::IsNearlyEqual(InAmp, 1.0f, 1e-5f))
			ArrayMultiplyByConstantInPlace(OutAudio, InAmp);
	}

	void FTwoStatesForceGen::RemapCurrentCycleIndex(const int32 NumSamplesPerCycle, const int32 NumSamplesFirstState, const int32 NumSamplesSecondState)
	{
		if(NumSamplesFirstState == LastNumSamplesFirstState && NumSamplesSecondState == LastNumSamplesSecondState)
			return;
		
		if(CurrentCycleIndex < LastNumSamplesFirstState)
		{
			CurrentCycleIndex = FMath::RoundToInt32(CurrentCycleIndex * static_cast<float>(NumSamplesFirstState) / LastNumSamplesFirstState);
		}
		else
		{
			CurrentCycleIndex = NumSamplesFirstState + FMath::RoundToInt32((CurrentCycleIndex - LastNumSamplesFirstState) * static_cast<float>(NumSamplesSecondState) / LastNumSamplesSecondState);
		}
		LastNumSamplesFirstState = NumSamplesFirstState;
		LastNumSamplesSecondState = NumSamplesSecondState;
		
		//Make sure CurrentCycleIndex always within bound when the frequency input is changed
		CurrentCycleIndex = CurrentCycleIndex % NumSamplesPerCycle;
	}
	
	void FTwoStatesForceGen::MergeNoise(EForceNoiseMergeMode NoiseMode, float Amp, TArrayView<float> OutBufferView)
	{
		if(NoiseMode == EForceNoiseMergeMode::None)
			return;
		
		const int32 NumGenSamples = OutBufferView.Num();
		for(int i = 0; i < NumGenSamples; i++)
		{
			RandomBuffer[i] = (RandomStream.FRand() - 0.5f) * Amp;
		}
			
		TArrayView<float> RandBufferView = TArrayView<float>(RandomBuffer.GetData(), NumGenSamples);
		
		if(NoiseMode == EForceNoiseMergeMode::Add)
		{
			ArrayAddInPlace(RandBufferView, OutBufferView);	
		}
		else
		{
			ArrayMultiplyInPlace(RandBufferView, OutBufferView);
		}
	}
}
