// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "SoundboardObjFactory.h"

#include "SHVirtualInstrumentEditorLog.h"
#include "Runtime/Launch/Resources/Version.h"
#include "SoundboardObj.h"
#include "EditorFramework/AssetImportData.h"

static bool bSoundboardObjFactorySuppressImportOverwriteDialog = false;

USoundboardObjFactory::USoundboardObjFactory()
{
	bCreateNew = false;
	SupportedClass = USoundboardObj::StaticClass();
	bEditorImport = true;
	bText = true;
	
	Formats.Add(TEXT("sbobj;Soundboard Obj file"));
	
	bAutomatedReimport = true;
}

UObject* USoundboardObjFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	// if already exists, remember the user settings
	USoundboardObj* ExisingSoundboardObj = FindObject<USoundboardObj>(InParent, *InName.ToString());

	bool bUseExistingSettings = bSoundboardObjFactorySuppressImportOverwriteDialog;

	if (ExisingSoundboardObj && !bUseExistingSettings && !GIsAutomationTesting)
	{
		DisplayOverwriteOptionsDialog(FText::Format(
			NSLOCTEXT("SoundboardObjFactory", "ImportOverwriteWarning", "You are about to import '{0}' over an existing soundboard obj."),
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
	bSoundboardObjFactorySuppressImportOverwriteDialog = false;

	// Use pre-existing obj if it exists and we want to keep settings,
	// otherwise create new obj and import raw data.
	USoundboardObj* SoundboardObj = (bUseExistingSettings && ExisingSoundboardObj) ? ExisingSoundboardObj : NewObject<USoundboardObj>(InParent, InName, Flags);
	if(ImportFromText(SoundboardObj, Buffer, BufferEnd))
	{
		// Store the current file path and timestamp for re-import purposes
		SoundboardObj->AssetImportData->Update(CurrentFilename);
		
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, SoundboardObj);
		return SoundboardObj;
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
	return nullptr;
}

bool USoundboardObjFactory::ImportFromText(USoundboardObj* SoundboardObj,  const TCHAR*& Buffer, const TCHAR* BufferEnd)
{
	TSharedPtr<FJsonObject> JsonParsed;
	TStringView<TCHAR> JsonString = TStringView<TCHAR>(Buffer, (BufferEnd - Buffer)); 
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::CreateFromView(JsonString);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
	{
		int32 Version;
		bool bSuccess = JsonParsed->TryGetNumberField(TEXT("Version"), Version);
		if(!bSuccess || Version <= 0)
		{
			UE_LOG(LogVirtualInstrumentEditor, Error, TEXT("Imported soundboard obj has invalid version = %d!"), Version);
			return false;
		}

		if(!JsonParsed->HasField(TEXT("ModalData")))
		{
			UE_LOG(LogVirtualInstrumentEditor, Error, TEXT("Imported soundboard obj has no modal data!"));
			return false;
		}
		
		TArray<TSharedPtr<FJsonValue, ESPMode::ThreadSafe>> JArrayData = JsonParsed->GetArrayField(TEXT("ModalData"));
		if(JArrayData.Num() % USoundboardObj::NumParamPerModal != 0)
		{
			UE_LOG(LogVirtualInstrumentEditor, Error, TEXT("Imported soundboard obj modal data has invalid length!"));
			return false;	
		}
		
		Audio::FAlignedFloatBuffer ParamsBuffer;
		ParamsBuffer.SetNumUninitialized(JArrayData.Num());
		int32 Index = 0;
		for(const TSharedPtr<FJsonValue> JEntry : JArrayData)
		{
			ParamsBuffer[Index] = JEntry->AsNumber();
			Index++;
		}
		int32 NumModals =  ParamsBuffer.Num() / USoundboardObj::NumParamPerModal;
	
		SoundboardObj->SetData(Version, NumModals, ParamsBuffer);
		return true;
	}

	return false;
}


bool USoundboardObjFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (USoundboardObj* SoundboardObj = Cast<USoundboardObj>(Obj))
		return true;
	
	return false;
}

void USoundboardObjFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	USoundboardObj* SoundboardObj = Cast<USoundboardObj>(Obj);
	if (SoundboardObj && ensure(NewReimportPaths.Num() == 1))
	{
		SoundboardObj->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type USoundboardObjFactory::Reimport(UObject* Obj)
{
	// Only handle valid object
	if (!Obj || !Obj->IsA(USoundboardObj::StaticClass()))
		return EReimportResult::Failed;	

	USoundboardObj* SoundboardObj = Cast<USoundboardObj>(Obj);
	check(SoundboardObj);

	const FString Filename = SoundboardObj->AssetImportData->GetFirstFilename();
	const FString FileExtension = FPaths::GetExtension(Filename);
	if (!FileExtension.Equals(FString(TEXT("sbobj")), ESearchCase::IgnoreCase))
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
	bSoundboardObjFactorySuppressImportOverwriteDialog = true;

	bool OutCanceled = false;
	if (!ImportObject(SoundboardObj->GetClass(), SoundboardObj->GetOuter(), *SoundboardObj->GetName(), RF_Public | RF_Standalone, Filename, nullptr, OutCanceled))
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

	SoundboardObj->AssetImportData->Update(Filename);
	SoundboardObj->MarkPackageDirty();

	return EReimportResult::Succeeded;
}

void USoundboardObjFactory::CleanUp()
{
	Super::CleanUp();
}

