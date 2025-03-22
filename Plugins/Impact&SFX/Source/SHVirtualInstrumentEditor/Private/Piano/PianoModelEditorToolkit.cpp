// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Piano/PianoModelEditorToolkit.h"

#include "Piano/PianoModel.h"
#include "DSP/FloatArrayMath.h"
#include "Widgets/Docking/SDockTab.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "PianoModelEditor"

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		const FName FPianoModelEditorToolkit::AppIdentifier(TEXT("PianoModelEditorApp"));
		const FName FPianoModelEditorToolkit::PropertiesTabId(TEXT("PianoModelEditor_Properties"));
		
		void FPianoModelEditorToolkit::Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InParentObject)
		{
			check(InParentObject);

			FDetailsViewArgs Args;
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertiesView = PropertyModule.CreateDetailView(Args);
			PropertiesView->SetObject(InParentObject);

			TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("PianoModelEditor_Layoutv1")
			->AddArea
			(
				FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.95f)
					->AddTab(PropertiesTabId, ETabState::OpenedTab)
				)
			);

			constexpr  bool bCreateDefaultStandaloneMenu = true;
			constexpr bool bCreateDefaultToolbar = true;
			constexpr bool bToolbarFocusable = false;
			constexpr bool bUseSmallIcons = true;
			FAssetEditorToolkit::InitAssetEditor(
				Mode,
				InitToolkitHost,
				AppIdentifier,
				StandaloneDefaultLayout,
				bCreateDefaultStandaloneMenu,
				bCreateDefaultToolbar,
				InParentObject,
				bToolbarFocusable,
				bUseSmallIcons);
		}

		void FPianoModelEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
		{
			WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_PianoModelEditor", "Multi Impact Data Editor"));
			FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
			
			InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FPianoModelEditorToolkit::SpawnTab_Properties))
						.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
						.SetGroup(WorkspaceMenuCategory.ToSharedRef())
						.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "PianoModelEditor.Tabs.Details"));
		}

		void FPianoModelEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
		{
			FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
			InTabManager->UnregisterTabSpawner(PropertiesTabId);
		}

		TSharedRef<SDockTab> FPianoModelEditorToolkit::SpawnTab_Properties(const FSpawnTabArgs& Args)
		{
			check(Args.GetTabId() == PropertiesTabId);

			return SNew(SDockTab)
				.Label(LOCTEXT("PianoModelDetailsTitle", "Details"))
				[
					PropertiesView.ToSharedRef()
				];
		}
	}
}

#undef LOCTEXT_NAMESPACE