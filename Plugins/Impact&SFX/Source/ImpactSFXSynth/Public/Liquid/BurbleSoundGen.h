// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "Extend/RCurveExtend.h"
#include "ImpactSFXSynth/Public/Utils.h"
#include "DSP/AlignedBuffer.h"

namespace LBSImpactSFXSynth
{
	using namespace Audio;
	
	struct IMPACTSFXSYNTH_API FBurbleSoundSpawnParams
	{
	public:
		FORCEINLINE float GetSpawnRate() const { return SpawnRate; }
		FORCEINLINE float GetSpawnChance() const { return SpawnChance; }
		
		FORCEINLINE float GetRadiusDistCoef() const { return RadiusDistCoef; }
		FORCEINLINE float GetRadiusMin() const { return RadiusMin; }
		FORCEINLINE float GetRadiusMax() const { return RadiusMax; }
		FORCEINLINE float GetRadiusOffset() const { return RadiusOffset; }
		FORCEINLINE float GetPitchShift() const { return PitchShift; }
		
		FORCEINLINE float GetAmpDistCoef() const { return AmpDistCoef; }
		FORCEINLINE float GetRadiusAmpFactor() const { return RadiusAmpFactor; }
		FORCEINLINE float GetAmpOffset() const { return AmpOffset; }
		FORCEINLINE float GetGain() const { return Gain; }
		FORCEINLINE float GetGainMax() const { return GainMax; }
		
		FORCEINLINE float GetDecayToChirpRatio() const { return DecayToChirpRatio; }
		FORCEINLINE float GetDecayScale() const { return DecayScale; }
		FORCEINLINE float GetDecayThresh() const { return DecayThresh; }
		
		FBurbleSoundSpawnParams(float InSpawnRate, float InSpawnChance,
								float InRadiusDist, float InRadiusMin, float InRadiusMax, float InRadiusOffset,  
								float InPitchShift,
								float InAmpDist, float InRadiusAmpFactor, float InAmpOffset, float InGain, float InGainMax,
								float InDecayToChirpRatio, float InDecayScale, float InDecayThresh) :
		SpawnRate(InSpawnRate), SpawnChance(InSpawnChance)
		{
			RadiusDist = FMath::Clamp(InRadiusDist, 0.3f, 2.f); 
			RadiusDistCoef = -RadiusDist;			
			
			RadiusMin = FMath::Clamp(InRadiusMin, 0.15e-3, 150e-3);
			RadiusMax = FMath::Clamp(InRadiusMax, 0.15e-3, 150e-3);
			RadiusOffset = FMath::Clamp(InRadiusOffset, 0.f, 100e-3f);
			
			PitchShift = GetPitchScale(InPitchShift);
			
			AmpDist = FMath::Clamp(InAmpDist, 0.1f, 1.5f);
			AmpDistCoef = -AmpDist;
			RadiusAmpFactor = FMath::Clamp(InRadiusAmpFactor, 0.f , 1.f);
			AmpOffset = FMath::Clamp(InAmpOffset, 0.f, 1.f);
			
			Gain = FMath::Abs(InGain);
			GainMax = FMath::Clamp(InGainMax, 0.f, 1.f);
			
			DecayToChirpRatio = FMath::Clamp(InDecayToChirpRatio, -1.f, 1.f);
			DecayScale = FMath::Max(InDecayScale, 0.1f);
			DecayThresh = FMath::Max(InDecayThresh, 1.f); 
		}
		
	private:
		float SpawnRate;
		float SpawnChance;

		float RadiusDist;
		float RadiusDistCoef;
		float PitchShift;
		float RadiusMin;
		float RadiusMax;
	
		float AmpDist;
		float AmpDistCoef;
		float RadiusAmpFactor;
		float AmpOffset;
		float Gain;
		float GainMax;
		
		float RadiusOffset;
		float DecayToChirpRatio;
		float DecayScale;
		float DecayThresh;
	};
	
	class IMPACTSFXSYNTH_API FBurbleSoundGen
	{
	public:
		FBurbleSoundGen(const float InSamplingRate, int32 InSeed = -1, int32 InMaxNumberOfBurbles = -1);
 
		void Generate(TArrayView<float>& OutAudio, const FBurbleSoundSpawnParams& SpawnParams);

		int32 GetCurrentNumBurbles() const { return CurrentNumBurbles; }
		void SetFreqAmpCurve(const FRCurveExtendAssetProxyPtr& InCurve) { FreqAmpCurve = InCurve; }
	
	protected:
		FORCEINLINE bool IsShouldShrinkBurbles() const;
		void ShrinkCurrentBurbles();

		bool RandGenBurble(const FBurbleSoundSpawnParams& SpawnParams);
		float GetBurbleRadius(const FBurbleSoundSpawnParams& SpawnParams) const;
		float GetBurbleAmp(const FBurbleSoundSpawnParams& SpawnParams, const float Radius, const float Freq) const;
		FORCEINLINE float GetBurbleFreq(const FBurbleSoundSpawnParams& SpawnParams, const float Radius) const;
		FORCEINLINE float GetDecayRate(const float Freq) const;
		
		void ExtendBuffers(const int32 ExtendSize);

		void SynthesizeSamples(TArrayView<float>& OutBuffer, const int32 StartIndex, const int32 EndIdx);
		
	private:
		float SamplingRate;
		int32 MaxNumberOfBurbles;
		int32 NumSamplesPerGen;
		
		int32 Seed;
		FRandomStream RandomStream;
		FRCurveExtendAssetProxyPtr FreqAmpCurve;
		
		int32 CurrentBufferSize;
		int32 CurrentNumBurbles;

		float TimeStep;
		int32 DeltaGenerateSample;

		int32 LastShrinkDelaSample;

		FAlignedFloatBuffer D1Buffer;
		FAlignedFloatBuffer D2Buffer;
		FAlignedFloatBuffer TwoRCosBuffer;
		FAlignedFloatBuffer TwoRCosD2Buffer;
		FAlignedFloatBuffer TwoRCosMaxBuffer;
		FAlignedFloatBuffer R2Buffer;
		
		FAlignedFloatBuffer ChirpTwoRCosBuffer;
		FAlignedFloatBuffer DurationBuffer;
	};
}
