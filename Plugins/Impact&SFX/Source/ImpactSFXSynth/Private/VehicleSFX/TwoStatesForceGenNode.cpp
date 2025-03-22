// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "Runtime/Launch/Resources/Version.h"
#include "ImpactSFXSynthLog.h"
#include "ImpactSynthEngineNodesName.h"
#include "DSP/Dsp.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "Extend/MetasoundRCurveExtend.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "VehicleSFX/TwoStatesForceGen.h"
#include "MetasoundEnumRegistrationMacro.h"

#define LOCTEXT_NAMESPACE "LBSImpactSFXSynthNodes_TwoStatesForceGenNodes"

namespace Metasound
{
	using namespace LBSImpactSFXSynth;
	
	DECLARE_METASOUND_ENUM(EForceNoiseMergeMode, EForceNoiseMergeMode::None, IMPACTSFXSYNTH_API,
						   FForceNoiseMergeMode, FForceNoiseMergeModeTypeInfo, FForceNoiseMergeModeReadRef, FForceNoiseMergeModeWriteRef);
	
	DEFINE_METASOUND_ENUM_BEGIN(EForceNoiseMergeMode, Metasound::FForceNoiseMergeMode, "ForceNoiseMergeMode")
		DEFINE_METASOUND_ENUM_ENTRY(EForceNoiseMergeMode::None, "NoneModeDescription", "None", "NoneDescriptionTT", "Do not merge the ouput with white noise."),
		DEFINE_METASOUND_ENUM_ENTRY(EForceNoiseMergeMode::Add, "AddModeDescription", "Add", "AddDescriptionTT", "Add the ouput with white noise."),
		DEFINE_METASOUND_ENUM_ENTRY(EForceNoiseMergeMode::Multiply, "MultiplyModeDescription", "Multiply", "MultiplyDescriptionTT", "Multiply the ouput with white noise."),
	DEFINE_METASOUND_ENUM_END()
}

namespace LBSImpactSFXSynth
{
	using namespace Metasound;
	
	namespace TwoStatesForceGenVertexNames
	{
		METASOUND_PARAM(InputTriggerPlay, "Play", "Start generating.")
		METASOUND_PARAM(InputTriggerStop, "Stop", "Stop generating.")
		METASOUND_PARAM(InputTriggerSync, "Sync", "Reset cycle.")

		METASOUND_PARAM(InputSeed, "Seed", "Radomizer seed. If <= -1, use random seed.")
		METASOUND_PARAM(InputAmp, "Amplitude", "The amplitude of the output force.")
		METASOUND_PARAM(InputFreq, "Freq", "Number of cycles per second.")
		METASOUND_PARAM(InputFirstStateDutyCycle, "Duty Cycle", "The duty cycle of the first state in the range [0, 1].")
		
		METASOUND_PARAM(InputFirstCurve, "First State Curve", "The time of the start and end keys of the curve must be 0 and 1, respectively.")
		METASOUND_PARAM(InputFirstFreqScale, "First Curve Sample Scale", "This scales the sample frequency of the curve.")
		METASOUND_PARAM(InputFirstStateNoiseMode, "First State Noise Mode", "If not none, a white noise is added or multipled with the output curve.")
		METASOUND_PARAM(InputFirstStateNoiseAmp, "First State Noise Amplitude", "The amplitude of the white noise.")

		METASOUND_PARAM(InputSecondCurve, "Second Curve", "The time of the start and end keys of the curve must be 0 and 1, respectively.")
		METASOUND_PARAM(InputSecondFreqScale, "Second Curve Sample Scale", "This scales the sample frequency of the curve.")
		METASOUND_PARAM(InputSecondStateNoiseMode, "Second State Noise Mode", "If not none, a white noise is added or multipled with the output curve.")
		METASOUND_PARAM(InputSecondStateNoiseAmp, "Second State Noise Amplitude", "The amplitude of the white noise.")
		
