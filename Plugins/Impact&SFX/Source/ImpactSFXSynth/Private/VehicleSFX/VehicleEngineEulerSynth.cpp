// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "VehicleSFX/VehicleEngineEulerSynth.h"

#include "ModalSynth.h"
#include "ImpactSFXSynth/Public/Utils.h"

#define FREQ_BASE (100.0f)
#define AMP_THRESH (1e-5f)

namespace LBSImpactSFXSynth
{
	FVehicleEngineEulerSynth::FVehicleEngineEulerSynth(const float InSamplingRate, const int32 InNumPulsePerCycle,
													const FImpactModalObjAssetProxyPtr& ModalsParams,
													const int32 NumModal, const int32 NumModalNonThrottle,
													const float InHarmonicGain, 
													const int32 InSeed)
		: SamplingRate(InSamplingRate), LastFreq(0.f), BaseFreq(0.f),
	     LastHarmonicRand(0.f), 
		 PrevRPM(-1.f), DecelerationTimer(0.f), bIsNoThrottle(false)
	{
		Seed = InSeed > -1 ? InSeed : FMath::Rand();
		RandomStream = FRandomStream(Seed);
		TimeStep = 1.f / SamplingRate;
		FrameTime = 0.f;
		
		NumPulsePerCycle = FMath::Max(1, InNumPulsePerCycle);
		RPMBaseLine = FREQ_BASE / NumPulsePerCycle * 60.f;
		
		if(ModalsParams.IsValid())
			InitBuffers(ModalsParams->GetParams(), NumModal);

		//v1.6.0: Change to always start at non throttle mode
		SetNonThrottleMode(NumModalNonThrottle);
		for(int j = 0; j < CurrentModeNumModals; j++)
		{
			TargetEnvelopBuffer[j] = InHarmonicGain;
			CurrentEnvelopeBuffer[j] = InHarmonicGain;
		}
		//Set to 1. to allow immediate change to throttle mode
		DecelerationTimer = 1.f;
	}

	void FVehicleEngineEulerSynth::InitBuffers(const TArrayView<const float>& ModalsParams, const int32 NumUsedModals)
	{
		int32 NumModals = ModalsParams.Num() / FModalSynth::NumParamsPerModal;
		NumModals = NumUsedModals > 0 ? FMath::Min(NumModals, NumUsedModals) : NumModals;
		NumTrueModal = NumModals;
		checkf(NumModals > 0, TEXT("NumNodals must be larger than 0!"));
		
		EnvelopeIdxMap.Empty(NumTrueModal);
		NumUsedParams = FModalSynth::NumParamsPerModal * NumTrueModal;
		CurrentModeNumModals = NumTrueModal;
		
		//Make sure NumModals is a multiplier of Vector register so all modals can run in SIMD when synthesizing
		NumModals = FitToAudioRegister(NumModals);
		NumModalSynth = NumModals;
		
		const int32 ClearSize = NumModals * sizeof(float);
		D2Buffer.SetNumUninitialized(NumModals);
		D1Buffer.SetNumUninitialized(NumModals);
		TwoRCosBuffer.SetNumUninitialized(NumModals);
		CurrentEnvelopeBuffer.SetNumUninitialized(NumModals);
		TargetEnvelopBuffer.SetNumUninitialized(NumModals);
		CurrentFreqBuffer.SetNumUninitialized(NumModals);
		FinalAmpBuffer.SetNumUninitialized(NumModals);
		
		FMemory::Memzero(D1Buffer.GetData(), ClearSize);
		FMemory::Memzero(D2Buffer.GetData(), ClearSize);
		FMemory::Memzero(TwoRCosBuffer.GetData(), ClearSize);
		FMemory::Memzero(CurrentEnvelopeBuffer.GetData(), ClearSize);
		FMemory::Memzero(TargetEnvelopBuffer.GetData(), ClearSize);
		FMemory::Memzero(CurrentFreqBuffer.GetData(), ClearSize);
		FMemory::Memzero(FinalAmpBuffer.GetData(), ClearSize);

		const float PiTimeStep = UE_TWO_PI * TimeStep;
		for(int i = 0, j = 0; i < NumUsedParams; i++, j++)
		{
			const float Amp = ModalsParams[i];
			i+=2;
			const float Freq = ModalsParams[i];
			const float Angle = Freq * PiTimeStep;
			
			TwoRCosBuffer[j] = 2.f * FMath::Cos(Angle);
			D1Buffer[j] =  Amp * FMath::Sin(Angle);
			CurrentFreqBuffer[j] = Freq;
			FinalAmpBuffer[j] = 1.0f;
		}
	}

