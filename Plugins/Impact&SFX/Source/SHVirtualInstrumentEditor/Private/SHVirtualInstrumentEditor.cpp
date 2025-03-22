// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "SHVirtualInstrumentEditor.h"

#include "PropertyEditorModule.h"
#include "MetasoundEditorModule.h"
#include "SHVirtualInstrumentEditorLog.h"

#define LOCTEXT_NAMESPACE "FSHVirtualInstrumentEditorModule"

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		void FSHVirtualInstrumentEditorModule::StartupModule()
		{
			Metasound::Editor::IMetasoundEditorModule& MetasoundEditorModule = FModuleManager::GetModuleChecked<Metasound::Editor::IMetasoundEditorModule>("MetasoundEditor");
			MetasoundEditorModule.RegisterPinType("PianoModel");
			
			SoundboardObjTypeActions = MakeShared<FAssetTypeActions_SoundboardObj>();
			FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(SoundboardObjTypeActions.ToSharedRef());
			
			PianoKeyObjTypeActions = MakeShared<FAssetTypeActions_PianoKeyObj>();
			FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(PianoKeyObjTypeActions.ToSharedRef());
			
			PianoModelTypeActions = MakeShared<FAssetTypeActions_PianoModel>();
			FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(PianoModelTypeActions.ToSharedRef());
			
			UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSHVirtualInstrumentEditorModule::RegisterMenus));
		}

		void FSHVirtualInstrumentEditorModule::ShutdownModule()
		{
			if (!FModuleManager::Get().IsModuleLoaded("AssetTools"))
				return;
			
			FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(SoundboardObjTypeActions.ToSharedRef());
			FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(PianoKeyObjTypeActions.ToSharedRef());
			FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(PianoModelTypeActions.ToSharedRef());
		}

		void FSHVirtualInstrumentEditorModule::RegisterMenus()
		{
			FToolMenuOwnerScoped MenuOwner(this);
		}
	}
}

IMPLEMENT_MODULE(LBSVirtualInstrument::Editor::FSHVirtualInstrumentEditorModule, SHVirtualInstrumentEditor)
#undef LOCTEXT_NAMESPACE