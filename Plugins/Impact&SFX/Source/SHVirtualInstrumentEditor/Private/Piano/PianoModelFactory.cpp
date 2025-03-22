// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Piano/PianoModelFactory.h"
#include "Piano/PianoModel.h"

UPianoModelFactory::UPianoModelFactory()
{
	SupportedClass = UPianoModel::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UPianoModelFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
												   UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPianoModel>(InParent, Class, Name, Flags, Context);
}
