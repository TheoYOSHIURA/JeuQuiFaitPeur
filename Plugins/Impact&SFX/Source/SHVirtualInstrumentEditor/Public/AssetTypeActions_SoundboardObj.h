// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class USoundboardObj;

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		class SHVIRTUALINSTRUMENTEDITOR_API FAssetTypeActions_SoundboardObj : public FAssetTypeActions_Base
		{
		public:
			virtual UClass* GetSupportedClass() const override;
			virtual FText GetName() const override{ return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundboardObj", "Soundboard Obj"); }
			virtual FColor GetTypeColor() const override;
			
			virtual uint32 GetCategories() override;
			
			virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
			virtual bool IsImportedAsset() const override { return true; }
			
		};
	}
}