		METASOUND_PARAM(OutputTriggerOnPlay, "On Play", "Triggers when Play is triggered.")
		METASOUND_PARAM(OutputTriggerOnDone, "On Finished", "Triggers when the SFX energy decays to zero or reach the specified duration.")

		METASOUND_PARAM(OutputForce, "Out Force", "The output force.")
	}

	struct FTwoStatesForceGenOpArgs
	{
		FOperatorSettings Settings;
		
		FTriggerReadRef PlayTrigger;
		FTriggerReadRef StopTrigger;
		FTriggerReadRef SyncTrigger;
		
		int32 Seed;
		FFloatReadRef Amp;
		FFloatReadRef Freq;
		FFloatReadRef FirstStateDutyCycle;
		
		FRCurveExtendReadRef FirstCurve;
		FFloatReadRef FirstFreqScale;
		FForceNoiseMergeModeReadRef FirstStateNoiseMode;
		FFloatReadRef FirstStateNoiseAmp;
		
		FRCurveExtendReadRef SecondCurve;
		FFloatReadRef SecondFreqScale;
		FForceNoiseMergeModeReadRef SecondStateNoiseMode;
		FFloatReadRef SecondStateNoiseAmp;
	};
	
	class FTwoStatesForceGenOperator : public TExecutableOperator<FTwoStatesForceGenOperator>
	{
	public:

