// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "ImpactSFXSynth/Public/Utils.h"
#include "DSP/AlignedBuffer.h"

namespace LBSImpactSFXSynth
{
	bool IsPowerOf2(const int32 Num)
	{
		return  Num != 0 && !(Num & (Num - 1));
	}

	int32 PositiveMod(const int32 Value, const int32 MaxValue)
	{
		return  (MaxValue + (Value % MaxValue)) % MaxValue;
	}

	float GetPitchScale(const float Pitch)
	{
		return FMath::Pow(2.0f, Pitch / 12.0f);
	}

	float GetPitchScaleClamped(const float InPitchShift, const float MinShift, const float MaxPitch)
	{
		const float Pitch = FMath::Clamp(InPitchShift, MinShift, MaxPitch);
		return GetPitchScale(Pitch);
	}

	float GetDampingRatioClamped(const float InRatio)
	{
		return FMath::Clamp(InRatio, 0.f, 1.f);
	}

	float GetRandRange(const FRandomStream& RandomStream, const float InMinValue, const float InRange)
	{
		return InMinValue + InRange * RandomStream.GetFraction();
	}

	int32 FitToAudioRegister(int32 InNumber)
	{
		const int32 RemVec = InNumber % AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER;
		if(RemVec > 0)
			InNumber = InNumber + (AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER - RemVec);

		return InNumber;
	}

	int32 GetNumUsedModals(const int32 CurrentNumModals, TArrayView<const float> D1Buffer, TArrayView<const float> D2Buffer, const float StrengthMin)
	{
		const float* OutL1BufferPtr = D1Buffer.GetData();
		const float* OutL2BufferPtr = D2Buffer.GetData();

		int32 j = CurrentNumModals - AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER;
		for(; j > -1; j -= AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
		{
			VectorRegister4Float y1 = VectorLoadAligned(&OutL1BufferPtr[j]);
			VectorRegister4Float y2 = VectorLoadAligned(&OutL2BufferPtr[j]);
			VectorRegister4Float TotalMagVec = VectorAdd(VectorAbs(y1), VectorAbs(y2));
			
			float SumVal[4];
			VectorStore(TotalMagVec, SumVal);
			const float SampleSum = SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
			if(SampleSum > StrengthMin)
				break;
		}
		return FMath::Max(0, j + AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER);
	}

	int32 ValidateNumUsedModals(const int32 CurrentNumModals, TArrayView<float> D1Buffer,
		TArrayView<float> D2Buffer, const float StrengthMin)
	{
		float* OutL1BufferPtr = D1Buffer.GetData();
		float* OutL2BufferPtr = D2Buffer.GetData();

		int32 OutNumModals = 0;
		VectorRegister4Float OneVector = VectorOneFloat(); 
		for(int32 j = 0; j < CurrentNumModals; j += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER)
		{
			VectorRegister4Float y1 = VectorLoadAligned(&OutL1BufferPtr[j]);			
			VectorRegister4Float y1Abs = VectorAbs(y1);
			VectorRegister4Float Mask = VectorCompareLE(y1Abs, OneVector);
			VectorStore(VectorBitwiseAnd(y1, Mask), &OutL1BufferPtr[j]);
			y1Abs = VectorBitwiseAnd(y1Abs, Mask);
			
			VectorRegister4Float y2 = VectorLoadAligned(&OutL2BufferPtr[j]);
            VectorRegister4Float y2Abs = VectorAbs(y2);
            Mask = VectorCompareLE(y2Abs, OneVector);
			VectorStore(VectorBitwiseAnd(y2, Mask), &OutL2BufferPtr[j]);
			y2Abs = VectorBitwiseAnd(y2Abs, Mask);
			
			VectorRegister4Float TotalMagVec = VectorAdd(y1Abs, y2Abs);
			
			float SumVal[4];
			VectorStore(TotalMagVec, SumVal);
			const float SampleSum = SumVal[0] + SumVal[1] + SumVal[2] + SumVal[3];
			if(SampleSum < StrengthMin)
				break;

			OutNumModals += AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER;
		}
		
		return OutNumModals;
	}

	void ResetBuffersToZero(const int32 StartIdx, const int32 EndIdx, float* OutD1Buffer, float* OutD2Buffer)
	{
		for(int j = StartIdx; j < EndIdx; j++)
		{
			OutD1Buffer[j] = 0.f;
			OutD2Buffer[j] = 0.f;
		}
	}

	float ConvertLowPassDbToLinear(float InFreq, float CutoffFreq, float FallOffDb)
	{
		const float Amp = InFreq > CutoffFreq ? FMath::Pow(InFreq / CutoffFreq, FallOffDb / 20.0f) : 1.0f; 
		return Amp;
	}
}
