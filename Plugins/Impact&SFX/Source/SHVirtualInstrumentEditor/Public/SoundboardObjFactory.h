// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "SoundboardObjFactory.generated.h"

class USoundboardObj;


UCLASS()
class SHVIRTUALINSTRUMENTEDITOR_API USoundboardObjFactory: public UFactory, public FReimportHandler
{
	GENERATED_BODY()
	
public:
	USoundboardObjFactory();
	
	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;

	bool ImportFromText(USoundboardObj* SoundboardObj, const TCHAR*& Buffer, const TCHAR* BufferEnd);

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths ) override;
	virtual EReimportResult::Type Reimport( UObject* Obj ) override;
	//~ End FReimportHandler Interface

	//~ Being UFactory Interface
	virtual void CleanUp() override;
	//~ End UFactory Interface
};

