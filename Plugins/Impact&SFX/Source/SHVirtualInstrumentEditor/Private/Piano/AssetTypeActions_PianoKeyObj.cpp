// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "Piano/AssetTypeActions_PianoKeyObj.h"

#include "EditorReimportHandler.h"
#include "Piano/PianoKeyObj.h"
#include "SHVirtualInstrumentEditorLog.h"
#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		UClass* FAssetTypeActions_PianoKeyObj::GetSupportedClass() const
		{
			return UPianoKeyObj::StaticClass();
		}
		
		FColor FAssetTypeActions_PianoKeyObj::GetTypeColor() const
		{
			return FColor::Red;
		}
	 
		uint32 FAssetTypeActions_PianoKeyObj::GetCategories()
		{
			if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
			{
				return FAssetToolsModule::GetModule().Get().FindAdvancedAssetCategory(FVirtualInstrumentEditorConst::VirtualInstrumentCategory);
			}
			else
				return EAssetTypeCategories::Misc;
		}

		const TArray<FText>& FAssetTypeActions_PianoKeyObj::GetSubMenus() const
		{
			static const TArray<FText> SubMenus
			{
				INVTEXT("PianoCategory")
			};
			return SubMenus;
		}

		void FAssetTypeActions_PianoKeyObj::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
		{
			for (auto& Asset : TypeAssets)
			{
				const auto PianoKeyObj = CastChecked<UPianoKeyObj>(Asset);
				PianoKeyObj->AssetImportData->ExtractFilenames(OutSourceFilePaths);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE