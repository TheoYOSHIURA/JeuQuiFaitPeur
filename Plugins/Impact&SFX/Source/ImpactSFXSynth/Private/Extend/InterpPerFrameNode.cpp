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

#define LOCTEXT_NAMESPACE "LBSImpactSFXSynthNodes_InterpPerFrameNode"

namespace LBSImpactSFXSynth
{
	using namespace Metasound;
	
	namespace InterpPerFrameVertexNames
	{
		METASOUND_PARAM(InitValue, "Init Value", "The initial value.")
		METASOUND_PARAM(TargetValue, "Target Value", "The value which is changed per block rate.")
		METASOUND_PARAM(InputIncrement, "Increment Step", "The increment step per time frame.")
		METASOUND_PARAM(InputDecrement, "Decrement Step", "The decrement step per time frame.")
		METASOUND_PARAM(InputTimeStep, "Frame Time", "The time of each frame.")
		
		METASOUND_PARAM(OutInterpPerFrame, "Out Value", "The output value.");
	}

	struct FInterpPerFrameOpArgs
	{
		FOperatorSettings OperatorSettings;

		FFloatReadRef InitValue;
		FFloatReadRef InValue;
		FFloatReadRef Increment;
		FFloatReadRef Decrement;
		FTimeReadRef FrameTime;
	};
	
	class FInterpPerFrameOperator : public TExecutableOperator<FInterpPerFrameOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);
		
		FInterpPerFrameOperator(const FInterpPerFrameOpArgs& InArgs)
			: OperatorSettings(InArgs.OperatorSettings)
			, InitValue(InArgs.InitValue)
			, InTargetValue(InArgs.InValue)
			, Increment(InArgs.Increment)
			, Decrement(InArgs.Decrement)
			, FrameTime(InArgs.FrameTime)
			, OutInterpPerFrame(FFloatWriteRef::CreateNew(0.f))
		{
			const float SamplingRate = OperatorSettings.GetSampleRate();
			const int32 NumFramesPerBlock = OperatorSettings.GetNumFramesPerBlock();
			FrameTimePerBlock = NumFramesPerBlock / SamplingRate;
			*OutInterpPerFrame = *InitValue;
		}
		
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace InterpPerFrameVertexNames;
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InitValue), InitValue);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TargetValue), InTargetValue);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputIncrement), Increment);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDecrement), Decrement);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTimeStep), FrameTime);
			
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace InterpPerFrameVertexNames;
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutInterpPerFrame), OutInterpPerFrame);
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			*OutInterpPerFrame = 0.f;
			const float SampleRate = InParams.OperatorSettings.GetSampleRate();
			const int32 NumFramePerBlock = InParams.OperatorSettings.GetNumFramesPerBlock();
			FrameTimePerBlock = NumFramePerBlock / SampleRate;
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
			const float StepTime = FrameTimePerBlock / FMath::Max(1e-5f, FrameTime->GetSeconds());
			const float MaxStepValue =  (*Increment) * StepTime;
			const float MinStepValue = -(*Decrement) * StepTime;
			
			const float TargetValue = *InTargetValue;
			const float CurrentValue = *OutInterpPerFrame;
			const float TrueStep = FMath::Clamp(TargetValue - CurrentValue, MinStepValue, MaxStepValue);
			
			*OutInterpPerFrame = CurrentValue + TrueStep;
		}

	private:
		FOperatorSettings OperatorSettings;

		FFloatReadRef InitValue;
		FFloatReadRef InTargetValue;
		FFloatReadRef Increment;
		FFloatReadRef Decrement;
		FTimeReadRef FrameTime;
		
		FFloatWriteRef OutInterpPerFrame;

		float FrameTimePerBlock;
	};

	TUniquePtr<IOperator> FInterpPerFrameOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace InterpPerFrameVertexNames;
		
		const FInputVertexInterfaceData& Inputs = InParams.InputData;
		FInterpPerFrameOpArgs Args =
		{
			InParams.OperatorSettings,
			
			Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InitValue), InParams.OperatorSettings),
			Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(TargetValue), InParams.OperatorSettings),
			Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputIncrement), InParams.OperatorSettings),
			Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDecrement), InParams.OperatorSettings),
			Inputs.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InputTimeStep), InParams.OperatorSettings),
		};

		return MakeUnique<FInterpPerFrameOperator>(Args);
	}
	
	const FNodeClassMetadata& FInterpPerFrameOperator::GetNodeInfo()
	{
		auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName{ImpactSFXSynthEngineNodes::Namespace, TEXT("InterpPerFrame"), "InterpPerFrame"},
				1, // Major Version
				0, // Minor Version
				METASOUND_LOCTEXT("InterpPerFrameDisplayName", "InterpPerFrame"),
				METASOUND_LOCTEXT("InterpPerFrameDesc", "Interpolate to the target value over time."),
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

	const FVertexInterface& FInterpPerFrameOperator::GetVertexInterface()
	{
		using namespace InterpPerFrameVertexNames;
			
		static const FVertexInterface VertexInterface(
			FInputVertexInterface(
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InitValue), 0.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(TargetValue), 0.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputIncrement), 1.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDecrement), 1.f),
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTimeStep), 0.1f)
				),
			FOutputVertexInterface(
				TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutInterpPerFrame))
			)
		);
			
		return VertexInterface;
	}

	// Node Class
	class FInterpPerFrameNode : public FNodeFacade
	{
	public:
		FInterpPerFrameNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FInterpPerFrameOperator>())
		{
		}
	};

	// Register node
	METASOUND_REGISTER_NODE(FInterpPerFrameNode)
}

#undef LOCTEXT_NAMESPACE