	void FVehicleEngineEulerSynth::Generate(FMultichannelBufferView& OutAudio, const FVehicleEngineParams& Params, const FImpactModalObjAssetProxyPtr& ModalsParams)
	{
		const int32 NumOutputFrames = Audio::GetMultichannelBufferNumFrames(OutAudio);
		if(NumOutputFrames <= 0 || Params.RPM < 1e-5f || !ModalsParams.IsValid())
			return;
		
		FrameTime = TimeStep * NumOutputFrames;
		const float FreqRPM = Params.RPM / 60.f * NumPulsePerCycle;
		BaseFreq = FMath::Clamp(FreqRPM * Params.FreqScale, 20.0f, 20000.0f);
		const float DeltaRPM = Params.RPM - LastFreq / NumPulsePerCycle * 60.f;
		LastFreq = FreqRPM;
		
		const TArrayView<float> HarmonicBuffer = TArrayView<float>(OutAudio[0].GetData(), NumOutputFrames);
		const TArrayView<const float> ModalData = ModalsParams->GetParams();

		const float RPMFreqRate = FMath::Max((Params.RPM - RPMBaseLine) / RPMBaseLine, -0.8f);
		RPMCurve = FMath::Sqrt(FMath::Abs(DeltaRPM) * 10.f) * Params.RPMNoiseFactor;
		RPMCurve = FMath::Min(RPMCurve, 1.f);
		const float RPMChangeFactor = FMath::Max(Params.RPMNoiseFactor, RPMCurve);
		const float DeltaRand = Params.RandPeriod / FMath::Max(1.0f, RPMChangeFactor * 2.f);
		const float MaxAmpRand = Params.AmpRandMin * 2.0f;
		const float AmpRandRange = (Params.AmpRandMax - Params.AmpRandMin) * RandomStream.FRand() * FMath::Sign(DeltaRPM) * RPMCurve;
		const float FreqVar = Params.HarmonicFluctuation * RPMChangeFactor / 5.0f;
		
		ChangeEngineMode(Params);

		const float RandChance = FMath::Clamp(RPMCurve * 2.f, 0.5f, 1.f);
		if(LastHarmonicRand > DeltaRand || ModalsParams->IsParamChanged())
		{
			LastHarmonicRand = 0.f;
			for(int i = 2, j = 0; i < NumUsedParams && j < CurrentModeNumModals; i+=3, j++)
			{
				if(!EnvelopeIdxMap.Contains(j) && RandomStream.FRand() > RandChance)
				{
					const float RandEnv = FMath::Clamp((RandomStream.FRand() - 0.5f) * MaxAmpRand + AmpRandRange * RandomStream.FRand(), -1.f, 0.25f);
					const float NewEnv =  1.0f + RandEnv;
					AddToInterpEnv(j, NewEnv * Params.HarmonicGain, RandomStream.FRand() + 1.f);
				}
			}
		}
		else
		{
			LastHarmonicRand += FrameTime;
		}

		UpdateFreqParams(Params, ModalData, RPMFreqRate, FreqVar);
		
		NumModalSynth = GetNumNonZeroEnvelop();
		if(NumModalSynth <= 1)
		{
			for(int i = 0; i < NumOutputFrames; i++)
			{
				const float OutValue = D1Buffer[0] * TwoRCosBuffer[0] - D2Buffer[0];
				D2Buffer[0] = D1Buffer[0];
				D1Buffer[0] = OutValue;
				HarmonicBuffer[i] = OutValue * FinalAmpBuffer[0];
			}
		}
		else
		{
			VectorSynthHarmonics(HarmonicBuffer);
		}

		TArray<int32> ToRemoveKeys;
		ToRemoveKeys.Empty(EnvelopeIdxMap.Num());
		for(auto& MapIdx : EnvelopeIdxMap)
		{
			const int32 Index = MapIdx.Key;
			const float TargetValue = TargetEnvelopBuffer[Index];
			const float CurrentValue = CurrentEnvelopeBuffer[Index];
			if(CurrentValue > TargetValue)
			{
				CurrentEnvelopeBuffer[Index] = FMath::Max(TargetValue, CurrentValue - MapIdx.Value * FrameTime);
			}
			else if(CurrentValue < TargetValue)
			{
				CurrentEnvelopeBuffer[Index] = FMath::Min(TargetValue, CurrentValue + MapIdx.Value * FrameTime);
			}
			
			if(FMath::IsNearlyEqual(CurrentEnvelopeBuffer[Index], TargetValue, 1e-5f))
			{
				ToRemoveKeys.Add(Index);
			}
		}

		for(int32 Idx : ToRemoveKeys)
			EnvelopeIdxMap.Remove(Idx);
	}

