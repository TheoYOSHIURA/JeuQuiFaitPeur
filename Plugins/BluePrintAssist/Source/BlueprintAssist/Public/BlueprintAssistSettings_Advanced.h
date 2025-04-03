#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "UObject/Object.h"
#include "BlueprintAssistSettings_Advanced.generated.h"

#define BA_DEBUG_EARLY_EXIT(string) do { if (UBASettings_Advanced::HasDebugSetting(string)) return; } while(0)
#define BA_DEBUG(string) GetDefault<UBASettings_Advanced>()->BlueprintAssistDebug.Contains(string)

UENUM()
enum class EBACacheSaveLocation : uint8
{
	/** Save to PluginFolder/NodeSizeCache/PROJECT_ID.json */
	Plugin UMETA(DisplayName = "Plugin", Tooltip = "Save to PluginFolder/NodeSizeCache/PROJECT_ID.json"),

	/** Save to ProjectFolder/Saved/BlueprintAssist/BlueprintAssistCache.json */
	Project UMETA(DisplayName = "Project", Tooltip = "Save to ProjectFolder/Saved/BlueprintAssist/BlueprintAssistCache.json"),
};


UCLASS(config = EditorPerProjectUserSettings)
class BLUEPRINTASSIST_API UBASettings_Advanced final : public UBASettingsBase
{
	GENERATED_BODY()

public:
	UBASettings_Advanced(const FObjectInitializer& ObjectInitializer);

	////////////////////////////////////////////////////////////
	/// Cache
	////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, config, Category = "Cache")
	EBACacheSaveLocation CacheSaveLocation;

	/* Save the node size cache to a file (located in the the plugin folder) */
	UPROPERTY(EditAnywhere, config, Category = "Cache")
	bool bSaveBlueprintAssistCacheToFile;

	/* Enable slower but more accurate node size caching */
	UPROPERTY(EditAnywhere, config, Category = "Cache")
	bool bSlowButAccurateSizeCaching;

	/* If swapping produced any looping wires, remove them */
	UPROPERTY(EditAnywhere, config, Category = "Commands|Swap Nodes")
	bool bRemoveLoopingCausedBySwapping;

	UPROPERTY(EditAnywhere, config, Category = "Commands")
	TSet<FName> DisabledCommands;

	/* Potential issue where pins can get stuck in a hovered state on the material graph */
	UPROPERTY(EditAnywhere, config, Category = "Material Graph|Experimental")
	bool bEnableMaterialGraphPinHoverFix;

	/* Fix for issue where copy-pasting material nodes will result in their material expressions having the same GUID */
	UPROPERTY(EditAnywhere, config, Category = "Material Graph|Experimental", DisplayName="Generate Unique GUID For Material Expressions")
	bool bGenerateUniqueGUIDForMaterialExpressions;

	/* Instead of making a json file to store cache data, store it in the blueprint's package meta data */
	UPROPERTY(EditAnywhere, config, Category = "Cache|Experimental")
	bool bStoreCacheDataInPackageMetaData;

	/* Save cache file JSON in a more human-readable format. Useful for debugging, but increases size of cache files.  */
	UPROPERTY(EditAnywhere, config, Category = "Cache")
	bool bPrettyPrintCacheJSON;

	/* Use a custom blueprint action menu for creating nodes (very prototype, not supported in 5.0 or earlier) */
	UPROPERTY(EditAnywhere, config, Category = "Misc|Experimental")
	bool bUseCustomBlueprintActionMenu;

	/* Hacky workaround to ensure that default comment nodes will be correctly resized after formatting */
	UPROPERTY(EditAnywhere, config, Category = "Misc|Experimental")
	bool bForceRefreshGraphAfterFormatting;

	/* Disable the plugin (requires restarting engine) */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta=(ConfigRestartRequired = "true"))
	bool bDisableBlueprintAssistPlugin;

	/** Ignore this (setting for custom debugging) */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = "Misc")
	TArray<FString> BlueprintAssistDebug;

	/** Draw a red border around bad comment nodes after formatting */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	bool bHighlightBadComments;

	static FORCEINLINE bool HasDebugSetting(const FString& Setting)
	{
		return Get().BlueprintAssistDebug.Contains(Setting);
	}

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	FORCEINLINE static const UBASettings_Advanced& Get() { return *GetDefault<UBASettings_Advanced>(); }
	FORCEINLINE static UBASettings_Advanced& GetMutable() { return *GetMutableDefault<UBASettings_Advanced>(); }
};

class FBASettingsDetails_Advanced final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
