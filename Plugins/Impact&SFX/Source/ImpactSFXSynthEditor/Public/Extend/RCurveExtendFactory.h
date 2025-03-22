// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "RCurveExtendFactory.generated.h"

class URCurveExtend;

UCLASS()
class IMPACTSFXSYNTHEDITOR_API URCurveExtendFactory : public UFactory
{
	GENERATED_BODY()
	
public:
	URCurveExtendFactory();
	
	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);
};