	void FVehicleEngineEulerSynth::UpdateFreqParams(const FVehicleEngineParams& Params, const TArrayView<const float> ModalData, const float RPMFreqRate, const float FreqVar)
	{
		for(int i = 2, j = 0; i < NumUsedParams && j < NumModalSynth; i += 3, j++)
		{
			const float FreqScale = ModalData[i] * Params.FreqScale;
			const float FreqRPM = GetRandFreq(Params, RPMFreqRate, FreqVar, FreqScale);
			const float Angle =  FreqRPM * UE_TWO_PI * TimeStep;
			TwoRCosBuffer[j] = 2.f * FMath::Cos(Angle);

			const float y1D = D1Buffer[j];
			D2Buffer[j] = y1D - (y1D - D2Buffer[j]) * FreqRPM / CurrentFreqBuffer[j];
			CurrentFreqBuffer[j] = FreqRPM;
			
			const float LowPassAmp = ConvertLowPassDbToLinear(FreqRPM, Params.CutoffFreq, Params.FallOffDb);
			if(LowPassAmp <= AMP_THRESH)
			{
				CurrentEnvelopeBuffer[j] = 0.f;
				TargetEnvelopBuffer[j] = 0.f;
			}
			FinalAmpBuffer[j] = CurrentEnvelopeBuffer[j] * LowPassAmp;
		}
	}

	void FVehicleEngineEulerSynth::AddToInterpEnv(int Index, const float NewEnv, float InterpSpeed)
	{
		TargetEnvelopBuffer[Index] = NewEnv;
		EnvelopeIdxMap.Add(Index, InterpSpeed);
	}

	float FVehicleEngineEulerSynth::GetRandFreq(const FVehicleEngineParams& Params, const float RPMFreqRate,
	                                                       const float FreqVar, const float Freq) const
	{
		float FreqRPM = Freq * (1.f + RPMFreqRate);
		const float HarmonicScale = FMath::Sqrt(FreqRPM / BaseFreq) * FreqVar;
		FreqRPM += (RandomStream.FRand() - 0.5f) * (FreqRPM * HarmonicScale + Params.F0Fluctuation);
		return FMath::Clamp(FreqRPM, 20.f, 20e3f);
	}

