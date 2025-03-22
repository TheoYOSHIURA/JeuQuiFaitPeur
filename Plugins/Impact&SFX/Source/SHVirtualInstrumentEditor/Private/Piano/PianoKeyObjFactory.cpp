// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Piano/PianoKeyObjFactory.h"

#include "SHVirtualInstrumentEditorLog.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Piano/PianoKeyObj.h"
#include "EditorFramework/AssetImportData.h"

static bool bPianoKeyObjFactorySuppressImportOverwriteDialog = false;

UPianoKeyObjFactory::UPianoKeyObjFactory()
{
	bCreateNew = false;
	SupportedClass = UPianoKeyObj::StaticClass();
	bEditorImport = true;
	bText = true;
	
	Formats.Add(TEXT("pkeyobj;Piano Key obj file"));
	
	bAutomatedReimport = true;
}

UObject* UPianoKeyObjFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	// if already exists, remember the user settings
	UPianoKeyObj* ExisingPianoKeyObj = FindObject<UPianoKeyObj>(InParent, *InName.ToString());

	bool bUseExistingSettings = bPianoKeyObjFactorySuppressImportOverwriteDialog;

	if (ExisingPianoKeyObj && !bUseExistingSettings && !GIsAutomationTesting)
	{
		DisplayOverwriteOptionsDialog(FText::Format(
			NSLOCTEXT("PianoKeyObjFactory", "ImportOverwriteWarning", "You are about to import '{0}' over an existing piano key obj."),
			FText::FromName(InName)));

		switch (OverwriteYesOrNoToAllState)
		{
			case EAppReturnType::Yes:
			case EAppReturnType::YesAll:
			{
				// Overwrite existing settings
				bUseExistingSettings = false;
				break;
			}
			case EAppReturnType::No:
			case EAppReturnType::NoAll:
			{
				// Preserve existing settings
				bUseExistingSettings = true;
				break;
			}
			default:
			{
				GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
				return nullptr;
			}
		}
	}

	// Reset the flag back to false so subsequent imports are not suppressed unless the code explicitly suppresses it
	bPianoKeyObjFactorySuppressImportOverwriteDialog = false;

	// Use pre-existing obj if it exists and we want to keep settings,
	// otherwise create new obj and import raw data.
	UPianoKeyObj* PianoKeyObj = (bUseExistingSettings && ExisingPianoKeyObj) ? ExisingPianoKeyObj : NewObject<UPianoKeyObj>(InParent, InName, Flags);
	if(ImportFromText(PianoKeyObj, Buffer, BufferEnd))
	{
		// Store the current file path and timestamp for re-import purposes
		PianoKeyObj->AssetImportData->Update(CurrentFilename);
		
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, PianoKeyObj);
		return PianoKeyObj;
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
	return nullptr;
}

bool UPianoKeyObjFactory::ImportFromText(UPianoKeyObj* PianoKeyObj,  const TCHAR*& Buffer, const TCHAR* BufferEnd)
{
	TSharedPtr<FJsonObject> JsonParsed;
	TStringView<TCHAR> JsonString = TStringView<TCHAR>(Buffer, (BufferEnd - Buffer)); 
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::CreateFromView(JsonString);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
	{
		FString OutError;
		if(!InitDataFromJson(PianoKeyObj, JsonParsed, OutError))
		{
			UE_LOG(LogVirtualInstrumentEditor, Error, TEXT("Failed to import! %s"), *OutError);
			return false;	
		}
		return true;
	}

	return false;
}

