// Copyright 2023-2024, Le Binh Son, All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVirtualInstrumentEditor, Log, All);

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		class FVirtualInstrumentEditorConst 
		{
		public:
			static const FName VirtualInstrumentCategory;
			static const FText VirtualInstrumentSubCategory;
		};
	}
}
