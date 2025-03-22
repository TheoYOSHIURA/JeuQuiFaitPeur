// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once

#include "Math/RandomStream.h"
#include "Extend/RCurveExtend.h"
#include "DSP/AlignedBuffer.h"

namespace LBSImpactSFXSynth
{
	using namespace Audio;

	enum class EForceNoiseMergeMode : int32
	{
		None = 0,
		Add,
		Multiply
	};
	
	class IMPACTSFXSYNTH_API FTwoStatesForceGen
	{
	public:
		FTwoStatesForceGen(const float InSamplingRate, const int32 NumSamplesPerBlock, const int32 InSeed = -1);
		
		void Generate(TArrayView<float>& OutAudio,
		              float InAmp,
		              float InFreq,
		              float FirstStateDutyCycle,
		              const FRCurveExtendAssetProxyPtr& InFirstCurve,
		              float FirstFreqScale,
		              EForceNoiseMergeMode FirstStateNoiseMode,
		              float FirstStateNoiseAmp,
		              const FRCurveExtendAssetProxyPtr& InSecondCurve,
		              float SecondFreqScale,
		              EForceNoiseMergeMode SecondStateNoiseMode,
		              float SecondStateNoiseAmp);

		void ResetCycleIndex() { CurrentCycleIndex = 0; }
		
	protected:
		void RemapCurrentCycleIndex(int32 NumSamplesPerCycle, int32 NumSamplesFirstState, int32 NumSamplesSecondState);
		
		void MergeNoise(EForceNoiseMergeMode NoiseMode, float Amp, TArrayView<float> OutBufferView);
		
	private:
		float SamplingRate;
		
		int32 Seed;
		FRandomStream RandomStream;

		FRCurveExtendAssetProxyPtr FirstCurve;
		FRCurveExtendAssetProxyPtr SecondCurve;
		
		int32 CurrentCycleIndex;

		FAlignedFloatBuffer RandomBuffer;
		
		int32 LastNumSamplesFirstState;
		int32 LastNumSamplesSecondState;
	};
}
