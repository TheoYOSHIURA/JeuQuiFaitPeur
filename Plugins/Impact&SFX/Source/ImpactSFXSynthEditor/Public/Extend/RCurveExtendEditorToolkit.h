// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CurveEditor.h"
#include "EditorUndoClient.h"
#include "Extend/RCurveExtend.h"
#include "Curves/RichCurve.h"
#include "Toolkits/AssetEditorToolkit.h"

namespace LBSImpactSFXSynth
{
	namespace Editor
	{
		class IMPACTSFXSYNTHEDITOR_API FRCurveExtendEditorToolkit : public FAssetEditorToolkit, public FNotifyHook, public FEditorUndoClient
		{
		public:
			void Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InParentObject);

			/** FAssetEditorToolkit interface */
			virtual FName GetToolkitFName() const override { return TEXT("RCurveExtendEditor"); }
			virtual FText GetBaseToolkitName() const override { return INVTEXT("Rich Curve Extend Editor"); }
			virtual FString GetWorldCentricTabPrefix() const override { return TEXT("Rich Curve Extend "); }
			virtual FLinearColor GetWorldCentricTabColorScale() const override { return {}; }
			virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
			virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

			virtual void OnClose() override;
			
		protected:
			FCurveModelID EditingCurveModelID = FCurveModelID::Unique();
			FCurveModelID ReSampleCurveModelID = FCurveModelID::Unique();

			/* Curve used purely for display.  May be down-sampled from
			 * asset's curve for performance while editing */
			TSharedPtr<FRichCurve> ExpressionCurve;
			
			virtual void PostUndo(bool bSuccess) override;
			virtual void PostRedo(bool bSuccess) override;
			void OnUnDoRedo(bool bSuccess);
			
			void OnRCurveExtendDataBaked();
			void OnReSampleCurveShow(bool bShow);
			
			void RefreshSampleCurve(URCurveExtend* CurveExtend);
			void AddReSampleCurveToEditor(URCurveExtend* CurveExtend);
		private:
			TSharedPtr<FCurveEditor> CurveEditor;
			TSharedPtr<SCurveEditorPanel> CurvePanel;

			/** Properties tab */
			TSharedPtr<IDetailsView> PropertiesView;

			static const FName AppIdentifier;
			static const FName PropertiesTabId;
			static const FName CurveTabId;

			FRichCurve ReSampleCurve;
			
			/**	Spawns the tab allowing for editing/viewing the output curve(s) */
			TSharedRef<SDockTab> SpawnTab_OutputCurve(const FSpawnTabArgs& Args);

			/**	Spawns the tab allowing for editing/viewing the output curve(s) */
			TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);
		};
	}
}