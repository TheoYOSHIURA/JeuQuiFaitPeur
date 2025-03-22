// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UPianoKeyObj;

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		class FAssetTypeActions_PianoKeyObj : public FAssetTypeActions_Base
		{
		public:
			virtual UClass* GetSupportedClass() const override;
			virtual FText GetName() const override{ return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PianoKeyObj", "Piano Key Obj"); }
			virtual FColor GetTypeColor() const override;
			
			virtual uint32 GetCategories() override;
			virtual const TArray<FText>& GetSubMenus() const override;
			
			virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
			virtual bool IsImportedAsset() const override { return true; }
			
		};
	}
}