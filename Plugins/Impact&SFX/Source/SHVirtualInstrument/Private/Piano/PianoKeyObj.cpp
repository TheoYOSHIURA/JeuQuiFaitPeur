// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Piano/PianoKeyObj.h"

#include "SHVirtualInstrumentLog.h"
#include "EditorFramework/AssetImportData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PianoKeyObj)

UPianoKeyData::UPianoKeyData(const FObjectInitializer& ObjectInitializer)
{
	MidiNote = 0;
	NumModals = 0;
	NumBeats = 0;
	BaseFreq = 0;
	
	SecondDecay = 0;
	SecondDecayTime = 0;
	BeatingScalePedalOff = 0;
	BeatingScalePedalOn = 0;
	bHasDamper = false;
}

void UPianoKeyData::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << MidiNote;
	Ar << NumModals;
	Ar << NumBeats;
	Ar << BaseFreq;
	Ar << SecondDecay;
	Ar << SecondDecayTime;
	Ar << BeatingScalePedalOff;
	Ar << BeatingScalePedalOn;
	Ar << bHasDamper;
	
	Ar << VelocityParams;
	Ar << PedalOffParams;
	Ar << PedalOnParams;
}

void UPianoKeyData::SetData(int32 InMidiNote,
							int32 InNumModals,
							int32 InNumBeats,
							float InBaseFreq,
                            float InSecondDecay,
                            float InSecondDecayTime,
                            float InBeatingScalePedalOff,
                            float InBeatingScalePedalOn,
                            bool InHasDamper,
                            const TArrayView<const float>& InVelocityParams,
                            const TArrayView<const float>& InPedalOffParams,
                            const TArrayView<const float>& InPedalOnParams)
{
	MidiNote = InMidiNote;
	NumModals = InNumModals;
	NumBeats = InNumBeats;
	BaseFreq = InBaseFreq;
	SecondDecay = InSecondDecay;
	SecondDecayTime = InSecondDecayTime;
	BeatingScalePedalOff = InBeatingScalePedalOff;
	BeatingScalePedalOn = InBeatingScalePedalOn;
	bHasDamper = InHasDamper;
	
	VelocityParams.SetNumUninitialized(InVelocityParams.Num());
	FMemory::Memcpy(VelocityParams.GetData(), InVelocityParams.GetData(), InVelocityParams.Num() * sizeof(float));
	
	PedalOffParams.SetNumUninitialized(InPedalOffParams.Num());
	FMemory::Memcpy(PedalOffParams.GetData(), InPedalOffParams.GetData(), InPedalOffParams.Num() * sizeof(float));
	
	PedalOnParams.SetNumUninitialized(InPedalOnParams.Num());
	FMemory::Memcpy(PedalOnParams.GetData(), InPedalOnParams.GetData(), InPedalOnParams.Num() * sizeof(float));
}

UPianoKeyObj::UPianoKeyObj(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Version = 0;
	StartMidiNote = 0;
	EndMidiNote = 0;
	NumKeys = 0;
	VelocityStandard = 0.f;
	SymResonRescale = 0.f;
	NoteOffDecayDelta = 0.f;
	NoDamperStartingNote = 0;
	DynAdjustThreshold = 0.f;
	DynAdjustFactorMin = 0.f;
}

void UPianoKeyObj::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	Ar << Version;
	Ar << StartMidiNote;
	Ar << EndMidiNote;
	Ar << NumKeys;
	Ar << VelocityStandard;
	Ar << SymResonRescale;
	Ar << NoteOffDecayDelta;
	Ar << NoDamperStartingNote;
	Ar << DynAdjustThreshold;
	Ar << DynAdjustFactorMin;

	if(Ar.IsSaving())
	{
		for(int i = 0; i < NumKeys; i++)
			Ar << PianoKeys[i];
	}
	else if (Ar.IsLoading())
	{
		PianoKeys.Empty(NumKeys);
		for(int i = 0; i < NumKeys; i++)
		{
			UPianoKeyData* KeyData = NewObject<UPianoKeyData>(this);
			Ar << KeyData;
			PianoKeys.Emplace(KeyData);
		}
	}
}

void UPianoKeyObj::PostInitProperties()
{
	UObject::PostInitProperties();

#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

#if WITH_EDITORONLY_DATA
FString UPianoKeyObj::GetImportedSrcFilePath() const
{
	return AssetImportData->GetFirstFilename();
}
#endif


#if WITH_EDITOR

void UPianoKeyObj::InitModelParams(int32 InVersion, int32 InStartMidiNote, int32 InEndMidiNote, int32 InNumKeys,
	float InVelocityStandard, float InSymResonRescale, float InNoteOffDecayDelta, float InNoDamperStartingNote,
	float InDynAdjustThreshold, float InDynAdjustFactorMin)
{
	Version = InVersion;
	StartMidiNote = InStartMidiNote;
	EndMidiNote = InEndMidiNote;
	NumKeys = InNumKeys;
	VelocityStandard = InVelocityStandard;
	SymResonRescale = InSymResonRescale;
	NoteOffDecayDelta = InNoteOffDecayDelta;
	NoDamperStartingNote = InNoDamperStartingNote;
	DynAdjustThreshold = InDynAdjustThreshold;
	DynAdjustFactorMin = InDynAdjustFactorMin;
	
	bInitKeys.SetNumUninitialized(NumKeys);
	FMemory::Memzero(bInitKeys.GetData(), NumKeys * sizeof(bool));
	
	PianoKeys.SetNum(NumKeys);
}

bool UPianoKeyObj::AddPianoKeyData(int32 InMidiNote, int32 InNumModals, int32 InNumBeats, float InBaseFreq,
	float InSecondDecay, float InSecondDecayTime, float InBeatingScalePedalOff, float InBeatingScalePedalOn,
	bool InHasDamper, const TArray<float>& InVelocityParams, const TArray<float>& InPedalOffParams,
	const TArray<float>& InPedalOnParams)
{
	int32 Index = InMidiNote - StartMidiNote;
	if(Index >= NumKeys)
	{
		UE_LOG(LogVirtualInstrument, Error, TEXT("UPianoKeyObj::AddPianoKeyData: New data is outside of allowed MIDI note range!"));
		return false;
	}

	TArrayView<const float> InVelocityParamsView = TArrayView<const float>(InVelocityParams);
	TArrayView<const float> InPedalOffParamsView = TArrayView<const float>(InPedalOffParams);
	TArrayView<const float> InPedalOnParamsView = TArrayView<const float>(InPedalOnParams);

	UPianoKeyData* KeyData = NewObject<UPianoKeyData>(this);
	KeyData->SetData(InMidiNote,  InNumModals,  InNumBeats,  InBaseFreq,
							  InSecondDecay,  InSecondDecayTime,  InBeatingScalePedalOff,  InBeatingScalePedalOn,
							  InHasDamper,  InVelocityParamsView, InPedalOffParamsView,
							 InPedalOnParamsView);
	PianoKeys[Index] = KeyData; 
	bInitKeys[Index] = true;
	return true;
}

bool UPianoKeyObj::CheckAllKeyIsInit() const
{
	for(bool IsInit : bInitKeys)
	{
		if(!IsInit)
			return false;
	}

	return true;
}

#endif

UPianoKeyData* UPianoKeyObj::GetPianoKeyData(int32 Index) const
{
	check(Index < PianoKeys.Num() && Index >= 0);
	return PianoKeys[Index];
}
