// Copyright 2023-2024, Le Binh Son, All rights reserved.

#include "SHVirtualInstrumentEditorLog.h"

DEFINE_LOG_CATEGORY(LogVirtualInstrumentEditor);

namespace LBSVirtualInstrument
{
	namespace Editor
	{
		const FName FVirtualInstrumentEditorConst::VirtualInstrumentCategory = FName(TEXT("ImpactSFXSynthCat"));
		const FText FVirtualInstrumentEditorConst::VirtualInstrumentSubCategory = INVTEXT("VirtualInstrument");
	}
}