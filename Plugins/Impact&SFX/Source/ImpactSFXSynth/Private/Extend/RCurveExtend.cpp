// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Extend/RCurveExtend.h"

#include "ImpactSFXSynthLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RCurveExtend)

TSharedPtr<Audio::IProxyData, ESPMode::ThreadSafe> URCurveExtend::CreateProxyData(
		const Audio::FProxyDataInitParams& InitParams)
{
	return GetProxy();
}

FRCurveExtendAssetProxyPtr URCurveExtend::CreateRCurveExtendProxyData()
{
	return MakeShared<FRCurveExtendAssetProxy, ESPMode::ThreadSafe>(this);
}

FRCurveExtendAssetProxyPtr URCurveExtend::GetProxy()
{
#if WITH_EDITOR
	//Always return new proxy in editor as we can edit this data after MetaSound Graph is loaded
	return CreateRCurveExtendProxyData();
#endif
	
	if (!Proxy.IsValid())
	{
		Proxy = CreateRCurveExtendProxyData();
	}
	return Proxy;
}

#if WITH_EDITOR
void URCurveExtend::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	if(const FProperty* Property = InPropertyChangedEvent.Property)
	{
		const FName PropertyName = Property->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(URCurveExtend, bIsShowReSample))
		{
			// ReSharper disable once CppExpressionWithoutSideEffects
			OnReSampleCurveShow.ExecuteIfBound(bIsShowReSample);
		}
	}
	
	UObject::PostEditChangeProperty(InPropertyChangedEvent);
}

void URCurveExtend::BakeCurveData() 
{
	const int32 NumData = FMath::Max(1, NumDataPoints);
	Data.SetNumUninitialized(NumData);
	TimeStep = 0.f;
	
	const int32 NumKeys = Curve.Keys.Num();
	
	if (NumKeys == 0)
	{
		for(int i = 0; i < NumData; i++)
			Data[i] = DefaultYValue;
		MinTime = 0.f;
		MaxTime = 0.f;
		return;
	}

	MinTime = Curve.Keys[0].Time;
	MaxTime = Curve.Keys[NumKeys - 1].Time;
	TimeStep = NumData < 2 ? 0 : (MaxTime - MinTime) / (NumData - 1);
	
	float InTime = MinTime;
	for(int32 DataIndex = 0; DataIndex < NumData; DataIndex++)
	{
		Data[DataIndex] = Curve.Eval(InTime);
		InTime += TimeStep;
	}
	
	// ReSharper disable once CppExpressionWithoutSideEffects
	OnDataBaked.ExecuteIfBound();
}
#endif

void URCurveExtend::Serialize(FArchive& Ar)
{

#if WITH_EDITOR
	if(Ar.IsPersistent() && Ar.IsSaving())
		BakeCurveData();
#endif
	
	Super::Serialize(Ar);
}


FRCurveExtendAssetProxy::FRCurveExtendAssetProxy(const FRCurveExtendAssetProxy& InAssetProxy)
	: Data(InAssetProxy.Data), MinX(InAssetProxy.MinX), MinXInt(InAssetProxy.MinXInt), MaxX(InAssetProxy.MaxX), Step(InAssetProxy.Step)
{

}

FRCurveExtendAssetProxy::FRCurveExtendAssetProxy(const URCurveExtend* InCurveExtend)
{
	Data = TArray<float>(InCurveExtend->GetDataView());
	Step = InCurveExtend->GetTimeStep();
	MinX = InCurveExtend->MinTime;
	MinXInt = FMath::RoundToInt32(MinX);
	MaxX = InCurveExtend->MaxTime;
	
	if(Data.Num() < 2)
	{
		UE_LOG(LogImpactSFXSynth, Warning, TEXT("FRCurveExtend: The input curve only has %d values. Please make sure the number of data points in the curve editor is set correctly!"), Data.Num());
	}
}

bool FRCurveExtendAssetProxy::IsXAxisRangeMatch(float Start, float End) const
{
	return FMath::IsNearlyEqual(MinX, Start, 1e-5f) && FMath::IsNearlyEqual(MaxX, End, 1e-5f);
}

void FRCurveExtendAssetProxy::GetArrayByTimeCyclicInterp(float StartX, float XStep, TArrayView<float>& OutArray, int NumRemoveLastSample)
{
	const int32 NumSamples = OutArray.Num();
	if(Step <= 0.f || Data.Num() <= 0)
	{
		FMemory::Memzero(OutArray.GetData(), NumSamples * sizeof(float));
		return;
	}
	
	if(Data.Num() == 1)
	{
		for(int i = 0; i < NumSamples; i++)
			OutArray[i] = Data[0];
	}
	
	StartX = StartX - MinX;
	if(StartX < 0.f)
	{
		FMemory::Memzero(OutArray.GetData(), NumSamples * sizeof(float));
		return;
	}

	
	const int32 NumData = Data.Num() - NumRemoveLastSample;
	
	float StartBin = StartX / Step;
	float BinStep = XStep / Step;
	for(int i = 0; i < NumSamples; i++)
	{
		int32 LeftIndex = FMath::Floor(StartBin);
		float Percent = StartBin - LeftIndex;
		LeftIndex = LeftIndex % NumData;
		const int32 RightIndex = (LeftIndex + 1) % NumData;
		OutArray[i] = Data[LeftIndex] * (1.0f - Percent) + Data[RightIndex] * Percent;

		StartBin += BinStep;
	}
}

float FRCurveExtendAssetProxy::GetValueByTimeNearest(float InTime) const
{
	if(Step == 0.f)
		return Data[0];
	
	InTime = InTime - MinX;
	const int32 Index = FMath::Clamp(FMath::RoundToInt32(InTime / Step), 0, Data.Num() - 1);
	return Data[Index];
}

float FRCurveExtendAssetProxy::GetValueByTimeInterp(float InTime) const
{
	if(Step == 0.f)
		return Data[0];
	
	InTime = InTime - MinX;
	if(InTime <= 0.f)
		return Data[0];
	
	const float Bin = InTime / Step;
	const int32 EndIndex = Data.Num() - 1;
	const int32 Index = FMath::Clamp(static_cast<int32>(Bin), 0, EndIndex);
	if(Index == EndIndex)
		return Data[Index];

	float Percent = Bin - Index;
	return Data[Index] * (1.0f - Percent) + Data[Index + 1] * Percent;
}

float FRCurveExtendAssetProxy::GetValueByKeyIndex(const int32 InIndex) const
{
	return GetValueByArrayIndex(InIndex - MinXInt);
}

float FRCurveExtendAssetProxy::GetValueByArrayIndex(const int32 InIndex) const
{
	if(InIndex <= 0)
		return Data[0];

	if(InIndex >= Data.Num())
		return Data[Data.Num() - 1];
	
	return Data[InIndex];
}
