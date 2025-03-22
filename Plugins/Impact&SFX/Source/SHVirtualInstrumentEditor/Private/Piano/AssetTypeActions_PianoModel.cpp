// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "Piano/AssetTypeActions_PianoModel.h"

#include "Piano/PianoModel.h"
#include "SHVirtualInstrumentEditorLog.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		UClass* FAssetTypeActions_PianoModel::GetSupportedClass() const
		{
			return UPianoModel::StaticClass();
		}
		
		FColor FAssetTypeActions_PianoModel::GetTypeColor() const
		{
			return FColor::Cyan;
		}
	 
		uint32 FAssetTypeActions_PianoModel::GetCategories()
		{
			if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
			{
				return FAssetToolsModule::GetModule().Get().FindAdvancedAssetCategory(FVirtualInstrumentEditorConst::VirtualInstrumentCategory);
			}
			else
				return EAssetTypeCategories::Misc;
		}

		const TArray<FText>& FAssetTypeActions_PianoModel::GetSubMenus() const
		{
			static const TArray<FText> SubMenus
			{
				FVirtualInstrumentEditorConst::VirtualInstrumentSubCategory
			};
			return SubMenus;
		}

		void FAssetTypeActions_PianoModel::OpenAssetEditor(const TArray<UObject*>& InObjects,
			TSharedPtr<IToolkitHost> ToolkitHost)
		{
			return FAssetTypeActions_Base::OpenAssetEditor(InObjects, ToolkitHost);
			
			// EToolkitMode::Type Mode = ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
			// for (UObject* Object : InObjects)
			// {
			// 	if (UPianoModel* PianoModel = Cast<UPianoModel>(Object))
			// 	{
			// 		TSharedRef<FPianoModelEditorToolkit> Editor = MakeShared<FPianoModelEditorToolkit>();
			// 		Editor->Init(Mode, ToolkitHost, PianoModel);
			// 	}
			// }
		}
	}
}

#undef LOCTEXT_NAMESPACE