		FTwoStatesForceGenOperator(const FTwoStatesForceGenOpArgs& InArgs)
			: OperatorSettings(InArgs.Settings)
			, PlayTrigger(InArgs.PlayTrigger)
			, StopTrigger(InArgs.StopTrigger)
			, SyncTrigger(InArgs.SyncTrigger)
			, Seed(InArgs.Seed)
			, Amp(InArgs.Amp)
			, Freq(InArgs.Freq)
			, FirstStateDutyCycle(InArgs.FirstStateDutyCycle)
			, FirstCurve(InArgs.FirstCurve)
			, FirstFreqScale(InArgs.FirstFreqScale)
		    , FirstStateNoiseMode(InArgs.FirstStateNoiseMode)
			, FirstStateNoiseAmp(InArgs.FirstStateNoiseAmp)
			, SecondCurve(InArgs.SecondCurve)
			, SecondFreqScale(InArgs.SecondFreqScale)
			, SecondStateNoiseMode(InArgs.SecondStateNoiseMode)
			, SecondStateNoiseAmp(InArgs.SecondStateNoiseAmp)
			, TriggerOnDone(FTriggerWriteRef::CreateNew(InArgs.Settings))
			, ForceWriteBuffer(FAudioBufferWriteRef::CreateNew(InArgs.Settings))
		{
			SamplingRate = OperatorSettings.GetSampleRate();
			NumSamplesPerBlock = OperatorSettings.GetNumFramesPerBlock();
			OutputAudioView = TArrayView<float>(ForceWriteBuffer->GetData(), ForceWriteBuffer->Num());
		}
		
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace TwoStatesForceGenVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerPlay), PlayTrigger);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerStop), StopTrigger);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerSync), SyncTrigger);
			
			InOutVertexData.SetValue(METASOUND_GET_PARAM_NAME(InputSeed), Seed);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAmp), Amp);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFreq), Freq);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFirstStateDutyCycle), FirstStateDutyCycle);
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFirstCurve), FirstCurve);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFirstFreqScale), FirstFreqScale);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFirstStateNoiseMode), FirstStateNoiseMode);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFirstStateNoiseAmp), FirstStateNoiseAmp);

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSecondCurve), SecondCurve);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSecondFreqScale), SecondFreqScale);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSecondStateNoiseMode), SecondStateNoiseMode);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSecondStateNoiseAmp), SecondStateNoiseAmp);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace TwoStatesForceGenVertexNames;
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnPlay), PlayTrigger);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnDone), TriggerOnDone);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputForce), ForceWriteBuffer);
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
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::TwoStatesForceGenVertexNames::Execute);
			
			TriggerOnDone->AdvanceBlock();
			
			FMemory::Memzero(ForceWriteBuffer->GetData(), OperatorSettings.GetNumFramesPerBlock() * sizeof(float));
			
			ExecuteSubBlocks();
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			TriggerOnDone->Reset();
			
			ForceWriteBuffer->Zero();

			TwoStatesForceGen.Reset();
			
			bIsPlaying = false;
		}

	private:

		void ExecuteSubBlocks()
		{
			// Parse triggers and render audio
			int32 PlayTrigIndex = 0;
			int32 NextPlayFrame = 0;
			const int32 NumPlayTrigs = PlayTrigger->NumTriggeredInBlock();

			int32 StopTrigIndex = 0;
			int32 NextStopFrame = 0;
			const int32 NumStopTrigs = StopTrigger->NumTriggeredInBlock();

			int32 SyncTrigIndex = 0;
			int32 NextSyncFrame = 0;
			const int32 NumSyncTrigs = SyncTrigger->NumTriggeredInBlock();

			int32 CurrAudioFrame = 0;
			int32 NextAudioFrame = 0;
			const int32 LastAudioFrame = OperatorSettings.GetNumFramesPerBlock() - 1;
			const int32 NoTrigger = OperatorSettings.GetNumFramesPerBlock() << 1; //Same as multiply by 2

			while (NextAudioFrame < LastAudioFrame)
			{
				// get the next Play and Stop indices
				// (play)
				if (PlayTrigIndex < NumPlayTrigs)
				{
					NextPlayFrame = (*PlayTrigger)[PlayTrigIndex];
				}
				else
				{
					NextPlayFrame = NoTrigger;
				}

				// (stop)
				if (StopTrigIndex < NumStopTrigs)
				{
					NextStopFrame = (*StopTrigger)[StopTrigIndex];
				}
				else
				{
					NextStopFrame = NoTrigger;
				}

				// (sync)
				if (SyncTrigIndex < NumSyncTrigs)
				{
					NextSyncFrame = (*SyncTrigger)[SyncTrigIndex];
				}
				else
				{
					NextSyncFrame = NoTrigger;
				}
				
				// determine the next audio frame we are going to render up to
				NextAudioFrame = FMath::Min3(NextPlayFrame, NextStopFrame, NextSyncFrame);

				// no more triggers, rendering to the end of the block
				if (NextAudioFrame == NoTrigger)
				{
					NextAudioFrame = OperatorSettings.GetNumFramesPerBlock();
				}
				
				if (CurrAudioFrame != NextAudioFrame)
				{
					RenderFrameRange(CurrAudioFrame, NextAudioFrame);
					CurrAudioFrame = NextAudioFrame;
				}
				
				if (CurrAudioFrame == NextPlayFrame)
				{
					InitSynthesizers();
					++PlayTrigIndex;
				}

				if (CurrAudioFrame == NextStopFrame)
				{
					bIsPlaying = false;
					TriggerOnDone->TriggerFrame(CurrAudioFrame);
					++StopTrigIndex;
				}

				if(CurrAudioFrame == NextSyncFrame)
				{
					if(TwoStatesForceGen.IsValid())
						TwoStatesForceGen->ResetCycleIndex();
					SyncTrigIndex++;
				}
			}
		}

		void InitSynthesizers()
		{
			TwoStatesForceGen.Reset();
			TwoStatesForceGen = MakeUnique<FTwoStatesForceGen>(SamplingRate, NumSamplesPerBlock, Seed);
			bIsPlaying = TwoStatesForceGen.IsValid();
		}

		void RenderFrameRange(int32 StartFrame, int32 EndFrame)
		{
			if(!bIsPlaying)
				return;
			
			const int32 NumFramesToGenerate = EndFrame - StartFrame;
			if (NumFramesToGenerate <= 0)
			{
				UE_LOG(LogImpactSFXSynth, Warning, TEXT("TwoStatesForceGenNodes::RenderFrameRange: StartFrame = %d and EndFrame = %d are invalid!"), StartFrame, EndFrame);
				return;
			}
			
			if(TwoStatesForceGen.IsValid())
			{
				
				TArrayView<float> BufferToGenerate = OutputAudioView.Slice(StartFrame, NumFramesToGenerate);

				TwoStatesForceGen->Generate(BufferToGenerate, *Amp, *Freq, *FirstStateDutyCycle, 
										   FirstCurve->GetProxy(), *FirstFreqScale, *FirstStateNoiseMode, *FirstStateNoiseAmp,
										   SecondCurve->GetProxy(), *SecondFreqScale, *SecondStateNoiseMode, *SecondStateNoiseAmp);
			}
			else
			{
				TwoStatesForceGen.Reset();
				bIsPlaying = false;
				TriggerOnDone->TriggerFrame(EndFrame);
			}
		}
		
		const FOperatorSettings OperatorSettings;
		
		FTriggerReadRef PlayTrigger;
		FTriggerReadRef StopTrigger;
		FTriggerReadRef SyncTrigger;
		
		int32 Seed;
		FFloatReadRef Amp;
		FFloatReadRef Freq;
		FFloatReadRef FirstStateDutyCycle;
		
		FRCurveExtendReadRef FirstCurve;
		FFloatReadRef FirstFreqScale;
		FForceNoiseMergeModeReadRef FirstStateNoiseMode;
		FFloatReadRef FirstStateNoiseAmp;
		
		FRCurveExtendReadRef SecondCurve;
		FFloatReadRef SecondFreqScale;
		FForceNoiseMergeModeReadRef SecondStateNoiseMode;
		FFloatReadRef SecondStateNoiseAmp;
		
		FTriggerWriteRef TriggerOnDone;
		FAudioBufferWriteRef ForceWriteBuffer;
		TArrayView<float> OutputAudioView;
		
		TUniquePtr<FTwoStatesForceGen> TwoStatesForceGen;
				
		float SamplingRate;
		int32 NumSamplesPerBlock;
		bool bIsPlaying = false;
	};

	class FTwoStatesForceGenOperatorFactory : public IOperatorFactory
	{
	public:
		FTwoStatesForceGenOperatorFactory(const TArray<FOutputDataVertex>& InOutputAudioVertices)
		: OutputAudioVertices(InOutputAudioVertices)
		{
		}
		
		virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
		{
			using namespace Audio;
			using namespace TwoStatesForceGenVertexNames;
		
			const FInputVertexInterfaceData& Inputs = InParams.InputData;
			
			FTwoStatesForceGenOpArgs Args =
			{
				InParams.OperatorSettings,
				
				Inputs.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerPlay), InParams.OperatorSettings),
				Inputs.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerStop), InParams.OperatorSettings),
				Inputs.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerSync), InParams.OperatorSettings),
				
				Inputs.GetOrCreateDefaultValue<int32>(METASOUND_GET_PARAM_NAME(InputSeed), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputAmp), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFreq), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFirstStateDutyCycle), InParams.OperatorSettings),
				
				Inputs.GetOrCreateDefaultDataReadReference<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputFirstCurve), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFirstFreqScale), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<FForceNoiseMergeMode>(METASOUND_GET_PARAM_NAME(InputFirstStateNoiseMode), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFirstStateNoiseAmp), InParams.OperatorSettings),

				Inputs.GetOrCreateDefaultDataReadReference<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputSecondCurve), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSecondFreqScale), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<FForceNoiseMergeMode>(METASOUND_GET_PARAM_NAME(InputSecondStateNoiseMode), InParams.OperatorSettings),
				Inputs.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSecondStateNoiseAmp), InParams.OperatorSettings),
			};

			return MakeUnique<FTwoStatesForceGenOperator>(Args);
		}
	private:
		TArray<FOutputDataVertex> OutputAudioVertices;
	};

	
	template<typename AudioChannelConfigurationInfoType>
	class TTwoStatesForceGenNode : public FNode
	{

	public:
		static FVertexInterface DeclareVertexInterface()
		{
			using namespace TwoStatesForceGenVertexNames;
			
			FVertexInterface VertexInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerPlay)),
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerStop)),
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerSync)),
					
					TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSeed), -1),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmp), 1.f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFreq), 100.f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFirstStateDutyCycle), 0.5f),
					
					TInputDataVertex<FRCurveExtend>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFirstCurve)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFirstFreqScale), 1.f),
					TInputDataVertex<FForceNoiseMergeMode>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFirstStateNoiseMode)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFirstStateNoiseAmp), .5f),

					TInputDataVertex<FRCurveExtend>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSecondCurve)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSecondFreqScale), 1.f),
					TInputDataVertex<FForceNoiseMergeMode>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSecondStateNoiseMode)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSecondStateNoiseAmp), .5f)
					),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnPlay)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnDone)),
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputForce))
				)
			);
			
			return VertexInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Info;
				Info.ClassName = { ImpactSFXSynthEngineNodes::Namespace, TEXT("Two States Force Gen"), AudioChannelConfigurationInfoType::GetVariantName() };
				Info.MajorVersion = 1;
				Info.MinorVersion = 0;
				Info.DisplayName = AudioChannelConfigurationInfoType::GetNodeDisplayName();
				Info.Description = METASOUND_LOCTEXT("Metasound_TwoStatesForceGenNodeDescription", "Two States Force Generator.");
				Info.Author = TEXT("Le Binh Son");
				Info.PromptIfMissing = PluginNodeMissingPrompt;
				Info.DefaultInterface = DeclareVertexInterface();
				Info.Keywords = { METASOUND_LOCTEXT("TwoStatesForceGenKeyword", "Generator") };

				return Info;
			};

			static const FNodeClassMetadata Info = InitNodeInfo();

			return Info;
		}

		TTwoStatesForceGenNode(const FVertexName& InName, const FGuid& InInstanceID)
			:	FNode(InName, InInstanceID, GetNodeInfo())
			,	Factory(MakeOperatorFactoryRef<FTwoStatesForceGenOperatorFactory>(AudioChannelConfigurationInfoType::GetAudioOutputs()))
			,	Interface(DeclareVertexInterface())
		{
		}

		TTwoStatesForceGenNode(const FNodeInitData& InInitData)
			: TTwoStatesForceGenNode(InInitData.InstanceName, InInitData.InstanceID)
		{
		}

		virtual ~TTwoStatesForceGenNode() = default;

		virtual FOperatorFactorySharedRef GetDefaultOperatorFactory() const override
		{
			return Factory;
		}

		virtual const FVertexInterface& GetVertexInterface() const override
		{
			return Interface;
		}

		virtual bool SetVertexInterface(const FVertexInterface& InInterface) override
		{
			return InInterface == Interface;
		}

		virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const override
		{
			return InInterface == Interface;
		}

	private:
		FOperatorFactorySharedRef Factory;
		FVertexInterface Interface;
	};

	struct FTwoStatesForceGenMonoAudioChannelConfigurationInfo
	{
		static FText GetNodeDisplayName() { return METASOUND_LOCTEXT("Metasound_TwoStatesForceGenGenMonoNodeDisplayName", "Two States Force Gen (Mono)"); }
		static FName GetVariantName() { return ImpactSFXSynthEngineNodes::MonoVariant; }

		static TArray<FOutputDataVertex> GetAudioOutputs()
		{
			using namespace TwoStatesForceGenVertexNames;
			return { };
		}
	};
	using FMonoTwoStatesForceGenNode = TTwoStatesForceGenNode<FTwoStatesForceGenMonoAudioChannelConfigurationInfo>;
	METASOUND_REGISTER_NODE(FMonoTwoStatesForceGenNode);

}

#undef LOCTEXT_NAMESPACE