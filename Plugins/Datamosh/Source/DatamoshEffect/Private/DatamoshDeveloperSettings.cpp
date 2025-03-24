// Copyright 2025 Andrew Laptev. All Rights Reserved.

#include "DatamoshDeveloperSettings.h"
#include "DatamoshViewExtension.h"

UDatamoshDeveloperSettings::UDatamoshDeveloperSettings()
{
    RegisterConsoleVariableCallbacks();
}

void UDatamoshDeveloperSettings::OnDatamoshModeChanged(IConsoleVariable* Var)
{
    if (Var)
    {
        DatamoshMode = static_cast<EDatamoshModeSetting::Type>(Var->GetInt());
        UE_LOG(LogTemp, Verbose, TEXT("DatamoshDevSettings: DatamoshMode updated to %d"), DatamoshMode);
    }
}

void UDatamoshDeveloperSettings::OnEffectPreviewChanged(IConsoleVariable* Var)
{
    if (Var)
    {
        bEffectPreview = Var->GetBool();
        UE_LOG(LogTemp, Verbose, TEXT("DatamoshDevSettings: EffectPreview updated to %s"), bEffectPreview ? TEXT("true") : TEXT("false"));
    }
}

void UDatamoshDeveloperSettings::OnVelocityCompensationChanged(IConsoleVariable* Var)
{
    if (Var)
    {
        DatamoshVelocityCompensation = Var->GetFloat();
        UE_LOG(LogTemp, Verbose, TEXT("DatamoshDevSettings: DatamoshVelocityCompensation updated to %f"), DatamoshVelocityCompensation);
    }
}

void UDatamoshDeveloperSettings::OnEyeAdaptationOverrideChanged(IConsoleVariable* Var)
{
    if (Var)
    {
        EyeAdaptationPreExposureOverride = Var->GetInt();
        UE_LOG(LogTemp, Verbose, TEXT("DatamoshDevSettings: EyeAdaptationPreExposureOverride updated to %d"), EyeAdaptationPreExposureOverride);
    }
}

void UDatamoshDeveloperSettings::OnDatamoshStencilChanged(IConsoleVariable* Var)
{
    if (Var) 
    {
        DatamoshStencilValue = Var->GetInt();
        UE_LOG(LogTemp, Verbose, TEXT("DatamoshDevSettings: Datamosh Stencil updated to %d"), DatamoshStencilValue);
    }
}

void UDatamoshDeveloperSettings::RegisterConsoleVariableCallbacks()
{
    if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Datamosh")))
    {
        Var->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateUObject(this, &UDatamoshDeveloperSettings::OnDatamoshModeChanged));
    }

    if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Datamosh.Preview")))
    {
        Var->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateUObject(this, &UDatamoshDeveloperSettings::OnEffectPreviewChanged));
    }

    if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Datamosh.VelocityCompensation")))
    {
        Var->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateUObject(this, &UDatamoshDeveloperSettings::OnVelocityCompensationChanged));
    }

    if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Datamosh.MaskStencil")))
    {
        Var->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateUObject(this, &UDatamoshDeveloperSettings::OnDatamoshStencilChanged));
    }

    if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride")))
    {
        Var->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateUObject(this, &UDatamoshDeveloperSettings::OnEyeAdaptationOverrideChanged));
    }
}
