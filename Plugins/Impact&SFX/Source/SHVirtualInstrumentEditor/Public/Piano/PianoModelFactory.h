// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "PianoModelFactory.generated.h"

/**
 * 
 */
UCLASS()
class SHVIRTUALINSTRUMENTEDITOR_API UPianoModelFactory : public UFactory
{
	GENERATED_BODY()
	
public:
	UPianoModelFactory();
	
	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);
};
