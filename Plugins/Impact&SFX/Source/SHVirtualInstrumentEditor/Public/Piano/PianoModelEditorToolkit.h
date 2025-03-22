// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		class SHVIRTUALINSTRUMENTEDITOR_API FPianoModelEditorToolkit : public FAssetEditorToolkit
		{
		public:
			void Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InParentObject);

			/** FAssetEditorToolkit interface */
			virtual FName GetToolkitFName() const override { return TEXT("PianoModelEditor"); }
			virtual FText GetBaseToolkitName() const override { return INVTEXT("Piano Model Editor"); }
			virtual FString GetWorldCentricTabPrefix() const override { return TEXT("Piano Model"); }
			virtual FLinearColor GetWorldCentricTabColorScale() const override { return {}; }
			virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
			virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
			
		private:
			/** Properties tab */
			TSharedPtr<IDetailsView> PropertiesView;
			
			static const FName AppIdentifier;
			static const FName PropertiesTabId;
			
			TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);
		};
	}
}