bool UPianoKeyObjFactory::InitDataFromJson(UPianoKeyObj* PianoKeyObj, const TSharedPtr<FJsonObject>& JsonParsed, FString& OutError)
{
    int32 Version;
	bool bSuccess = JsonParsed->TryGetNumberField(TEXT("Version"), Version);
	if(!bSuccess || Version <= 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid Version = %d!"), Version);
		return false;
	}

    int32 StartMidiNote;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("StartMidi"), StartMidiNote);
	if(!bSuccess || StartMidiNote <= 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid start midi note = %d!"), StartMidiNote);
		return false;
	}
	
	int32 EndMidiNote;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("EndMidi"), EndMidiNote);
	if(!bSuccess || EndMidiNote > 127)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid end midi note = %d!"), EndMidiNote);
		return false;
	}
	
    int32 NumKeys;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("NumKeys"), NumKeys);
	if(!bSuccess || (NumKeys != (EndMidiNote - StartMidiNote + 1)))
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid number of keys = %d!"), NumKeys);
		return false;
	}

    float VelocityStandard;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("VelocityStandard"), VelocityStandard);
	if(!bSuccess || VelocityStandard <= 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid velocity standard = %f!"), VelocityStandard);
		return false;
	}
	
	float SymResonRescale;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("SymResonRescale"), SymResonRescale);
	if(!bSuccess || SymResonRescale <= 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid SymResonRescale = %f!"), SymResonRescale);
		return false;
	}

    float NoteOffDecayDelta;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("NoteOffDecayDelta"), NoteOffDecayDelta);
	if(!bSuccess || NoteOffDecayDelta <= 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid NoteOffDecayDelta = %f!"), NoteOffDecayDelta);
		return false;
	}

    int32 NoDamperStartingNote;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("NoDamperNote"), NoDamperStartingNote);
	if(!bSuccess || NoDamperStartingNote <= 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid NoteOffDecayDelta = %d!"), NoDamperStartingNote);
		return false;
	}

	float DynAdjustThreshold;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("DynAdjustThreshold"), DynAdjustThreshold);
	if(!bSuccess || DynAdjustThreshold <= 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid DynAdjustThreshold = %f!"), DynAdjustThreshold);
		return false;
	}

	float DynAdjustFactorMin;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("DynAdjustFactorMin"), DynAdjustFactorMin);
	if(!bSuccess || DynAdjustFactorMin <= 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid DynAdjustFactorMin = %f!"), DynAdjustFactorMin);
		return false;
	}

	if(!JsonParsed->HasField(TEXT("VelocityParams")))
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has no VelocityParams field!"));
		return false;
	}

	TArray<TSharedPtr<FJsonValue, ESPMode::ThreadSafe>> JArrayData = JsonParsed->GetArrayField(TEXT("VelocityParams"));
	if(JArrayData.Num() != 3)
	{
		OutError =  FString::Printf(TEXT("Imported piano key obj has invalid VelocityParams data!"));
		return false;	
	}

	TArray<float> GlobalVelScaleParams;
	GlobalVelScaleParams.SetNumUninitialized(JArrayData.Num());
	CopyFromJsonArray(GlobalVelScaleParams.GetData(), JArrayData);
	TArrayView<const float> GlobalVelScaleParamsView = TArrayView<const float>(GlobalVelScaleParams);
	
	if(!JsonParsed->HasField(TEXT("Keys")))
    {
    	OutError = FString::Printf(TEXT("Imported piano key obj has no Keys field!"));
    	return false;
    }
	
	JArrayData = JsonParsed->GetArrayField(TEXT("Keys"));
	if(JArrayData.Num() == 0 || JArrayData.Num() != NumKeys)
	{
		OutError =  FString::Printf(TEXT("Imported piano key obj has invalid number of keys!"));
		return false;	
	}

	PianoKeyObj->InitModelParams(Version, StartMidiNote, EndMidiNote, NumKeys,
								 VelocityStandard, SymResonRescale, NoteOffDecayDelta, NoDamperStartingNote,
								 DynAdjustThreshold, DynAdjustFactorMin);
	
	for(const TSharedPtr<FJsonValue> JEntry : JArrayData)
	{
	    TSharedPtr<FJsonObject> KeyJson = JEntry->AsObject();	    
		if(!ProcessOneKeyFromJson(PianoKeyObj, KeyJson, OutError, GlobalVelScaleParamsView))
			return false;
	}

	if(!PianoKeyObj->CheckAllKeyIsInit())
	{
		OutError =  FString::Printf(TEXT("Imported piano key obj has missing keys!"));
		return false;	
	}
	
	return true;
}

