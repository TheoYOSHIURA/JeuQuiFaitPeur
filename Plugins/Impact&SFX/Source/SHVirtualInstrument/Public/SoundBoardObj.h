// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IAudioProxyInitializer.h"
#include "SoundBoardObj.generated.h"

class UAssetImportData;
class FSoundboardObjAssetProxy;
using FSoundboardObjAssetProxyPtr = TSharedPtr<FSoundboardObjAssetProxy, ESPMode::ThreadSafe>;

UCLASS(hidecategories=Object, BlueprintType,  meta = (LoadBehavior = "LazyOnDemand"))
class SHVIRTUALINSTRUMENT_API USoundboardObj : public UObject, public IAudioProxyDataFactory
{
	GENERATED_BODY()
	
public:
	static constexpr int32 NumParamPerModal = 4;
	
private:
	TArray<float> Params;
	
	UPROPERTY(VisibleAnywhere, Category = "Soundboard")
	int32 Version;

	UPROPERTY(VisibleAnywhere, Category = "Soundboard")
	int32 NumModals;
	
public:
	USoundboardObj(const FObjectInitializer& ObjectInitializer);

	//~Begin IAudioProxyDataFactory Interface.
	virtual TSharedPtr<Audio::IProxyData, ESPMode::ThreadSafe> CreateProxyData(const Audio::FProxyDataInitParams& InitParams) override;
	//~ End IAudioProxyDataFactory Interface.
	FSoundboardObjAssetProxyPtr CreateNewSoundboardObjProxyData();
	FSoundboardObjAssetProxyPtr GetProxy();
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;
#endif

	int32 GetVersion() const { return Version; }
	int32 GetNumModals() const { return NumModals; }

	//~ Begin UObject Interface. 
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface.

	void SetData(const int32 InVersion, const int32 InNumModals, TArrayView<float> InParams);

	TArrayView<const float> GetParams() const { return TArrayView<const float>(Params);}
	int32 GetNumParams() const { return Params.Num(); }

private:
	FSoundboardObjAssetProxyPtr Proxy{ nullptr };
};

class SHVIRTUALINSTRUMENT_API FSoundboardObjAssetProxy : public Audio::TProxyData<FSoundboardObjAssetProxy>, public TSharedFromThis<FSoundboardObjAssetProxy, ESPMode::ThreadSafe>
{
public:
	IMPL_AUDIOPROXY_CLASS(FSoundboardObjAssetProxy);

	FSoundboardObjAssetProxy(const FSoundboardObjAssetProxy& InAssetProxy) :
		SoundboardObj(InAssetProxy.SoundboardObj)
	{
	}

	explicit FSoundboardObjAssetProxy(USoundboardObj* InSoundboardData);
	
	TArrayView<const float> GetParams() const
	{
		return SoundboardObj->GetParams();
	}
	
	int32 GetNumModals() const { return SoundboardObj->GetNumModals(); }
	int32 GetNumParams() const { return SoundboardObj->GetNumParams(); }

protected:
	TObjectPtr<USoundboardObj> SoundboardObj;
};