// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Liquid/BurbleSoundGen.h"

#include "ImpactSFXSynthLog.h"
#include "ImpactSFXSynth/Public/Utils.h"

namespace LBSImpactSFXSynth
{
	using namespace Audio;

	FBurbleSoundGen::FBurbleSoundGen(const float InSamplingRate, int32 InSeed, int32 InMaxNumberOfBurbles)
		: SamplingRate(InSamplingRate) , MaxNumberOfBurbles(InMaxNumberOfBurbles), LastShrinkDelaSample(0)
	{
		Seed = InSeed < 0 ? FMath::Rand() : InSeed;
		RandomStream = FRandomStream(Seed);

		TimeStep = 1.0f / InSamplingRate;
		NumSamplesPerGen = 0.0025f * SamplingRate;
		DeltaGenerateSample = INT_MAX; //Make sure we can gen at time 0
		
		CurrentNumBurbles = 0;
		CurrentBufferSize = FitToAudioRegister(64);
		
		D1Buffer.SetNumUninitialized(CurrentBufferSize);
		D2Buffer.SetNumUninitialized(CurrentBufferSize);
		TwoRCosBuffer.SetNumUninitialized(CurrentBufferSize);
		R2Buffer.SetNumUninitialized(CurrentBufferSize);
		ChirpTwoRCosBuffer.SetNumUninitialized(CurrentBufferSize);
		TwoRCosD2Buffer.SetNumUninitialized(CurrentBufferSize);
		TwoRCosMaxBuffer.SetNumUninitialized(CurrentBufferSize);
		DurationBuffer.SetNumUninitialized(CurrentBufferSize);
		
		int32 ClearSize = CurrentBufferSize * sizeof(float);
		FMemory::Memzero(D1Buffer.GetData(), ClearSize);
		FMemory::Memzero(D2Buffer.GetData(), ClearSize);		
		FMemory::Memzero(TwoRCosBuffer.GetData(), ClearSize);
		FMemory::Memzero(R2Buffer.GetData(), ClearSize);
		FMemory::Memzero(ChirpTwoRCosBuffer.GetData(), ClearSize);
		FMemory::Memzero(TwoRCosD2Buffer.GetData(), ClearSize);
		FMemory::Memzero(TwoRCosMaxBuffer.GetData(), ClearSize);
		FMemory::Memzero(DurationBuffer.GetData(), ClearSize);
	}

	void FBurbleSoundGen::Generate(TArrayView<float>& OutAudio, const FBurbleSoundSpawnParams& SpawnParams)
	{
		const int32 NumOutFrames = OutAudio.Num();
		int32 CurrentOutBufferIndex = 0;

		if(IsShouldShrinkBurbles())
			ShrinkCurrentBurbles();
		
		int32 GenStep = INT_MAX - 1;
		const float BubbleSpawnRate = SpawnParams.GetSpawnRate();
		bool bCanSpawn = BubbleSpawnRate > 0.f && SpawnParams.GetSpawnChance() > 0.f;
		if(bCanSpawn)
		{
			GenStep = BubbleSpawnRate >= 0.05f ? static_cast<int32>(SamplingRate / BubbleSpawnRate) : (INT_MAX - 1);
			if(DeltaGenerateSample > GenStep)
				RandGenBurble(SpawnParams);
		}
		
		while(CurrentOutBufferIndex < NumOutFrames)
		{
			int32 Increment;
			if(bCanSpawn)
			{
				Increment = FMath::Min3(GenStep - DeltaGenerateSample, NumOutFrames - CurrentOutBufferIndex, NumSamplesPerGen);
				DeltaGenerateSample += Increment;
			}
			else
			{
				Increment = FMath::Min(NumOutFrames - CurrentOutBufferIndex, NumSamplesPerGen);
			}
			
			if(CurrentNumBurbles > 0)
				SynthesizeSamples(OutAudio, CurrentOutBufferIndex, CurrentOutBufferIndex + Increment);			
			
			CurrentOutBufferIndex += Increment;
			LastShrinkDelaSample += Increment;
			
			if(IsShouldShrinkBurbles())
				ShrinkCurrentBurbles();
			
			if(bCanSpawn && DeltaGenerateSample >= GenStep)
				RandGenBurble(SpawnParams);
		}
	}

