// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "Extend/RCurveExtend.h"

namespace Metasound
{
	// Forward declare ReadRef
	class FRCurveExtend;
	typedef TDataReadReference<FRCurveExtend> FRCurveExtendReadRef;
	
	// Metasound data type that holds onto a weak ptr. Mostly used as a placeholder until we have a proper proxy type.
	class IMPACTSFXSYNTH_API FRCurveExtend
	{
		FRCurveExtendAssetProxyPtr Proxy;
		
	public:
	
		FRCurveExtend()
			: Proxy(nullptr)
		{
		}
	
		FRCurveExtend(const FRCurveExtend& InOther)
			: Proxy(InOther.Proxy)
		{
		}
	
		FRCurveExtend& operator=(const FRCurveExtend& InOther)
		{
			Proxy = InOther.Proxy;
			return *this;
		}
		
		FRCurveExtend& operator=(FRCurveExtend&& InOther)
		{
			Proxy = MoveTemp(InOther.Proxy);
			return *this;
		}
	
		FRCurveExtend(const TSharedPtr<Audio::IProxyData>& InInitData)
		{
			if (InInitData.IsValid())
			{
				if (InInitData->CheckTypeCast<FRCurveExtendAssetProxy>())
				{
					Proxy = MakeShared<FRCurveExtendAssetProxy, ESPMode::ThreadSafe>(InInitData->GetAs<FRCurveExtendAssetProxy>());
				}
			}
		}
	
		bool IsValid() const
		{
			return Proxy.IsValid();
		}
	
		const FRCurveExtendAssetProxyPtr& GetProxy() const
		{
			return Proxy;
		}
	
		const FRCurveExtendAssetProxy* operator->() const
		{
			return Proxy.Get();
		}
	
		FRCurveExtendAssetProxy* operator->()
		{
			return Proxy.Get();
		}
	};
	
	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FRCurveExtend, IMPACTSFXSYNTH_API, FRCurveExtendTypeInfo, FRCurveExtendReadRef, FRCurveExtendWriteRef)
}
