// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UPianoModel;

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		class FAssetTypeActions_PianoModel : public FAssetTypeActions_Base
		{
		public:
			virtual UClass* GetSupportedClass() const override;
			virtual FText GetName() const override{ return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PianoModel", "Piano Model"); }
			virtual FColor GetTypeColor() const override;
			
			virtual uint32 GetCategories() override;
			virtual const TArray<FText>& GetSubMenus() const override;
			
			virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> ToolkitHost) override;
		};
	}
}