	void FBurbleSoundGen::ShrinkCurrentBurbles()
	{
		LastShrinkDelaSample = 0;
		int32 SwapIdx = CurrentNumBurbles - 1;
		for(int i = 0; i < CurrentNumBurbles;)
		{
			if(DurationBuffer[i] <= 0.f)
			{
				D1Buffer[i] = D1Buffer[SwapIdx];
				D2Buffer[i] = D2Buffer[SwapIdx];
				TwoRCosBuffer[i] = TwoRCosBuffer[SwapIdx];
				R2Buffer[i] = R2Buffer[SwapIdx];
		
				TwoRCosD2Buffer[i] = TwoRCosD2Buffer[SwapIdx];
				TwoRCosMaxBuffer[i]  = TwoRCosMaxBuffer[SwapIdx];
				ChirpTwoRCosBuffer[i] = ChirpTwoRCosBuffer[SwapIdx];
				DurationBuffer[i] = DurationBuffer[SwapIdx];

				D1Buffer[SwapIdx] = 0.f;
				D2Buffer[SwapIdx] = 0.f;
				TwoRCosBuffer[SwapIdx] = 0.f;
				TwoRCosD2Buffer[SwapIdx] = 0.f;
				TwoRCosMaxBuffer[SwapIdx] = 0.f;
				
				CurrentNumBurbles--;
				SwapIdx--;
				
				continue;
			}

			i++;
		}
	}

	bool FBurbleSoundGen::RandGenBurble(const FBurbleSoundSpawnParams& SpawnParams)
	{
		DeltaGenerateSample = 0;
		
		bool bIsReachMax = MaxNumberOfBurbles > 0 ? CurrentNumBurbles >= MaxNumberOfBurbles : false; 
		if(bIsReachMax || RandomStream.FRand() > SpawnParams.GetSpawnChance())
			return false;
		
		const float Radius = GetBurbleRadius(SpawnParams);
		const float Freq = GetBurbleFreq(SpawnParams, Radius);
		const float AmpAbs = GetBurbleAmp(SpawnParams, Radius, Freq);
		if(AmpAbs == 0.f || Radius == 0.f)
			return false;
		
		float Decay = GetDecayRate(Freq);
		float ChirpRate = Decay * SpawnParams.GetDecayToChirpRatio();
		const float Threshold = SpawnParams.GetDecayThresh();
		Decay = Decay >= Threshold ? Decay : FMath::Min(Threshold, SpawnParams.GetDecayScale() * Decay);  
		
		const int32 Index = CurrentNumBurbles;
		CurrentNumBurbles++;
		if(CurrentNumBurbles > CurrentBufferSize)
		{
			//0.32 is based on spawning 10000 burbles per second with default params
			const float EstMaxNumBurbles = SpawnParams.GetSpawnChance() * SpawnParams.GetSpawnRate() * 0.032f;
			const int32 MaxExtend = FMath::Max(FMath::RoundToInt32(EstMaxNumBurbles) - CurrentBufferSize, 16);
			const int32 ExtendSize = FitToAudioRegister(FMath::Min(MaxNumberOfBurbles - CurrentBufferSize, MaxExtend));
			ExtendBuffers(ExtendSize);
		}
		
		const float R = FMath::Exp(-Decay * TimeStep);
		R2Buffer[Index] = R * R;
		
		const double Theta = UE_DOUBLE_TWO_PI * TimeStep * Freq;
		TwoRCosBuffer[Index] = 2.f * R * FMath::Cos(Theta);
		
		const double ChirpSampling = ChirpRate * TimeStep;
		TwoRCosD2Buffer[Index] = 2.f * R * FMath::Cos(Theta * (1.0f - ChirpSampling));
		TwoRCosMaxBuffer[Index] = 2.f * R;
		
		const double FChirp = ChirpSampling * Freq;
		const double TwoRCos = 2.f * FMath::Cos(FChirp * UE_DOUBLE_TWO_PI * TimeStep);
		ChirpTwoRCosBuffer[Index] = TwoRCos;
		
		// Random phase (0 or 180) to avoid DC artifacts 
		const float Amp = RandomStream.FRand() > 0.5f ? AmpAbs : -AmpAbs;
		D1Buffer[Index] = Amp * R * FMath::Sin(Theta);
		D2Buffer[Index] = 0.f;
		
		DurationBuffer[Index] = FMath::Loge(1e-4f / AmpAbs) / -Decay;
		return true;
	}

	float FBurbleSoundGen::GetBurbleRadius(const FBurbleSoundSpawnParams& SpawnParams) const
	{
		//0.97 is chosen to reduce radius dynamic range.
		const float Rand = (1.0f - RandomStream.FRand() * 0.97f);
		const float Radius = SpawnParams.GetRadiusMin() * FMath::Pow(Rand, SpawnParams.GetRadiusDistCoef());
		return FMath::Min(Radius + SpawnParams.GetRadiusOffset(), SpawnParams.GetRadiusMax());
	}

	float FBurbleSoundGen::GetBurbleAmp(const FBurbleSoundSpawnParams& SpawnParams, const float Radius, const float Freq) const
	{
		//0.95 is chosen to reduce amplitude dynamic range.
		const float Rand = 1.0f - RandomStream.FRand() * 0.95f;
		const float RadiusAmpFactor = SpawnParams.GetRadiusAmpFactor();
		const float RadiusFactor = (Radius * RadiusAmpFactor + 0.01f * (1.f - RadiusAmpFactor));
		float Amp = SpawnParams.GetGain() * (SpawnParams.GetAmpOffset() + RadiusFactor * FMath::Pow(Rand, SpawnParams.GetAmpDistCoef()));

		if(FreqAmpCurve.IsValid())		
			Amp *= FreqAmpCurve->GetValueByTimeInterp(Freq);
		
		return FMath::Clamp(Amp, 0.f, SpawnParams.GetGainMax()) ;
	}

