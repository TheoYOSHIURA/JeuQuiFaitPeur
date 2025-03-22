// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IAudioProxyInitializer.h"
#include "Curves/RichCurve.h"

#include "RCurveExtend.generated.h"

#if WITH_EDITOR
DECLARE_DELEGATE(FOnRCurveExtendDataBaked)
DECLARE_DELEGATE_OneParam(FOnRCurveExtendOnReSampleCurveShow, bool)
#endif

class FRCurveExtendAssetProxy;
using FRCurveExtendAssetProxyPtr = TSharedPtr<FRCurveExtendAssetProxy, ESPMode::ThreadSafe>;

UCLASS(editinlinenew, BlueprintType,  meta = (LoadBehavior = "LazyOnDemand"))
class IMPACTSFXSYNTH_API URCurveExtend : public UObject, public IAudioProxyDataFactory
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FRichCurve Curve;

	UPROPERTY(EditDefaultsOnly, Category="Curve", meta = (DisplayName = "Default Y Value"))
	float DefaultYValue = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Curve", meta = (ClampMin = 1, UIMin = 1, DisplayName = "Num Data Points", ToolTip="Number of data that will be evaluated, cached, and sent to MetaSounds graphs before execution. The caching is actually ran on Editor and stored together with this object. This is done because evaluating values when graphs are executing can be expensive if you have many keys."))
	int32 NumDataPoints = 1;
	
	//~Begin IAudioProxyDataFactory Interface.
	virtual TSharedPtr<Audio::IProxyData, ESPMode::ThreadSafe> CreateProxyData(const Audio::FProxyDataInitParams& InitParams) override;
	//~ End IAudioProxyDataFactory Interface.
	FRCurveExtendAssetProxyPtr CreateRCurveExtendProxyData();
	FRCurveExtendAssetProxyPtr GetProxy();

	float GetTimeStep() const { return TimeStep; }
	TArrayView<const float> GetDataView() const { return TArrayView<const float>(Data); }

	UPROPERTY(VisibleAnywhere, Category="Curve", meta = (DisplayName = "X-Asis Step"))
	float TimeStep;

	UPROPERTY(VisibleAnywhere, Category="Curve", meta = (DisplayName = "Min X Value"))
	float MinTime;

	UPROPERTY(VisibleAnywhere, Category="Curve", meta = (DisplayName = "Max X Value"))
	float MaxTime;
	
	UPROPERTY()
	TArray<float> Data;

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category="Curve", meta = (DisplayName = "Show Data Curve"))
	bool bIsShowReSample = true;
#endif
	
#if WITH_EDITOR
	FOnRCurveExtendDataBaked OnDataBaked;
	FOnRCurveExtendOnReSampleCurveShow OnReSampleCurveShow;
	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	
private:
	// cached proxy
	FRCurveExtendAssetProxyPtr Proxy{ nullptr };

#if WITH_EDITOR
	void BakeCurveData();
#endif
};

class IMPACTSFXSYNTH_API FRCurveExtendAssetProxy : public Audio::TProxyData<FRCurveExtendAssetProxy>, public TSharedFromThis<FRCurveExtendAssetProxy, ESPMode::ThreadSafe>
{
public:
	IMPL_AUDIOPROXY_CLASS(FRCurveExtendAssetProxy);

	FRCurveExtendAssetProxy(const FRCurveExtendAssetProxy& InAssetProxy);
	FRCurveExtendAssetProxy(const URCurveExtend* InCurveExtend);

	float GetXStart() const { return MinX; }
	float GetXEnd() const { return MaxX; }
	float GetXStep() const { return Step; }
	int32 GetNumValues() const { return Data.Num(); }

	bool IsXAxisRangeMatch(float Start, float End) const;
	
	/// Get an array of values from the curve by using cyclic interpolating
	/// @param StartX Start Key value (X axis)
	/// @param XStep Key Step
	/// @param OutArray Out Array
	/// @param NumRemoveLastSample Default to 1. This assumes the last key is redundant (the same as the first key) for a perfect cyclic interpolation. 
	void GetArrayByTimeCyclicInterp(float StartX, float XStep, TArrayView<float>& OutArray, int NumRemoveLastSample = 1);
	
	float GetValueByTimeNearest(float InTime) const;
	float GetValueByTimeInterp(float InTime) const;

	/// Index will be subtracted by MinX before accessing the array
	/// @param InIndex Index of the key
	/// @return value of the indexed key
	float GetValueByKeyIndex(const int32 InIndex) const;

	/// Direct array access with boundary checks.
	/// @param InIndex the input array index
	/// @return the value at the index
	float GetValueByArrayIndex(const int32 InIndex) const;
	
protected:
	TArray<float> Data;
	float MinX;
	int32 MinXInt;
	
	float MaxX;
	float Step;
};
