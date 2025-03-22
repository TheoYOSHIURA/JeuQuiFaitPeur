// Copyright 2023-2024, Le Binh Son, All rights reserved.


#include "Piano/PianoModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PianoModel)

UPianoModel::UPianoModel(const FObjectInitializer& ObjectInitializer)
{

}

TSharedPtr<Audio::IProxyData> UPianoModel::CreateProxyData(const Audio::FProxyDataInitParams& InitParams)
{
#if WITH_EDITOR
	//Always return new proxy in editor as we can edit this data after MetaSound Graph is loaded
	return CreateNewPianoModelProxyData();
#endif
	
	if (!Proxy.IsValid())
	{
		Proxy = CreateNewPianoModelProxyData();
	}
	return Proxy;
}

FPianoModelAssetProxyPtr UPianoModel::CreateNewPianoModelProxyData()
{
	return MakeShared<FPianoModelAssetProxy, ESPMode::ThreadSafe>(this);
}

FImpactModalObjAssetProxyPtr UPianoModel::GetHammerObjProxy() const
{
	if(HammerObj)
		return HammerObj->GetProxy();
	else
		return nullptr;
}

FSoundboardObjAssetProxyPtr UPianoModel::GetSoundboardObjAssetProxy() const
{
	if(SoundboardObj)
		return SoundboardObj->GetProxy();
	else
		return nullptr;
}

#if WITH_EDITOR
void UPianoModel::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	if(const FProperty* Property = InPropertyChangedEvent.Property)
	{
		const FName PropertyName = Property->GetFName();
		const FName PianoKeyObjName = GET_MEMBER_NAME_CHECKED(UPianoModel, PianoKeyModel);
		if (PropertyName == PianoKeyObjName)
		{
			if(PianoKeyModel != nullptr)
			{
				StartMidiNote = PianoKeyModel->GetStartMidiNote();
				EndMidiNote = PianoKeyModel->GetEndMidiNote();
			}
			else
			{
				StartMidiNote = 0;
				EndMidiNote = 0;
			}
		}
	}
	
	UObject::PostEditChangeProperty(InPropertyChangedEvent);
}
#endif

FPianoModelAssetProxy::FPianoModelAssetProxy(const FPianoModelAssetProxy& InAssetProxy)
	: HammerObjAssetProxyPtr(InAssetProxy.HammerObjAssetProxyPtr),
	  SoundboardObjAssetProxyPtr(InAssetProxy.SoundboardObjAssetProxyPtr),
      PianoKeyObj(InAssetProxy.PianoKeyObj)
{
	
}

FPianoModelAssetProxy::FPianoModelAssetProxy(const UPianoModel* InPianoModel)
{
	HammerObjAssetProxyPtr = InPianoModel->GetHammerObjProxy();
	SoundboardObjAssetProxyPtr = InPianoModel->GetSoundboardObjAssetProxy();

	PianoKeyObj = InPianoModel->GetPianoKeyModel();
}

const FImpactModalObjAssetProxyPtr& FPianoModelAssetProxy::GetHammerObjProxy() const
{
	return HammerObjAssetProxyPtr;
}

const FSoundboardObjAssetProxyPtr& FPianoModelAssetProxy::GetSoundboardObjProxy() const
{
	return SoundboardObjAssetProxyPtr;
}

UPianoKeyData* FPianoModelAssetProxy::GetPianoKeyData(const int32 Index) const
{
	return PianoKeyObj->GetPianoKeyData(Index);
}