	float FBurbleSoundGen::GetBurbleFreq(const FBurbleSoundSpawnParams& SpawnParams, const float Radius) const
	{
		return FMath::Clamp(3.0f * SpawnParams.GetPitchShift() / Radius, 20.f, 20e3f);
	}

	float FBurbleSoundGen::GetDecayRate(const float Freq) const
	{
		const float TotalDecay = FMath::Sqrt(Freq) * 0.0009760646f + 0.0592092f;
		return  TotalDecay * UE_PI * Freq;
	}

	void FBurbleSoundGen::ExtendBuffers(const int32 ExtendSize)
	{
		CurrentBufferSize += ExtendSize;
		D1Buffer.AddZeroed(ExtendSize);
		D2Buffer.AddZeroed(ExtendSize);
		TwoRCosBuffer.AddZeroed(ExtendSize);
		R2Buffer.AddZeroed(ExtendSize);
		ChirpTwoRCosBuffer.AddZeroed(ExtendSize);
		TwoRCosD2Buffer.AddZeroed(ExtendSize);
		TwoRCosMaxBuffer.AddZeroed(ExtendSize);
		DurationBuffer.AddZeroed(ExtendSize);
	}

	void FBurbleSoundGen::SynthesizeSamples(TArrayView<float>& OutBuffer, const int32 StartIndex, const int32 EndIdx)
	{
		float* TwoRCosData = TwoRCosBuffer.GetData();
		const float* R2Data = R2Buffer.GetData();
		float* OutL1BufferPtr = D1Buffer.GetData();
		float* OutL2BufferPtr = D2Buffer.GetData();
		
		float* TwoRCosD1BufferPtr = TwoRCosD2Buffer.GetData();
		float* TwoRCosMaxBufferPtr = TwoRCosMaxBuffer.GetData();
		float* ChirpTwoRCosBufferPtr = ChirpTwoRCosBuffer.GetData();

		float Max = FMath::Sqrt(R2Buffer[0]) * 2.f;
		VectorRegister4Float MaxTwoR = VectorLoadFloat1(&Max);
		
		for(int i = StartIndex; i < EndIdx; i++)
		{
			VectorRegister4Float SumModalVector = VectorZeroFloat();
			
			for(int j = 0; j < CurrentNumBurbles; j += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
			{
				VectorRegister4Float y1 = VectorLoadAligned(&OutL1BufferPtr[j]);
				VectorRegister4Float y2 = VectorLoadAligned(&OutL2BufferPtr[j]);
				VectorRegister4Float TwoRCos = VectorLoadAligned(&TwoRCosData[j]);
				VectorRegister4Float R2 = VectorMultiply(VectorLoadAligned(&R2Data[j]), y2);
    			
				VectorStore(y1, &OutL2BufferPtr[j]);
				y1 = VectorMultiply(TwoRCos, y1);
				y1 = VectorSubtract(y1, R2);
    					
				SumModalVector = VectorAdd(SumModalVector, y1);
				VectorStore(y1, &OutL1BufferPtr[j]);
				
				// Update TwoRCos buffer to increase freq
				y1 = VectorMultiply(TwoRCos, VectorLoadAligned(&ChirpTwoRCosBufferPtr[j]));
				y1 = VectorSubtract(y1, VectorLoadAligned(&TwoRCosD1BufferPtr[j]));
				y1 = VectorMin(VectorLoadAligned(&TwoRCosMaxBufferPtr[j]), y1);
				VectorStore(TwoRCos, &TwoRCosD1BufferPtr[j]);
				VectorStore(y1, &TwoRCosData[j]);
				
			}

			float SumVal[4];
			VectorStore(SumModalVector, SumVal);
			OutBuffer[i] += SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
		}

		float* DurationBufferPtr = DurationBuffer.GetData();
		const float Duration = (EndIdx - StartIndex) * TimeStep;
		VectorRegister4Float ProcessedDurationVector = VectorLoadFloat1(&Duration);
		for(int j = 0; j < CurrentNumBurbles; j += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
		{
			// Reduce duration of processed burbles
			VectorRegister4Float DurVectorReg = VectorLoadAligned(&DurationBufferPtr[j]);
			DurVectorReg = VectorSubtract(DurVectorReg, ProcessedDurationVector);
			VectorStore(DurVectorReg, &DurationBufferPtr[j]);
		}
	}

	bool FBurbleSoundGen::IsShouldShrinkBurbles() const
	{
		return LastShrinkDelaSample > NumSamplesPerGen;
	}
}