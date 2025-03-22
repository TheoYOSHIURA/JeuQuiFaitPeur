// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Extend/RCurveExtendFactory.h"

#include "Extend/RCurveExtend.h"

URCurveExtendFactory::URCurveExtendFactory()
{
	SupportedClass = URCurveExtend::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* URCurveExtendFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
												UObject* Context, FFeedbackContext* Warn)
{
	URCurveExtend* NewData = NewObject<URCurveExtend>(InParent, Class, Name, Flags, Context);
	return NewData;
}
