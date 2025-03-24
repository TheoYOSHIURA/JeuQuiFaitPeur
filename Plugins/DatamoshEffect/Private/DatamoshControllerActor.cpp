// Copyright 2025 Andrew Laptev. All Rights Reserved.

#include "DatamoshControllerActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"

ADatamoshControllerActor::ADatamoshControllerActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Mode = EDatamoshModeSetting::Off;
    bEnabled = true;

    PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("DatamoshPostProcessComponent"));
    PostProcessComponent->SetupAttachment(RootComponent);
    PostProcessComponent->bUnbound = true;
    PostProcessComponent->bEnabled = false;
    PostProcessComponent->Priority = -1;
    SetRootComponent(PostProcessComponent);

    LoadDefaultMaterial();
}

void ADatamoshControllerActor::BeginPlay()
{
    Super::BeginPlay();
    UpdateDatamoshParameters();
}

void ADatamoshControllerActor::UpdateDatamoshParameters()
{
    UDatamoshBlueprintLibrary::SetDatamoshMode(Mode);
    if (Mode != EDatamoshModeSetting::Off)
    {
        UDatamoshBlueprintLibrary::SetDatamoshParameters(Parameters);
    }
    if (Mode == EDatamoshModeSetting::ScreenRendering)
    {
        UDatamoshBlueprintLibrary::SetDatamoshActive(bEnabled);
    }

    UpdatePostProcess();
}

void ADatamoshControllerActor::ResetDatamoshParameters()
{
    Parameters = UDatamoshBlueprintLibrary::GetDatamoshParameters();
    Mode = EDatamoshModeSetting::Off;
    bEnabled = false;
    UDatamoshBlueprintLibrary::ResetToDefaultParameters();

    UpdatePostProcess();
}

void ADatamoshControllerActor::UpdatePostProcess()
{
    if (Mode == EDatamoshModeSetting::RenderTarget && DatamoshMaterial)
    {
        PostProcessComponent->bEnabled = true;
        PostProcessComponent->Settings.WeightedBlendables.Array.Empty();
        PostProcessComponent->Settings.AddBlendable(DatamoshMaterial, 1.0f);
    }
    else
    {
        PostProcessComponent->bEnabled = false;
    }
}

void ADatamoshControllerActor::LoadDefaultMaterial()
{
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/DatamoshEffect/Materials/PPM_Datamosh.PPM_Datamosh"));
    if (MaterialFinder.Succeeded())
    {
        DatamoshMaterial = MaterialFinder.Object;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PPM_Datamosh not found in /Datamosh/Materials/"));
    }
}