	void FVehicleEngineEulerSynth::ChangeEngineMode(const FVehicleEngineParams& Params)
	{
		const int32 OldNumModals = CurrentModeNumModals;
		//If PrevRPM <= 0, the engine is just init so we avoid changing engine mode
		const float DeltaRPM = PrevRPM > 0.f ? Params.RPM - PrevRPM : 0.f;
		PrevRPM = Params.RPM;
		if(FMath::IsNearlyZero(Params.ThrottleInput, 1e-1))
		{
			// if(Params.RPM < Params.IdleRPM)
			// 	 SetThrottleMode();
			// else
				 SetNonThrottleMode(Params.NumModalsDeceleration);			
		}
		else
		{
			DecelerationTimer += FrameTime;
			if(DeltaRPM < -25.f)
			{
				 SetNonThrottleMode(Params.NumModalsDeceleration);
			}
			else if (DeltaRPM > -5.f && DecelerationTimer > 0.1f)
				SetThrottleMode();
		}
		
		if(OldNumModals < CurrentModeNumModals)
		{
			const int32 NumModals = FMath::Min(CurrentModeNumModals, D1Buffer.Num());
			for(int j = OldNumModals; j < NumModals; j++)
			{
				AddToInterpEnv(j, Params.HarmonicGain, 2.0f);
			}
		}
		else if (OldNumModals > CurrentModeNumModals)
		{
			const int32 NumModals = FMath::Min(OldNumModals, TwoRCosBuffer.Num());
			for(int j = CurrentModeNumModals; j < NumModals; j++)
			{
				AddToInterpEnv(j, 0.f, 10.f);
			}
		}
	}

	void FVehicleEngineEulerSynth::SetNonThrottleMode(const int NumNoThrottleModals)
	{		
		bIsNoThrottle = true;
		DecelerationTimer = 0.f;
		CurrentModeNumModals = NumNoThrottleModals > 0 ? (FMath::Min(NumTrueModal, NumNoThrottleModals)) : NumTrueModal;
	}

	void FVehicleEngineEulerSynth::SetThrottleMode()
	{
		bIsNoThrottle = false;
		CurrentModeNumModals = NumTrueModal;
	}

	void FVehicleEngineEulerSynth::VectorSynthHarmonics(TArrayView<float> OutBuffer)
	{
		const float* TwoRCosData = TwoRCosBuffer.GetData();
		float* OutL1BufferPtr = D1Buffer.GetData();
		float* OutL2BufferPtr = D2Buffer.GetData();
		float* EnvelopBufferPtr = FinalAmpBuffer.GetData();
		
		const int32 NumOutputFrames = OutBuffer.Num();
		for(int i = 0; i < NumOutputFrames; i++)
		{
			VectorRegister4Float SumModalVector = VectorZeroFloat();
			float SumVal[4];
			for(int j = 0; j < NumModalSynth; j += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
			{
				VectorRegister4Float y1 = VectorLoadAligned(&OutL1BufferPtr[j]);
				VectorRegister4Float y2 = VectorLoadAligned(&OutL2BufferPtr[j]);

				VectorRegister4Float TwoRCos = VectorMultiply(VectorLoadAligned(&TwoRCosData[j]), y1);
				
				VectorStore(y1, &OutL2BufferPtr[j]);
				y1 = VectorSubtract(TwoRCos, y2);
				VectorStore(y1, &OutL1BufferPtr[j]);
				
				VectorRegister4Float EnvReg = VectorLoadAligned(&EnvelopBufferPtr[j]);
				SumModalVector = VectorMultiplyAdd(EnvReg, y1, SumModalVector);
			}
									
			VectorStore(SumModalVector, SumVal);
			const float SampleSum = SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
			
			OutBuffer[i] = SampleSum;
		}
	}

	int32 FVehicleEngineEulerSynth::GetNumNonZeroEnvelop()
	{
		const int32 NumModals = FMath::Min(NumTrueModal, FMath::Max(NumModalSynth, CurrentModeNumModals));
		int32 Count = NumModals - 1;
		for(; Count > 0; Count--)
		{
			if(CurrentEnvelopeBuffer[Count] > 1.5e-5f || TargetEnvelopBuffer[Count] > 1.5e-5f)
				break;
		}
		Count++;
		return Count;
	}
}

#undef FREQ_BASE
#undef AMP_THRESH