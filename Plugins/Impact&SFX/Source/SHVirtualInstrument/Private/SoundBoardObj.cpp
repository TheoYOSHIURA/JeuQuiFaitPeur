// Copyright 2023-2024, Le Binh Son, All rights reserved.


#include "SoundBoardObj.h"

#include "EditorFramework/AssetImportData.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(SoundBoardObj)

USoundboardObj::USoundboardObj(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Version = 0;
	NumModals = -1;
}

void USoundboardObj::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << Version;
	Ar << NumModals;
	Ar << Params;
}

void USoundboardObj::PostInitProperties()
{
	UObject::PostInitProperties();

#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

void USoundboardObj::SetData(const int32 InVersion, const int32 InNumModals, TArrayView<float> InParams)
{
	Version = InVersion;
	NumModals = InNumModals;

	Params.SetNumUninitialized(InParams.Num());
	FMemory::Memcpy(Params.GetData(), InParams.GetData(), InParams.Num() * sizeof(float));
}


TSharedPtr<Audio::IProxyData, ESPMode::ThreadSafe> USoundboardObj::CreateProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	return GetProxy();
}

FSoundboardObjAssetProxyPtr USoundboardObj::GetProxy()
{
#if WITH_EDITOR
	return CreateNewSoundboardObjProxyData();
#endif
	
	if (!Proxy.IsValid())
	{
		Proxy = CreateNewSoundboardObjProxyData();
	}
	return Proxy;
}

FSoundboardObjAssetProxyPtr USoundboardObj::CreateNewSoundboardObjProxyData()
{
	return MakeShared<FSoundboardObjAssetProxy>(this);
}

FSoundboardObjAssetProxy::FSoundboardObjAssetProxy(USoundboardObj* InSoundboardData)
{
	SoundboardObj = InSoundboardData;
}
