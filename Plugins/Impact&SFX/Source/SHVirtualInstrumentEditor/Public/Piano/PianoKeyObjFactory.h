// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Piano/PianoKeyObj.h"
#include "Factories/Factory.h"
#include "PianoKeyObjFactory.generated.h"

class UPianoKeyObj;


UCLASS()
class SHVIRTUALINSTRUMENTEDITOR_API UPianoKeyObjFactory: public UFactory, public FReimportHandler
{
	GENERATED_BODY()
	
public:
	UPianoKeyObjFactory();
	
	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;

	bool ImportFromText(UPianoKeyObj* PianoKeyObj, const TCHAR*& Buffer, const TCHAR* BufferEnd);
	
	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths ) override;
	virtual EReimportResult::Type Reimport( UObject* Obj ) override;
	//~ End FReimportHandler Interface

	//~ Being UFactory Interface
	virtual void CleanUp() override;
	//~ End UFactory Interface
	
private:
	bool InitDataFromJson(UPianoKeyObj* PianoKeyObj, const TSharedPtr<FJsonObject>& JsonParsed, FString& OutError);
	bool ProcessOneKeyFromJson(UPianoKeyObj* PianoKeyObj, const TSharedPtr<FJsonObject>& JsonParsed, FString& OutError, TArrayView<const float>& GlobalVelParams);
	static void CopyFromJsonArray(float* OutData, TArray<TSharedPtr<FJsonValue, ESPMode::ThreadSafe>> JArrayData);
};

