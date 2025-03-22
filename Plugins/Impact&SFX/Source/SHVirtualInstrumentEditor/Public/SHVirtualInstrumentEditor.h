// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_SoundboardObj.h"
#include "Piano/AssetTypeActions_PianoKeyObj.h"
#include "Piano/AssetTypeActions_PianoModel.h"
#include "Modules/ModuleManager.h"

namespace LBSVirtualInstrument
{
    namespace Editor
    {
        class FSHVirtualInstrumentEditorModule : public IModuleInterface
        {
        public:
           
            virtual void StartupModule() override;
            virtual void ShutdownModule() override;
            
        private:
            TSharedPtr<FAssetTypeActions_SoundboardObj> SoundboardObjTypeActions;
            TSharedPtr<FAssetTypeActions_PianoKeyObj> PianoKeyObjTypeActions;
            TSharedPtr<FAssetTypeActions_PianoModel> PianoModelTypeActions;
            
            void RegisterMenus();
        };
    }
}