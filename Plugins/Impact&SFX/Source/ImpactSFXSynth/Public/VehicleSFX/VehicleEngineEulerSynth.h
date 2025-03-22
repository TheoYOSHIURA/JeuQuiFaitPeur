// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once

#include "ImpactModalObj.h"
#include "DSP/MultichannelBuffer.h"
#include "VehicleSFX/VehicleEngineSynth.h"

namespace LBSImpactSFXSynth
{
	using namespace Audio;
	
	class IMPACTSFXSYNTH_API FVehicleEngineEulerSynth
	{
	public:

		FVehicleEngineEulerSynth(const float InSamplingRate, const int32 InNumPulsePerCycle,
							  const FImpactModalObjAssetProxyPtr& ModalsParams,
							  const int32 NumModal, const int32 NumModalNonThrottle,
							  const float InHarmonicGain, 
							  const int32 InSeed = -1);

		void Generate(FMultichannelBufferView& OutAudio, const FVehicleEngineParams& Params, const FImpactModalObjAssetProxyPtr& ModalsParams);

		float GetCurrentBaseFreq() const { return BaseFreq; }
		bool IsInDeceleration() const { return bIsNoThrottle; }
		float GetRPMCurve() const { return  RPMCurve; }
		
	protected:
		void InitBuffers(const TArrayView<const float>& ModalsParams, const int32 NumUsedModals);

		FORCEINLINE float GetRandFreq(const FVehicleEngineParams& Params, float RPMFreqRate, float FreqVar, float Freq) const;
		FORCEINLINE void AddToInterpEnv(int Index, float NewEnv, float InterpSpeed = 1.f);

		void UpdateFreqParams(const FVehicleEngineParams& Params, TArrayView<const float> ModalData, float RPMFreqRate, float FreqVar);
		
		void ChangeEngineMode(const FVehicleEngineParams& Params);

		void SetNonThrottleMode(const int NumNoThrottleModals);
		void SetThrottleMode();

		void VectorSynthHarmonics(TArrayView<float> OutBuffer);

		int32 GetNumNonZeroEnvelop();
		
	private:
		float SamplingRate;
		
		int32 Seed;
		int32 NumPulsePerCycle;
		float RPMBaseLine;
		float LastFreq;
		float BaseFreq;
		
		FRandomStream RandomStream;

		float TimeStep;
		float FrameTime;
		
		int32 NumUsedParams;
		int32 NumTrueModal;
		float LastHarmonicRand;
		FAlignedFloatBuffer D1Buffer;
		FAlignedFloatBuffer D2Buffer;
		FAlignedFloatBuffer TwoRCosBuffer;
		FAlignedFloatBuffer TargetEnvelopBuffer;
		FAlignedFloatBuffer CurrentEnvelopeBuffer;
		FAlignedFloatBuffer CurrentFreqBuffer;
		FAlignedFloatBuffer FinalAmpBuffer;

		int32 CurrentModeNumModals;
		int32 NumModalSynth;
		
		TMap<int32, float> EnvelopeIdxMap;

		float PrevRPM;
		float DecelerationTimer;
		bool bIsNoThrottle;
		float RPMCurve;
	};
}