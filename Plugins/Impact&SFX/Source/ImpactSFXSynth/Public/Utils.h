// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

namespace LBSImpactSFXSynth
{
	IMPACTSFXSYNTH_API bool IsPowerOf2(const int32 Num);
	IMPACTSFXSYNTH_API int32 PositiveMod(const int32 Value, const int32 MaxValue);
	IMPACTSFXSYNTH_API float GetPitchScale(const float Pitch);
	IMPACTSFXSYNTH_API float GetPitchScaleClamped(const float InPitchShift, const float MinShift = -72.f, const float MaxPitch = 72.f);
	IMPACTSFXSYNTH_API float GetDampingRatioClamped(const float InRatio);
	IMPACTSFXSYNTH_API float GetRandRange(const FRandomStream& RandomStream, const float InMinValue, const float InRange);

	// Fit number to fit audio register length to run on SIMD
	IMPACTSFXSYNTH_API int32 FitToAudioRegister(int32 InNumber);
	IMPACTSFXSYNTH_API int32 GetNumUsedModals(const int32 CurrentNumModals, TArrayView<const float> D1Buffer, TArrayView<const float> D2Buffer, const float StrengthMin=0.0001f);
	IMPACTSFXSYNTH_API int32 ValidateNumUsedModals(const int32 CurrentNumModals, TArrayView<float> D1Buffer, TArrayView<float> D2Buffer, const float StrengthMin=0.0001f);
	
	IMPACTSFXSYNTH_API void ResetBuffersToZero(const int32 StartIdx, const int32 EndIdx, float* OutD1Buffer, float* OutD2Buffer);

	IMPACTSFXSYNTH_API float ConvertLowPassDbToLinear(float InFreq, float CutoffFreq, float FallOffDb);

}
