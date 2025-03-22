// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "Extend/RCurveExtendEditorToolkit.h"

#include "ImpactSynthCurveEditorModel.h"
#include "RichCurveEditorModel.h"
#include "SCurveEditorPanel.h"
#include "Extend/RCurveExtend.h"
#include "Widgets/Docking/SDockTab.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "RCurveDataEditor"

namespace LBSImpactSFXSynth
{
	namespace Editor
	{
		const FName FRCurveExtendEditorToolkit::AppIdentifier(TEXT("RCurveExtendEditorApp"));
		const FName FRCurveExtendEditorToolkit::PropertiesTabId(TEXT("RCurveExtendEditor_Properties"));
		const FName FRCurveExtendEditorToolkit::CurveTabId(TEXT("RCurveExtendEditor_Curves"));
		
		void FRCurveExtendEditorToolkit::Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InParentObject)
		{
			check(InParentObject);
			
			CurveEditor = MakeShared<FCurveEditor>();
			FCurveEditorInitParams InitParams;
			CurveEditor->InitCurveEditor(InitParams);
			CurveEditor->GridLineLabelFormatXAttribute = LOCTEXT("GridXLabelFormat", "{0}");

			TUniquePtr<ICurveEditorBounds> EditorBounds = MakeUnique<FStaticCurveEditorBounds>();
			EditorBounds->SetInputBounds(0.0, 1.0);
			CurveEditor->SetBounds(MoveTemp(EditorBounds));
		
			if(URCurveExtend* CurveExtend = Cast<URCurveExtend>(InParentObject))
			{
				RefreshSampleCurve(CurveExtend);
				
				if(CurveExtend->bIsShowReSample)
					AddReSampleCurveToEditor(CurveExtend);
				
				TUniquePtr<FRichCurveEditorModelRaw> NewCurve = MakeUnique<FRichCurveEditorModelRaw>(&CurveExtend->Curve, CurveExtend);
				NewCurve->SetColor(FImpactSynthCurveModel::GetColorCyan());
				EditingCurveModelID = CurveEditor->AddCurve(MoveTemp(NewCurve));
				CurveEditor->PinCurve(EditingCurveModelID);

				CurveExtend->OnDataBaked.BindRaw(this, &FRCurveExtendEditorToolkit::OnRCurveExtendDataBaked);
				CurveExtend->OnReSampleCurveShow.BindRaw(this, &FRCurveExtendEditorToolkit::OnReSampleCurveShow);
			}
			
			CurvePanel = SNew(SCurveEditorPanel, CurveEditor.ToSharedRef());
			CurvePanel->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
			
			// Support undo/redo
			InParentObject->SetFlags(RF_Transactional);
			GEditor->RegisterForUndo(this);

			FDetailsViewArgs Args;
			Args.bHideSelectionTip = true;
			Args.NotifyHook = this;
			
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertiesView = PropertyModule.CreateDetailView(Args);
			PropertiesView->SetObject(InParentObject);

			TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("RCurveExtendEditor_Layoutv1")
			->AddArea
			(
				FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.9f)
					->SetOrientation(Orient_Horizontal)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.1f)
						->AddTab(PropertiesTabId, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewSplitter()
						->SetSizeCoefficient(0.9f)
						->SetOrientation(Orient_Vertical)
						->Split
						(
							FTabManager::NewStack()
							->SetHideTabWell(true)
							->SetSizeCoefficient(1.f)
							->AddTab(CurveTabId, ETabState::OpenedTab)
						)
					)
				)
			);

			const bool bCreateDefaultStandaloneMenu = true;
			const bool bCreateDefaultToolbar = true;
			const bool bToolbarFocusable = false;
			const bool bUseSmallIcons = true;
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

			AddToolbarExtender(CurvePanel->GetToolbarExtender());

			if (CurveEditor.IsValid())
			{
				RegenerateMenusAndToolbars();
			}
		}

		void FRCurveExtendEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
		{
			WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_RCurveExtendEditor", "Rich Curve Extend Editor"));
			FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
			
			InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FRCurveExtendEditorToolkit::SpawnTab_Properties))
						.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
						.SetGroup(WorkspaceMenuCategory.ToSharedRef())
						.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "RCurveExtendEditor.Tabs.Details"));

			FSlateIcon CurveIcon(FAppStyle::GetAppStyleSetName(), "RCurveExtendEditor.Tabs.Properties");
			InTabManager->RegisterTabSpawner(CurveTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args)
			{
				return SpawnTab_OutputCurve(Args);
			}))
			.SetDisplayName(LOCTEXT("ModificationCurvesTab", "Modification Curves"))
			.SetGroup(WorkspaceMenuCategory.ToSharedRef())
			.SetIcon(CurveIcon);
		}

		void FRCurveExtendEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
		{
			FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
			InTabManager->UnregisterTabSpawner(CurveTabId);
			InTabManager->UnregisterTabSpawner(PropertiesTabId);
		}

		void FRCurveExtendEditorToolkit::OnClose()
		{
			if(HasEditingObject())
			{
				if(URCurveExtend* CurveExtend = Cast<URCurveExtend>(GetEditingObject()))
				{
					CurveExtend->OnDataBaked.Unbind();
					CurveExtend->OnReSampleCurveShow.Unbind();
				}
			}
			
			FAssetEditorToolkit::OnClose();
			
		}

		void FRCurveExtendEditorToolkit::PostUndo(bool bSuccess)
		{
			FEditorUndoClient::PostUndo(bSuccess);
			OnUnDoRedo(bSuccess);
		}

		void FRCurveExtendEditorToolkit::PostRedo(bool bSuccess)
		{
			FEditorUndoClient::PostRedo(bSuccess);
			OnUnDoRedo(bSuccess);
		}

		void FRCurveExtendEditorToolkit::OnUnDoRedo(bool bSuccess)
		{
			if(IsHosted() && HasEditingObject())
			{
				URCurveExtend* RCurveExtend = Cast<URCurveExtend>(GetEditingObject());
				if(!bSuccess || RCurveExtend == nullptr)
					return;
				
				RefreshSampleCurve(RCurveExtend);
				OnReSampleCurveShow(RCurveExtend->bIsShowReSample);
			}
		}

		void FRCurveExtendEditorToolkit::OnRCurveExtendDataBaked()
		{
			//Use try catch to avoid crashing due to failed to show data.
			//This might happen when the data is saved by closing the editor  
			try
			{
				// Check to make sure the editor is actually open  
				if(HasEditingObject() && IsHosted())
				{
					URCurveExtend* RCurveExtend = Cast<URCurveExtend>(GetEditingObject());
					if(RCurveExtend == nullptr)
						return;

					RefreshSampleCurve(RCurveExtend);
				}	
			}
			catch (...)
			{
			}
		}

		void FRCurveExtendEditorToolkit::OnReSampleCurveShow(bool bShow)
		{
			if(IsHosted() && HasEditingObject())
			{
				URCurveExtend* RCurveExtend = Cast<URCurveExtend>(GetEditingObject());
				if(RCurveExtend == nullptr)
					return;
			
				if(bShow)
				{
					AddReSampleCurveToEditor(RCurveExtend);
				}
				else
				{
					if(CurveEditor->FindCurve(ReSampleCurveModelID))
						CurveEditor->RemoveCurve(ReSampleCurveModelID);
				}
			}
		}

		void FRCurveExtendEditorToolkit::RefreshSampleCurve(URCurveExtend* CurveExtend)
		{
			ReSampleCurve.Reset();
			float KeyTime = CurveExtend->MinTime;				
			for(float SampleValue : CurveExtend->Data)
			{
				ReSampleCurve.AddKey(KeyTime, SampleValue);
				KeyTime += CurveExtend->TimeStep;
			}
		}

		void FRCurveExtendEditorToolkit::AddReSampleCurveToEditor(URCurveExtend* CurveExtend)
		{
			if(CurveEditor->GetCurves().Num() > 1 && CurveEditor->FindCurve(ReSampleCurveModelID))
				return;
			
			TUniquePtr<FRichCurveEditorModelRaw> SampleCurve = MakeUnique<FRichCurveEditorModelRaw>(&ReSampleCurve, CurveExtend);
			SampleCurve->SetColor(FColor { 255, 25, 25, 255 });
			SampleCurve->SetIsReadOnly(true);
			SampleCurve->SetIsKeyDrawEnabled(true);
			ReSampleCurveModelID = CurveEditor->AddCurve(MoveTemp(SampleCurve));
		}

		TSharedRef<SDockTab> FRCurveExtendEditorToolkit::SpawnTab_OutputCurve(const FSpawnTabArgs& Args)
		{
			CurveEditor->ZoomToFitAll();
			
			TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
				.Label(FText::Format(LOCTEXT("RCurveExtendEditorTitle", "Modification Curve: {0}"), FText::FromString(GetEditingObject()->GetName())))
				.TabColorScale(GetTabColorScale())
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						CurvePanel.ToSharedRef()
					]
				];

			return NewDockTab;
		}

		TSharedRef<SDockTab> FRCurveExtendEditorToolkit::SpawnTab_Properties(const FSpawnTabArgs& Args)
		{
			check(Args.GetTabId() == PropertiesTabId);

			return SNew(SDockTab)
				.Label(LOCTEXT("RCurveExtendDetailsTitle", "Details"))
				[
					PropertiesView.ToSharedRef()
				];
		}
	}
}

#undef LOCTEXT_NAMESPACE