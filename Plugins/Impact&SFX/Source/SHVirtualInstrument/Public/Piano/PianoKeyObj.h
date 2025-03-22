// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ImpactSFXSynth/Public/Utils.h"
#include "DSP/AlignedBuffer.h"

#include "PianoKeyObj.generated.h"

class UAssetImportData;

UCLASS(hidecategories=Object,  meta = (LoadBehavior = "LazyOnDemand"))
class SHVIRTUALINSTRUMENT_API UPianoKeyData : public UObject
{
	GENERATED_BODY()
	
private:
	int32 MidiNote ;
	int32 NumModals;
	int32 NumBeats;
	float BaseFreq;
	
	float SecondDecay;
	float SecondDecayTime;
	float BeatingScalePedalOff;
	float BeatingScalePedalOn ;
	bool bHasDamper;
    
	TArray<float> VelocityParams;
	Audio::FAlignedFloatBuffer PedalOffParams;
	Audio::FAlignedFloatBuffer PedalOnParams;
	
public:
	UPianoKeyData(const FObjectInitializer& ObjectInitializer);
	
	virtual void Serialize(FArchive& Ar) override;
	
	void SetData(int32 InMidiNote, int32 InNumModals, int32 InNumBeats,
				  float InBaseFreq, float InSecondDecay, float InSecondDecayTime,
				  float InBeatingScalePedalOff, float InBeatingScalePedalOn, bool InHasDamper,
				  const TArrayView<const float>& InVelocityParams,
				  const TArrayView<const float>& InPedalOffParams,
				  const TArrayView<const float>& InPedalOnParams);

	int32 GetMidiNote() const { return MidiNote; }
	int32 GetNumModals() const { return NumModals; }
	int32 GetNumBeats() const { return NumBeats; }
	float GetBaseFreq() const { return BaseFreq; }
	float GetSecondDecay() const { return SecondDecay; }
	float GetSecondDecayTime() const { return SecondDecayTime; }
	float GetBeatingScalePedalOff() const { return BeatingScalePedalOff; }
	float GetBeatingScalePedalOn() const { return BeatingScalePedalOn; }
	bool HasDamper() const { return bHasDamper; }
	
	TArrayView<const float> GetPedalOffParams() const { return TArrayView<const float>(PedalOffParams); }
	TArrayView<const float> GetPedalOnParams() const { return TArrayView<const float>(PedalOnParams); }
	TArrayView<const float> GetVelocityParams() const { return TArrayView<const float>(VelocityParams); }
};

UCLASS(hidecategories=Object, BlueprintType,  meta = (LoadBehavior = "LazyOnDemand"))
class SHVIRTUALINSTRUMENT_API UPianoKeyObj : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 NumParamPerModal = 4;
	static constexpr int32 AmpIndex = 0;
	static constexpr int32 DecayIndex = 1;
	static constexpr int32 FreqIndex = 2;
	static constexpr int32 BeatFreqIndex = 3;
	
private:
	UPROPERTY(VisibleAnywhere, Category = "PianoModel")
	int32 Version;

	UPROPERTY(VisibleAnywhere, Category = "PianoModel")
	int32 StartMidiNote;

	UPROPERTY(VisibleAnywhere, Category = "PianoModel")
	int32 EndMidiNote;

	UPROPERTY(VisibleAnywhere, Category = "PianoModel")
	int32 NumKeys;
	
	float VelocityStandard;

	float SymResonRescale;
	float NoteOffDecayDelta;

	int32 NoDamperStartingNote;

	float DynAdjustThreshold;
	float DynAdjustFactorMin;
	
	UPROPERTY()
	TArray<UPianoKeyData*> PianoKeys;

#if WITH_EDITORONLY_DATA
	TArray<bool> bInitKeys;
#endif
	
public:
	virtual void Serialize(FArchive& Ar) override;
	UPianoKeyObj(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitProperties() override;
	
#if WITH_EDITOR
	void InitModelParams(int32 InVersion, int32 InStartMidiNote, int32 InEndMidiNote, int32 InNumKeys,
						float InVelocityStandard, float InSymResonRescale, float InNoteOffDecayDelta, float InNoDamperStartingNote,
						float InDynAdjustThreshold, float InDynAdjustFactorMin);


	bool AddPianoKeyData(int32 InMidiNote, int32 InNumModals, int32 InNumBeats, float InBaseFreq, float InSecondDecay, float InSecondDecayTime,
				         float InBeatingScalePedalOff, float InBeatingScalePedalOn, bool InHasDamper,
				         const TArray<float>& InVelocityParams, const TArray<float>& InPedalOffParams, const TArray<float>& InPedalOnParams);

	bool CheckAllKeyIsInit() const;
#endif
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;

	FString GetImportedSrcFilePath() const;
#endif

	int32 GetVersion() const { return Version; }
	int32 GetStartMidiNote() const { return StartMidiNote; }
	int32 GetEndMidiNote() const { return EndMidiNote; }
	int32 GetNumKeys() const { return NumKeys; }

	float GetVelocityStandard() const { return VelocityStandard; }
	float GetSymResonRescale() const { return SymResonRescale; }
	float GetNoteOffDecayDelta() const { return NoteOffDecayDelta; }
	int32 GetNoDamperStartingNote() const { return NoDamperStartingNote; }
	float GetDynAdjustThreshold() const { return DynAdjustThreshold; }
	float GetDynAdjustFactorMin() const { return DynAdjustFactorMin; }
	
	UPianoKeyData* GetPianoKeyData(int32 Index) const;
};
