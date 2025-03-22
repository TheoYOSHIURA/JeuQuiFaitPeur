// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "AssetTypeActions_SoundboardObj.h"

#include "SoundboardObj.h"
#include "SHVirtualInstrumentEditorLog.h"
#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		UClass* FAssetTypeActions_SoundboardObj::GetSupportedClass() const
		{
			return USoundboardObj::StaticClass();
		}
		
		FColor FAssetTypeActions_SoundboardObj::GetTypeColor() const
		{
			return FColor::Red;
		}
	 
		uint32 FAssetTypeActions_SoundboardObj::GetCategories()
		{
			if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
			{
				return FAssetToolsModule::GetModule().Get().FindAdvancedAssetCategory(FVirtualInstrumentEditorConst::VirtualInstrumentCategory);
			}
			else
				return EAssetTypeCategories::Misc;
		}

		void FAssetTypeActions_SoundboardObj::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
		{
			for (auto& Asset : TypeAssets)
			{
				const auto SoundboardObj = CastChecked<USoundboardObj>(Asset);
				SoundboardObj->AssetImportData->ExtractFilenames(OutSourceFilePaths);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE