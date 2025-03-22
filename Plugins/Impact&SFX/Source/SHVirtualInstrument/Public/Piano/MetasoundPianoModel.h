// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "PianoModel.h"

namespace Metasound
{
	// Forward declare ReadRef
	class FPianoModel;
	typedef TDataReadReference<FPianoModel> FPianoModelDataReadRef;
	
	// Metasound data type that holds onto a weak ptr. Mostly used as a placeholder until we have a proper proxy type.
	class SHVIRTUALINSTRUMENT_API FPianoModel
	{
		FPianoModelAssetProxyPtr Proxy;
		
	public:
	
		FPianoModel()
			: Proxy(nullptr)
		{
		}
	
		FPianoModel(const FPianoModel& InOther)
			: Proxy(InOther.Proxy)
		{
		}
	
		FPianoModel& operator=(const FPianoModel& InOther)
		{
			Proxy = InOther.Proxy;
			return *this;
		}
		
		FPianoModel& operator=(FPianoModel&& InOther)
		{
			Proxy = MoveTemp(InOther.Proxy);
			return *this;
		}
	
		FPianoModel(const TSharedPtr<Audio::IProxyData>& InInitData)
		{
			if (InInitData.IsValid())
			{
				if (InInitData->CheckTypeCast<FPianoModelAssetProxy>())
				{
					Proxy = MakeShared<FPianoModelAssetProxy, ESPMode::ThreadSafe>(InInitData->GetAs<FPianoModelAssetProxy>());
				}
			}
		}
	
		bool IsValid() const
		{
			return Proxy.IsValid();
		}
	
		const FPianoModelAssetProxyPtr& GetProxy() const
		{
			return Proxy;
		}
	
		const FPianoModelAssetProxy* operator->() const
		{
			return Proxy.Get();
		}
	
		FPianoModelAssetProxy* operator->()
		{
			return Proxy.Get();
		}
	};
	
	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FPianoModel, SHVIRTUALINSTRUMENT_API, FPianoModelTypeInfo, FPianoModelReadRef, FPianoModelWriteRef)
}
