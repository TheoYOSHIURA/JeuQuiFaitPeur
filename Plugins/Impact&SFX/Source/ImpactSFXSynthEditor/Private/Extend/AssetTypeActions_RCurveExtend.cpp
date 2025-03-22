// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Extend/AssetTypeActions_RCurveExtend.h"
#include "ImpactSFXSynthEditor.h"
#include "Extend/RCurveExtend.h"
#include "Extend/RCurveExtendEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace LBSImpactSFXSynth
{
	namespace Editor
	{
		UClass* FAssetTypeActions_RCurveExtend::GetSupportedClass() const
		{
			return URCurveExtend::StaticClass();
		}
		
		FColor FAssetTypeActions_RCurveExtend::GetTypeColor() const
		{
			return FColor::Cyan;
		}
	 
		uint32 FAssetTypeActions_RCurveExtend::GetCategories()
		{
			if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
			{
				return FAssetToolsModule::GetModule().Get().FindAdvancedAssetCategory(FImpactSFXSynthEditorModule::ImpactSynthCategory);
			}
			else
				return EAssetTypeCategories::Misc;
		}

		void FAssetTypeActions_RCurveExtend::OpenAssetEditor(const TArray<UObject*>& InObjects,
			TSharedPtr<IToolkitHost> ToolkitHost)
		{
			EToolkitMode::Type Mode = ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

			for (UObject* Object : InObjects)
			{
				if (URCurveExtend* CurveExtend = Cast<URCurveExtend>(Object))
				{
					TSharedRef<FRCurveExtendEditorToolkit> Editor = MakeShared<FRCurveExtendEditorToolkit>();
					Editor->Init(Mode, ToolkitHost, CurveExtend);
				}
			}
		}
		
	}
}

#undef LOCTEXT_NAMESPACE