bool UPianoKeyObjFactory::ProcessOneKeyFromJson(UPianoKeyObj* PianoKeyObj, const TSharedPtr<FJsonObject>& JsonParsed, FString& OutError, TArrayView<const float>& GlobalVelParams)
{
	int32 MidiNote;
	bool bSuccess = JsonParsed->TryGetNumberField(TEXT("MidiNote"), MidiNote);
	if(!bSuccess || MidiNote < 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid Midi Note = %d!"), MidiNote);
		return false;
	}

	bool bHasDamper;
	if(MidiNote < PianoKeyObj->GetNoDamperStartingNote())
	    bHasDamper = true;
	else
	    bHasDamper = false;

	float SecondDecay;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("SecondDecay"), SecondDecay);
	if(!bSuccess || SecondDecay < 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid Second Decay = %f!"), SecondDecay);
		return false;
	}

	float SecondDecayTime;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("SecondDelayTime"), SecondDecayTime);
	if(!bSuccess || SecondDecayTime < 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid Second Decay Time = %f!"), SecondDecayTime);
		return false;
	}

	float BeatingScalePedalOff;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("BeatingScaleOff"), BeatingScalePedalOff);
	if(!bSuccess || BeatingScalePedalOff < 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid Beating Scale Pedal Off = %f!"), BeatingScalePedalOff);
		return false;
	}

	float BeatingScalePedalOn;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("BeatingScaleOn"), BeatingScalePedalOn);
	if(!bSuccess || BeatingScalePedalOn < 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid Beating Scale Pedal On = %f!"), BeatingScalePedalOn);
		return false;
	}

	float BaseFreq;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("BaseFreq"), BaseFreq);
	if(!bSuccess || BaseFreq < 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid base frequency = %f!"), BaseFreq);
		return false;
	}

	int32 NumBeats;
	bSuccess = JsonParsed->TryGetNumberField(TEXT("NumBeats"), NumBeats);
	if(!bSuccess || NumBeats < 0)
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has invalid number of beats = %d!"), NumBeats);
		return false;
	}

	if(!JsonParsed->HasField(TEXT("OnData")))
    	{
    		OutError = FString::Printf(TEXT("Imported piano key obj has no pedal on modal data!"));
    		return false;
    	}

	TArray<float> VelocityParams;
	if(JsonParsed->HasField(TEXT("VelocityParams")))
	{
		TArray<TSharedPtr<FJsonValue, ESPMode::ThreadSafe>> JArrayData = JsonParsed->GetArrayField(TEXT("VelocityParams"));
		VelocityParams.SetNumUninitialized(JArrayData.Num());
		if(JArrayData.Num() != 3)
		{
			OutError = FString::Printf(TEXT("Imported piano key obj has invalid number of velocity scale params!"));
			return false;
		}
		
		CopyFromJsonArray(VelocityParams.GetData(), JArrayData); 
	}
	else
	{
		VelocityParams.SetNumUninitialized(GlobalVelParams.Num());
		FMemory::Memcpy(VelocityParams.GetData(), GlobalVelParams.GetData(), GlobalVelParams.Num() * sizeof(float));
	}
		
	if(!JsonParsed->HasField(TEXT("OnData")))
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has no pedal on modal data!"));
		return false;
	}
		
	TArray<TSharedPtr<FJsonValue, ESPMode::ThreadSafe>> JArrayData = JsonParsed->GetArrayField(TEXT("OffData"));
	if(JArrayData.Num() % UPianoKeyObj::NumParamPerModal != 0)
	{
		OutError =  FString::Printf(TEXT("Imported piano key obj pedal off modals has invalid length!"));
		return false;	
	}

	TArray<float> PedalOffParams;
	PedalOffParams.SetNumUninitialized(JArrayData.Num());
	CopyFromJsonArray(PedalOffParams.GetData(), JArrayData); 

	int32 NumModals =  PedalOffParams.Num() / UPianoKeyObj::NumParamPerModal;

	
	JArrayData = JsonParsed->GetArrayField(TEXT("OnData"));
	if(JArrayData.Num() != PedalOffParams.Num())
	{
		OutError = FString::Printf(TEXT("Imported piano key obj has unequal pedal on and off data length!"));
		return false;	
	}

	TArray<float> PedalOnParams;
	PedalOnParams.SetNumUninitialized(JArrayData.Num());
	CopyFromJsonArray(PedalOnParams.GetData(), JArrayData); 

	return PianoKeyObj->AddPianoKeyData(MidiNote, NumModals, NumBeats, BaseFreq, SecondDecay, SecondDecayTime, BeatingScalePedalOff,
						BeatingScalePedalOn, bHasDamper, VelocityParams, PedalOffParams, PedalOnParams);
}

void UPianoKeyObjFactory::CopyFromJsonArray(float* OutData, TArray<TSharedPtr<FJsonValue, ESPMode::ThreadSafe>> JArrayData)
{
	int32 Index = 0;
	for(const TSharedPtr<FJsonValue> JEntry : JArrayData)
	{
		OutData[Index] = JEntry->AsNumber();
		Index++;
	}
}


bool UPianoKeyObjFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (UPianoKeyObj* PianoKeyObj = Cast<UPianoKeyObj>(Obj))
	{
		FString FilePath = PianoKeyObj->GetImportedSrcFilePath();
		if (FilePath.IsEmpty())
		{
			UE_LOG(LogVirtualInstrumentEditor, Warning, TEXT("Couldn't find source path for %s!"), *PianoKeyObj->GetPathName());
			return false;
		}
		OutFilenames.Push(FilePath);
		return true;
	}
	
	return false;
}

void UPianoKeyObjFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (UPianoKeyObj* AsMidiFile = Cast<UPianoKeyObj>(Obj))
	{
		AsMidiFile->AssetImportData->UpdateFilenameOnly(FPaths::ConvertRelativePathToFull(NewReimportPaths[0]));
	}
}

EReimportResult::Type UPianoKeyObjFactory::Reimport(UObject* Obj)
{
	// Only handle valid object
	if (!Obj || !Obj->IsA(UPianoKeyObj::StaticClass()))
		return EReimportResult::Failed;	

	UPianoKeyObj* PianoKeyObj = Cast<UPianoKeyObj>(Obj);
	check(PianoKeyObj);

	const FString Filename = PianoKeyObj->AssetImportData->GetFirstFilename();
	const FString FileExtension = FPaths::GetExtension(Filename);
	if (!FileExtension.Equals(FString(TEXT("pkeyobj")), ESearchCase::IgnoreCase))
		return EReimportResult::Failed;

	// If there is no file path provided, can't reimport from source
	if (!Filename.Len())
		return EReimportResult::Failed;

	UE_LOG(LogVirtualInstrumentEditor, Log, TEXT("Performing reimport of [%s]"), *Filename);

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		UE_LOG(LogVirtualInstrumentEditor, Warning, TEXT("-- cannot reimport: source file cannot be found."));
		return EReimportResult::Failed;
	}

	// Always suppress dialogs on re-import
	bPianoKeyObjFactorySuppressImportOverwriteDialog = true;

	bool OutCanceled = false;
	if (!ImportObject(PianoKeyObj->GetClass(), PianoKeyObj->GetOuter(), *PianoKeyObj->GetName(), RF_Public | RF_Standalone, Filename, nullptr, OutCanceled))
	{
		if (OutCanceled)
		{
			UE_LOG(LogVirtualInstrumentEditor, Warning, TEXT("-- import canceled"));
			return EReimportResult::Cancelled;
		}

		UE_LOG(LogVirtualInstrumentEditor, Warning, TEXT("-- import failed"));
		return EReimportResult::Failed;
	}

	UE_LOG(LogVirtualInstrumentEditor, Log, TEXT("-- imported successfully"));

	PianoKeyObj->AssetImportData->Update(Filename);
	PianoKeyObj->MarkPackageDirty();

	return EReimportResult::Succeeded;
}

void UPianoKeyObjFactory::CleanUp()
{
	Super::CleanUp();
}

