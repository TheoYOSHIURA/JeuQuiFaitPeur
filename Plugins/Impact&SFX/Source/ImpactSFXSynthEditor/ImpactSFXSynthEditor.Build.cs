// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

using UnrealBuildTool;

public class ImpactSFXSynthEditor : ModuleRules
{
    public ImpactSFXSynthEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "ImpactSFXSynth",
            "SignalProcessing",
            "ContentBrowser"
        });
        
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AudioEditor",
                "AudioExtensions",
                "AudioMixer",
                "Core",
                "CoreUObject",
                "CurveEditor",
                "Engine",
                "EditorFramework",
                "EditorWidgets",
                "GameProjectGeneration",
                "PropertyEditor",
                "SequenceRecorder",
                "Slate",
                "SlateCore",
                "ToolWidgets",
                "ToolMenus",
                "Json", 
                "JsonUtilities", 
                "EditorScriptingUtilities", 
                "MetasoundEditor"
            }
        );
        
        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetTools",
                "AssetRegistry"
            });

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetTools",
                "AssetRegistry"
            });

        PublicDefinitions.AddRange(
            new string[] {
                "BUILDING_STATIC"
            }
        );
    }
}