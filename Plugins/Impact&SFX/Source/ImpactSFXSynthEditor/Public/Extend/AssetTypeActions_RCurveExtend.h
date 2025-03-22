// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class URCurveExtend;

namespace LBSImpactSFXSynth
{
	namespace Editor
	{
		class IMPACTSFXSYNTHEDITOR_API FAssetTypeActions_RCurveExtend : public FAssetTypeActions_Base
		{
		public:
			virtual UClass* GetSupportedClass() const override;
			virtual FText GetName() const override{ return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_RCurveExtend", "Rich Curve Extend"); }
			virtual FColor GetTypeColor() const override;
			virtual uint32 GetCategories() override;
			
			virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> ToolkitHost) override;
		};

		class IMPACTSFXSYNTHEDITOR_API FRCurveExtendExtension
		{
		public:
			static void RegisterMenus();
			static void ExecuteCreateResidualDataFromResidualObj(const struct FToolMenuContext& MenuContext);
		};
		
	}
}