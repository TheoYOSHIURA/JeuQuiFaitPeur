// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

using UnrealBuildTool;

public class SHVirtualInstrument : ModuleRules
{
    public SHVirtualInstrument(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "MetasoundFrontend",
                "MetasoundStandardNodes",
                "DeveloperSettings",
                "AudioPlatformConfiguration",
                "WaveTable",
                "SignalProcessing",
                "ImpactSFXSynth"
            }
        );
			
		
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "AudioExtensions",
                "AudioMixer",
                "MetasoundEngine",
                "MetasoundGenerator",
                "MetasoundGraphCore",
                "HarmonixMetasound",
                "HarmonixMidi"
                // ... add private dependencies that you statically link with here ...	
            }
        );
        
    }
}