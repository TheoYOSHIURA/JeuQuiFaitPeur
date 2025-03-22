// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once

#include "SoundBoardObj.h"
#include "DSP/AlignedBuffer.h"

namespace LBSVirtualInstrument
{
	using namespace Audio;
	
	class SHVIRTUALINSTRUMENT_API FSoundBoardSynth
	{
	public:
		
		FSoundBoardSynth(const float InSamplingRate, const FSoundboardObjAssetProxyPtr& SbObjectPtr,
						 const float InGain, const float InFreqScatter, const int32 InQualityScaleDown);

		void Synthesize(TArrayView<float>& OutAudio, const FSoundboardObjAssetProxyPtr& SbObjectPtr, const bool bIsNewNoteOnEvent, const float InGain);

		bool IsRunning() const { return CurrentNumModals > 0; }
		
	protected: 
		void InitBuffers(const FSoundboardObjAssetProxyPtr& SbObjectPtr);
 
		void ProcessAudio(TArrayView<float>& OutAudio, const int32 NumOutputFrames);

		FORCEINLINE float ProcessOneSample(const float* TwoRCosData, const float* R2Data, const float* GainFData, const float* GainCData,
					float* OutL1BufferPtr, float* OutL2BufferPtr, const VectorRegister4Float& InAudio1DReg, const VectorRegister4Float& InAudioReg) const;

		void SetupParams(const FSoundboardObjAssetProxyPtr& SbObjectPtr, const float InGain);
	
	private:
		float SamplingRate;
		float FreqScatter;
		
		float LastInAudioSample;
		int32 CurrentNumModals;
		bool bIsInit;
		int32 QualityScale;
		
        FAlignedFloatBuffer TwoDecayCosBuffer;
        FAlignedFloatBuffer R2Buffer;
        FAlignedFloatBuffer Activation1DBuffer;
        FAlignedFloatBuffer ActivationBuffer;
        FAlignedFloatBuffer OutD1Buffer;
        FAlignedFloatBuffer OutD2Buffer;
	};
}