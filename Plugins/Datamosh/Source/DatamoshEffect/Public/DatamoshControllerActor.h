// Copyright 2025 Andrew Laptev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PostProcessComponent.h"
#include "DatamoshBlueprintLibrary.h"
#include "DatamoshControllerActor.generated.h"

UCLASS()
class DATAMOSHEFFECT_API ADatamoshControllerActor : public AActor
{
    GENERATED_BODY()

public:
    ADatamoshControllerActor();

protected:
    virtual void BeginPlay() override;

public:
    // Current Datamosh mode.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    TEnumAsByte<EDatamoshModeSetting::Type> Mode;

    // Screen Rendering mode only.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode == EDatamoshModeSetting::ScreenRendering"))
    bool bEnabled;

    // Current Datamosh shader parameters. You can also use these parameters instead of modifying the parameters in the PPM_Datamosh material.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh", meta = (EditConditionHides, EditCondition = "Mode != EDatamoshModeSetting::Off"))
    FDatamoshParameters Parameters;

    UPROPERTY(VisibleAnywhere, Category = "Datamosh")
    UPostProcessComponent* PostProcessComponent;

    // The effect material used in Render Target mode.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
    UMaterialInterface* DatamoshMaterial;

    // Event updating the current effect parameters.
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Datamosh")
    void UpdateDatamoshParameters();
    
    // Reset effect parameters to default settings.
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Datamosh")
    void ResetDatamoshParameters();

    void UpdatePostProcess();
    void LoadDefaultMaterial();
};