// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "ImpactSynthEngineNodesName.h"
#include "Runtime/Launch/Resources/Version.h"
#include "DSP/Dsp.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"

#define LOCTEXT_NAMESPACE "LBSImpactSFXSynthNodes_VelocityNode"

namespace LBSImpactSFXSynth
{
	using namespace Metasound;
	
	namespace VelocityVertexNames
	{
		METASOUND_PARAM(NFrameFilter, "Num Avg Frames", "The number of frame to average out the velocity.")
		METASOUND_PARAM(StandstillDuration, "Standstill Duration", "Velocity is reduced to zero only if the input value isn't changed during this duration. This is done because the block rate of MetaSounds (default to 10ms) can be faster than the FPS on the gamethread.")
		METASOUND_PARAM(InputValue, "In Value", "The input value.")
		
		METASOUND_PARAM(OutAvgVelocity, "Avg Velocity", "The average velocities over N frames. Return a positive value if the input is increasing. Otherwise, negative or zero.");
		METASOUND_PARAM(OutInstantVelocity, "Current Velocity", "The current instantaneous velocity. Return a positive value if the input is increasing. Otherwise, negative or zero.");
		METASOUND_PARAM(OutChangedSign, "Sign Changed", "True if the sign of the previous velocity != the current velocity.");
	}

	struct FVelocityOpArgs
	{
		FOperatorSettings OperatorSettings;

		int32 NFrameAvg;
		FTimeReadRef StandstillDuration;
		FFloatReadRef InValue;
	};
	
	class FVelocityOperator : public TExecutableOperator<FVelocityOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);
		
		FVelocityOperator(const FVelocityOpArgs& InArgs)
			: OperatorSettings(InArgs.OperatorSettings)
			, NFrameAvg(InArgs.NFrameAvg)
			, StandstillDuration(InArgs.StandstillDuration)
			, InValue(InArgs.InValue)
			, OutAvgVelocity(FFloatWriteRef::CreateNew(0.f))
			, OutCurrentVelocity(FFloatWriteRef::CreateNew(0.f))
			, OutIsChangeSign(FBoolWriteRef::CreateNew(false))
		{
			InitValues();
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			InitValues();
		}
		
		void InitValues()
		{
			const float SamplingRate = OperatorSettings.GetSampleRate();
			const int32 NumFramesPerBlock = OperatorSettings.GetNumFramesPerBlock();
			FrameTimeStep = FMath::Max(1e-5f, NumFramesPerBlock / SamplingRate);			
			PrevValue = *InValue;
			NumFrames = 0;
			*OutAvgVelocity = 0.f;
			*OutCurrentVelocity = 0.f;
			*OutIsChangeSign = false;
			
			Velocities.SetNumUninitialized(NFrameAvg);
			FMemory::Memzero(Velocities.GetData(), sizeof(float) * NFrameAvg);
			CurrentIdx = 0;
		}
		
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace VelocityVertexNames;
			
			InOutVertexData.SetValue(METASOUND_GET_PARAM_NAME(NFrameFilter), NFrameAvg);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(StandstillDuration), StandstillDuration);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue), InValue);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace VelocityVertexNames;
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutAvgVelocity), OutAvgVelocity);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutInstantVelocity), OutCurrentVelocity);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutChangedSign), OutIsChangeSign);
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed.
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed.
			checkNoEntry();
			return {};
		}

		void Execute()
		{
			const float CurrentValue = *InValue;
			if(NumFrames == 0)
			{ // Can only be zero if just init or reset
				PrevValue = CurrentValue;
				NumFrames = 1;
				return;
			}
			
			const float Duration = NumFrames * FrameTimeStep;
			if(CurrentValue == PrevValue)
			{
				*OutIsChangeSign = false;
				
				if(Duration >= *StandstillDuration)
				{
					*OutAvgVelocity = 0.f;
					*OutCurrentVelocity = 0.f;
					
					FMemory::Memzero(Velocities.GetData(), sizeof(float) * Velocities.Num());
					CurrentIdx = 0;
					PrevValue = CurrentValue;
					NumFrames = 1;
					return;
				}
				
				NumFrames++;
				return;
			}
			
			const float NewVelocity = (CurrentValue - PrevValue) / Duration;
			
			*OutAvgVelocity = (*OutAvgVelocity * Velocities.Num() - Velocities[CurrentIdx] + NewVelocity) / Velocities.Num();
			*OutIsChangeSign = FMath::Sign(*OutCurrentVelocity * NewVelocity) < 0; 
			*OutCurrentVelocity = NewVelocity; 
			Velocities[CurrentIdx] = NewVelocity;
			CurrentIdx = (CurrentIdx + 1) % Velocities.Num();
			PrevValue = CurrentValue;
			NumFrames = 1;
		}

	private:
		FOperatorSettings OperatorSettings;
		
		int32 NFrameAvg;
		FTimeReadRef StandstillDuration;
		FFloatReadRef InValue;
		
		FFloatWriteRef OutAvgVelocity;
		FFloatWriteRef OutCurrentVelocity;
		FBoolWriteRef OutIsChangeSign;

		float FrameTimeStep;
		float PrevValue;
		int32 NumFrames;
		TArray<float> Velocities;
		int32 CurrentIdx;
	};

	TUniquePtr<IOperator> FVelocityOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace VelocityVertexNames;
		
		const FInputVertexInterfaceData& Inputs = InParams.InputData;
		FVelocityOpArgs Args =
		{
			InParams.OperatorSettings,
			Inputs.GetOrCreateDefaultValue<int32>(METASOUND_GET_PARAM_NAME(NFrameFilter), InParams.OperatorSettings),
			Inputs.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(StandstillDuration), InParams.OperatorSettings),
			Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputValue), InParams.OperatorSettings),
		};

		return MakeUnique<FVelocityOperator>(Args);
	}
	
	const FNodeClassMetadata& FVelocityOperator::GetNodeInfo()
	{
		auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName{ImpactSFXSynthEngineNodes::Namespace, TEXT("Velocity"), "Velocity"},
				1, // Major Version
				0, // Minor Version
				METASOUND_LOCTEXT("VelocityDisplayName", "Velocity"),
				METASOUND_LOCTEXT("VelocityDesc", "Find the changing speed of the input value."),
				PluginAuthor,
				PluginNodeMissingPrompt,
				GetVertexInterface(),
				{NodeCategories::Math},
				{},
				FNodeDisplayStyle{}
			};

			return Metadata;
		};

		static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
		return Metadata;
	}

	const FVertexInterface& FVelocityOperator::GetVertexInterface()
	{
		using namespace VelocityVertexNames;
			
		static const FVertexInterface VertexInterface(
			FInputVertexInterface(
			    TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(NFrameFilter), 5),
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(StandstillDuration), 0.1f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue), 0.f)
				),
			FOutputVertexInterface(
				TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutAvgVelocity)),
				TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutInstantVelocity)),
				TOutputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutChangedSign))
			)
		);
			
		return VertexInterface;
	}

	// Node Class
	class FVelocityNode : public FNodeFacade
	{
	public:
		FVelocityNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FVelocityOperator>())
		{
		}
	};

	// Register node
	METASOUND_REGISTER_NODE(FVelocityNode)
}

#undef LOCTEXT_NAMESPACE
