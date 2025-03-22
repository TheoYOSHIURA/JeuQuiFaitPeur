// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IAudioProxyInitializer.h"
#include "ImpactModalObj.h"
#include "PianoKeyObj.h"
#include "SoundBoardObj.h"

#include "PianoModel.generated.h"

class FPianoModelAssetProxy;
using FPianoModelAssetProxyPtr = TSharedPtr<FPianoModelAssetProxy, ESPMode::ThreadSafe>;

UCLASS(editinlinenew, BlueprintType,  meta = (LoadBehavior = "LazyOnDemand"))
class SHVIRTUALINSTRUMENT_API UPianoModel : public UObject, public IAudioProxyDataFactory
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly, meta = (DisplayName = "Hammer Model"), Category="Hammer")
	TObjectPtr<UImpactModalObj> HammerObj;
	
	UPROPERTY(EditDefaultsOnly, meta = (DisplayName = "Soundboard Model"), Category="Soundboard")
	TObjectPtr<USoundboardObj> SoundboardObj;

	UPROPERTY(EditDefaultsOnly, meta = (DisplayName = "Piano Keys Model"), Category="Keys")
	TObjectPtr<UPianoKeyObj> PianoKeyModel;
	
	UPROPERTY(VisibleAnywhere, meta = (DisplayName = "First MIDI Note"), Category="Keys")
	int32 StartMidiNote = 0;

	UPROPERTY(VisibleAnywhere, meta = (DisplayName = "Last MIDI Note"), Category="Keys")
	int32 EndMidiNote = 0;

public:
	
	
	UPianoModel(const FObjectInitializer& ObjectInitializer);
	
	//~Begin IAudioProxyDataFactory Interface.
	virtual TSharedPtr<Audio::IProxyData> CreateProxyData(const Audio::FProxyDataInitParams& InitParams) override;
	//~ End IAudioProxyDataFactory Interface.
	FPianoModelAssetProxyPtr CreateNewPianoModelProxyData();
	
	FImpactModalObjAssetProxyPtr GetHammerObjProxy() const;
	FSoundboardObjAssetProxyPtr GetSoundboardObjAssetProxy() const;
	UPianoKeyObj* GetPianoKeyModel() const { return PianoKeyModel; }
	
	int32 GetStartMidiNote() const { return StartMidiNote; };
	int32 GetEndMidiNote() const { return EndMidiNote; };
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	
private:
	FPianoModelAssetProxyPtr Proxy{ nullptr };
};

class SHVIRTUALINSTRUMENT_API FPianoModelAssetProxy : public Audio::TProxyData<FPianoModelAssetProxy>, public TSharedFromThis<FPianoModelAssetProxy, ESPMode::ThreadSafe>
{
public:
	IMPL_AUDIOPROXY_CLASS(FPianoModelAssetProxy);

	FPianoModelAssetProxy(const FPianoModelAssetProxy& InAssetProxy);
	FPianoModelAssetProxy(const UPianoModel* InPianoModel);

	const FImpactModalObjAssetProxyPtr& GetHammerObjProxy() const;
	const FSoundboardObjAssetProxyPtr& GetSoundboardObjProxy() const;
	UPianoKeyData* GetPianoKeyData(const int32 Index) const;
	
	int32 GetStartMidiNote() const { return PianoKeyObj->GetStartMidiNote(); }
	int32 GetNumKeys() const { return PianoKeyObj->GetNumKeys(); }

	float GetVelocityStandard() const { return PianoKeyObj->GetVelocityStandard(); }
	float GetSymResonRescale() const { return PianoKeyObj->GetSymResonRescale(); }
	float GetNoteOffDecayDelta() const { return PianoKeyObj->GetNoteOffDecayDelta(); }
	float GetDynAdjustThreshold() const { return PianoKeyObj->GetDynAdjustThreshold(); }
	float GetDynAdjustFactorMin() const { return PianoKeyObj->GetDynAdjustFactorMin(); }
	
protected:
	FImpactModalObjAssetProxyPtr HammerObjAssetProxyPtr;
	FSoundboardObjAssetProxyPtr SoundboardObjAssetProxyPtr;
	UPianoKeyObj* PianoKeyObj;
};