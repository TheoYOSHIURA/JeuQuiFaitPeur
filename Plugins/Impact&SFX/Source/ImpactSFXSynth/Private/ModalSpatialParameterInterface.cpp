// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "ModalSpatialParameterInterface.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "ModalSpatialParameterInterface"

#define AUDIO_PARAMETER_INTERFACE_NAMESPACE "ModalSpatial"

namespace ModalSpatialParameterInterface
{
    const FName Name = AUDIO_PARAMETER_INTERFACE_NAMESPACE;

    namespace Inputs
    {
        const FName EnableHRTF = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("EnableHRTF");
        const FName RoomSize = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("RoomSize");
        const FName Absorption = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("Absorption");
        const FName OpenRoomFactor = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("OpenRoomFactor");
        const FName EnclosedFactor = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("EnclosedFactor");
    } 

    Audio::FParameterInterfacePtr GetInterface()
    {
        struct FInterface : public Audio::FParameterInterface
        {
            FInterface() : FParameterInterface(ModalSpatialParameterInterface::Name, {1, 0})
            {
                Inputs = {
                    {FText(),
                     LOCTEXT(
                         "EnableHRTFDescription",
                         "Enable HRTF or not."),
                     FName(),
                     {Inputs::EnableHRTF, 1.0f}},
                    
                    {FText(),
                     LOCTEXT(
                         "RoomSizeDescription",
                         "The size of the current room in Unreal unit."),
                     FName(),
                     {Inputs::RoomSize, 100.0f}},

                    {FText(),
                     LOCTEXT(
                         "AbsorptionDescription",
                         "The audio absorption quality of the current room."),
                     FName(),
                     {Inputs::Absorption, 2.0f}},
   
                    {FText(),
                     LOCTEXT(
                         "OpenRoomFactorescription",
                         "An enclosed room is assigned a default value of 1, while outdoor environments are given a value of 10. This parameter should only be used for the reverb node."),
                     FName(),
                     {Inputs::OpenRoomFactor, 1.0f}},

                    {FText(),
                     LOCTEXT(
                         "EnclosedFactorDescription",
                         "Enclosed Factor. A completely enclosed room will have a value of 1. And an empty outdoor environments will have a value of 0."),
                     FName(),
                     {Inputs::EnclosedFactor, 0.0f}},
                };
            }
        };

        static Audio::FParameterInterfacePtr InterfacePtr;
        if (!InterfacePtr.IsValid())
        {
            InterfacePtr = MakeShared<FInterface>();
        }

        return InterfacePtr;
    }
    void RegisterInterface()
    {
        Audio::IAudioParameterInterfaceRegistry& InterfaceRegistry = Audio::IAudioParameterInterfaceRegistry::Get();
        InterfaceRegistry.RegisterInterface(GetInterface());
    }
}

#undef AUDIO_PARAMETER_INTERFACE_NAMESPACE
#undef LOCTEXT_NAMESPACE