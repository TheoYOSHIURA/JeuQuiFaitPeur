// Copyright 2025 Andrew Laptev. All Rights Reserved.

#include "DatamoshBlueprintLibrary.h"
#include "DatamoshViewExtension.h"
#include "Engine/Engine.h"

FDatamoshParameters UDatamoshBlueprintLibrary::DefaultParameters = FDatamoshParameters();

TEnumAsByte<EDatamoshModeSetting::Type> UDatamoshBlueprintLibrary::SetDatamoshMode(TEnumAsByte<EDatamoshModeSetting::Type> Mode)
{
    IConsoleVariable* CVarDatamosh = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Datamosh"));
    int ModeIndex = static_cast<int>(Mode);
    CVarDatamosh->Set(ModeIndex, ECVF_SetByProjectSetting);
    return Mode;
}

bool UDatamoshBlueprintLibrary::SetDatamoshActive(bool bActive)
{
    if (FDatamoshViewExtension* ViewExtension = FDatamoshViewExtension::GetActiveInstance())
    {
        ViewExtension->SetDatamoshActive(bActive);
        UE_LOG(LogTemp, Verbose, TEXT("DatamoshLib: SetDatamoshActive: %d"), ViewExtension->GetDatamoshActive());

    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DatamoshLib: SetDatamoshActive: Cannot find active instance"));
    }
    return bActive;
}

void UDatamoshBlueprintLibrary::ResetRenderTarget()
{
    IConsoleVariable* CVarDatamoshReset = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Datamosh.Reset"));
    if (CVarDatamoshReset)
    {
        CVarDatamoshReset->AsCommand();
    }
}

TEnumAsByte<EDatamoshModeSetting::Type> UDatamoshBlueprintLibrary::GetDatamoshMode()
{
    IConsoleVariable* CVarDatamosh = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Datamosh"));
    int ModeIndex = CVarDatamosh->GetInt();
    return static_cast<TEnumAsByte<EDatamoshModeSetting::Type>>(ModeIndex);
}

bool UDatamoshBlueprintLibrary::GetDatamoshActive()
{
    bool bActive = false;
    if (FDatamoshViewExtension* ViewExtension = FDatamoshViewExtension::GetActiveInstance())
    {
        bActive = ViewExtension->GetDatamoshActive();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DatamoshLib: GetDatamoshActive: Cannot find active instance"));
    }
    return bActive;
}

FDatamoshParameters UDatamoshBlueprintLibrary::GetDatamoshParameters()
{
    FDatamoshParameters Parameters;
    if (FDatamoshViewExtension* ViewExtension = FDatamoshViewExtension::GetActiveInstance())
    {
        Parameters = ViewExtension->GetDatamoshParameters();
    }
    return Parameters;
}

void UDatamoshBlueprintLibrary::ResetToDefaultParameters()
{
    if (FDatamoshViewExtension* ViewExtension = FDatamoshViewExtension::GetActiveInstance())
    {
        ViewExtension->UpdateDatamoshParameters(DefaultParameters);
    }
}

void UDatamoshBlueprintLibrary::SetDatamoshParameters(FDatamoshParameters Parameters)
{
    if (FDatamoshViewExtension* ViewExtension = FDatamoshViewExtension::GetActiveInstance())
    {
        ViewExtension->UpdateDatamoshParameters(Parameters);
    }
}