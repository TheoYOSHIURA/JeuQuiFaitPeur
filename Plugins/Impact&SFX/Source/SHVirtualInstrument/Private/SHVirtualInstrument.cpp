// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "SHVirtualInstrument.h"
#include "MetasoundFrontendRegistries.h"
#include "Piano/MetasoundPianoModel.h"

#define LOCTEXT_NAMESPACE "FSHVirtualInstrumentModule"

namespace Metasound
{
	REGISTER_METASOUND_DATATYPE(Metasound::FPianoModel, "PianoModel", Metasound::ELiteralType::UObjectProxy, UPianoModel);
}

void FSHVirtualInstrumentModule::StartupModule()
{
	// All MetaSound interfaces are required to be loaded prior to registering & loading MetaSound assets,
	// so check that the MetaSoundEngine is loaded prior to pending Modulation defined classes
	FModuleManager::Get().LoadModuleChecked("MetasoundEngine");
	
	FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();
}

void FSHVirtualInstrumentModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FSHVirtualInstrumentModule, SHVirtualInstrument)