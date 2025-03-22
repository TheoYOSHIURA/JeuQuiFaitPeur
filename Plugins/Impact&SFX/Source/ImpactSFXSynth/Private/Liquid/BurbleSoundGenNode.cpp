// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "Liquid/BurbleSoundGen.h"
#include "ImpactSFXSynthLog.h"
#include "Runtime/Launch/Resources/Version.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "ImpactSynthEngineNodesName.h"
#include "Extend\MetasoundRCurveExtend.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundTrace.h"

#define LOCTEXT_NAMESPACE "LBSImpactSFXSynthNodes_BurbleGenNode"

namespace LBSImpactSFXSynth
{
	using namespace Metasound;
	
	namespace BurbleGenVertexNames
	{
		METASOUND_PARAM(InputTriggerPlay, "Play", "Start generating.")
		
		METASOUND_PARAM(InputIsAutoStop, "Is Auto Stop", "If true, this node will be stopped when the spawn rate and the output have decayed to zero.")
		METASOUND_PARAM(InputSeed, "Seed", "If < 0, use a random seed.")
		METASOUND_PARAM(InputMaxNumBurbles, "Max Num Burbles", "The total maximum number of burbles that can be simulated simultaneously. If <= 0, inf number of burbles can be spawned for wave file writing purporses. For runtime usages, please use a positive value to avoid high CPU load.")
		METASOUND_PARAM(InputSpawnRate, "Spawn Rate", "The number of burbles that can be spawned per second.")
		METASOUND_PARAM(InputSpawnChance, "Spawn Chance", "The chance a new burble can be spawned.")
		METASOUND_PARAM(InputRadiusDist, "Radius Distribution", "Range [0.3, 2]. The radius of a new burble is chosen by sampling a power law disribution. Smaller values = smaller ranges but also smaller burbles. Use ~0.526 for rain. For breaking ocean waves, simulate the values between [0.435, 2].")
		METASOUND_PARAM(InputRadiusMin, "Radius Min", "The minimum possible radius of a burble in millimeters. For liquid simulation, it should be between [0.15, 5]. The full range is [0.15, 150].")
		METASOUND_PARAM(InputRadiusMax, "Radius Max", "The maximum possible radius of a burble in millimeters. For liquid simulation, it should be between [50, 150]. The full range is [0.15, 150].")
		METASOUND_PARAM(InputRadiusOffset, "Radius Offset", "This value is added to the radius of new burbles. Range [0, 100]. Keep this at 0 or extremely small (<0.1) for liquid simulations. This input is mostly used for some non-liquid effects.")
		METASOUND_PARAM(InputPitchShift, "Pitch Shift", "The pitch of each bubble is inversely related to its radius. This allow you to change their pitch in semitones without affecting the radius.")
		METASOUND_PARAM(InputAmpDist, "Amp Distribution", "The full value range is [0.1, 1.5]. But a reasonable value should be around [0.5, 1.]. The amplitude of a new burble is randomly sampled from a power distribution and multiplied with its radius. Lower values = smaller amplitude random ranges = easier to control but less realistic.")
		METASOUND_PARAM(InputRadiusAmpFactor, "Radius To Amp Ratio", "The radius of a burble will affect its amplitude. If 1, the sampled amplitude is multiplied with the radius directly. This is more reaslitic but can have a very large dynamic range. If 0, the amplitude of a burble won't depend on its radius.")
		METASOUND_PARAM(InputAmpOffset, "Amp Offset", "This value is added to the amplitude of new burbles. It can be used some special effects. The full value range is [0, 1]. But it should be kept around [0, 0.1]. This affects small burbles more than large ones. If you just want to increase the loudness, please use the Gain input below.")
		METASOUND_PARAM(InputGain, "Gain", "The scale applied to the final amplitude of new burbles. Increase this if you decrease the radius distribution value. And vice versa. Don't be afraid to use extreme values like 0.01 or 1000, to make sure the values of the output audio don't exceed [-1, 1] and hearable.")
		METASOUND_PARAM(InputGainMax, "Gain Max", "Range [0, 1]. This clamp the final gain of new burbles.")
		METASOUND_PARAM(InputDecayToChirpRatio, "Chirp Ratio", "Range [-1, 1]. This is the ratio applied to the chirp rate of new burbles. 0.1 should give the most realistic result for fresh water. But you can also randomize it between [0.05. 0.1] to have more varations.")
		METASOUND_PARAM(InputDecayScale, "Decay Scale", "> 0.1. Scale the decay rate of all burbles. WARNING: values lower than 1 at high spawn rate can have very high loudness.")
		METASOUND_PARAM(InputDecayThresh, "Decay Scale Threshold", "> 1. Only apply the decay scale above to a burble if its decay rate is lower than this value.")
		METASOUND_PARAM(InputFreqAmpCurve, "Freq Amp Curve", "This curve allows you to control the amplitude of a burble based on its frequency. X axis = frequency. Y axis = amplitude scale.")
		
		METASOUND_PARAM(OutputTriggerOnDone, "On Finished", "Triggers when all channels decays to zero and IsInAudioStop is true.")
		METASOUND_PARAM(OutputAudio, "Out Mono", "Output audio.")
	}

	class FBurbleGenOperator : public TExecutableOperator<FBurbleGenOperator>
	{
	public:

		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FBurbleGenOperator(const FOperatorSettings& InSettings,
							const FTriggerReadRef& InTriggerPlay,
							const FBoolReadRef& InIsAutoStop,
							int32 InSeed,
							int32 InMaxNumBurbles,
							const FFloatReadRef& InSpawnRate,
							const FFloatReadRef& InSpawnChance,
							const FFloatReadRef& InRadiusDist,
							const FFloatReadRef& InRadiusMin,
							const FFloatReadRef& InRadiusMax,
							const FFloatReadRef& InRadiusOffset,
							const FFloatReadRef& InPitchShift,
							const FFloatReadRef& InAmpDist,
							const FFloatReadRef& InRadiusToAmp,
							const FFloatReadRef& InAmpOffset,
							const FFloatReadRef& InGain,
							const FFloatReadRef& InGainMax,							
							const FFloatReadRef& InDecayToChirpRatio,
							const FFloatReadRef& InDecayScale,
							const FFloatReadRef& InDecayThresh,
							const FRCurveExtendReadRef& InFreqAmpCurve);

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		void Reset(const IOperator::FResetParams& InParams);
		void Execute();
		void ExecuteSubBlocks();
		void InitSynthesizers();
		void RenderFrameRange(int32 StartFrame, int32 EndFrame);
		
	private:
		const FOperatorSettings OperatorSettings;
		
		FTriggerReadRef PlayTrigger;
		
		FBoolReadRef bIsAutoStop;
		int32 Seed;
		int32 MaxNumBurbles;
		FFloatReadRef SpawnRate;
		FFloatReadRef SpawnChance;
		FFloatReadRef RadiusDist;
		FFloatReadRef RadiusMin;
		FFloatReadRef RadiusMax;
		FFloatReadRef RadiusOffset;
		FFloatReadRef PitchShift;
		
		FFloatReadRef AmpDist;
		FFloatReadRef RadiusToAmp;
		FFloatReadRef AmpOffset;
		FFloatReadRef Gain;		
		FFloatReadRef GainMax;
		FFloatReadRef DecayToChirpRatio;
		FFloatReadRef DecayScale;
		FFloatReadRef DecayThresh;
		FRCurveExtendReadRef FreqAmpCurve;

		FTriggerWriteRef TriggerOnDone;
		FAudioBufferWriteRef AudioOutput;

		float SamplingRate;
		int32 NumFramesPerBlock;

		TArrayView<float> OutputAudioView;
		TUniquePtr<FBurbleSoundGen> BurbleGen;
		bool bIsPlaying;
		bool bFinish;
	};

	FBurbleGenOperator::FBurbleGenOperator(const FOperatorSettings& InSettings,
											const FTriggerReadRef& InTriggerPlay,
											const FBoolReadRef& InIsAutoStop,
											int32 InSeed,
											int32 InMaxNumBurbles,
											const FFloatReadRef& InSpawnRate,
											const FFloatReadRef& InSpawnChance,
											const FFloatReadRef& InRadiusDist,
											const FFloatReadRef& InRadiusMin,
											const FFloatReadRef& InRadiusMax,
											const FFloatReadRef& InRadiusOffset,
											const FFloatReadRef& InPitchShift,
											const FFloatReadRef& InAmpDist,
											const FFloatReadRef& InRadiusToAmp,
											const FFloatReadRef& InAmpOffset,
											const FFloatReadRef& InGain,
											const FFloatReadRef& InGainMax,
											const FFloatReadRef& InDecayToChirpRatio,
											const FFloatReadRef& InDecayScale,
											const FFloatReadRef& InDecayThresh,
											const FRCurveExtendReadRef& InFreqAmpCurve)
		: OperatorSettings(InSettings)
		, PlayTrigger(InTriggerPlay)
		, bIsAutoStop(InIsAutoStop)
		, Seed(InSeed)
		, MaxNumBurbles(InMaxNumBurbles)
		, SpawnRate(InSpawnRate)
		, SpawnChance(InSpawnChance)
		, RadiusDist(InRadiusDist)
		, RadiusMin(InRadiusMin)
		, RadiusMax(InRadiusMax)
		, RadiusOffset(InRadiusOffset)
		, PitchShift(InPitchShift)
		, AmpDist(InAmpDist)
		, RadiusToAmp(InRadiusToAmp)
		, AmpOffset(InAmpOffset)
		, Gain(InGain)
		, GainMax(InGainMax)		
		, DecayToChirpRatio(InDecayToChirpRatio)
		, DecayScale(InDecayScale)
		, DecayThresh(InDecayThresh)
		, FreqAmpCurve(InFreqAmpCurve)
		, TriggerOnDone(FTriggerWriteRef::CreateNew(InSettings))
		, AudioOutput(FAudioBufferWriteRef::CreateNew(InSettings))
	{
		SamplingRate = InSettings.GetSampleRate();
		NumFramesPerBlock = InSettings.GetNumFramesPerBlock();
		
		OutputAudioView = TArrayView<float>(AudioOutput->GetData(), AudioOutput->Num());
		bIsPlaying = false;
		bFinish = false;
	}

	void FBurbleGenOperator::BindInputs(FInputVertexInterfaceData& InOutVertexData)
	{
		using namespace BurbleGenVertexNames;
		
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerPlay), PlayTrigger);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputIsAutoStop), bIsAutoStop);
		InOutVertexData.SetValue(METASOUND_GET_PARAM_NAME(InputSeed), Seed);
		InOutVertexData.SetValue(METASOUND_GET_PARAM_NAME(InputMaxNumBurbles), MaxNumBurbles);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSpawnRate), SpawnRate);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSpawnChance), SpawnChance);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRadiusDist), RadiusDist);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRadiusMin), RadiusMin);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRadiusMax), RadiusMax);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRadiusOffset), RadiusOffset);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPitchShift), PitchShift);
		
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAmpDist), AmpDist);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRadiusAmpFactor), RadiusToAmp);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAmpOffset), AmpOffset);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputGain), Gain);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputGainMax), GainMax);
		
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDecayToChirpRatio), DecayToChirpRatio);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDecayScale), DecayScale);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDecayThresh), DecayThresh);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFreqAmpCurve), FreqAmpCurve);
	}

	void FBurbleGenOperator::BindOutputs(FOutputVertexInterfaceData& InOutVertexData)
	{
		using namespace BurbleGenVertexNames;
		
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnDone), TriggerOnDone);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputAudio), AudioOutput);
	}

	FDataReferenceCollection FBurbleGenOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FBurbleGenOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}
	
	void FBurbleGenOperator::Reset(const IOperator::FResetParams& InParams)
	{
		TriggerOnDone->Reset();
		AudioOutput->Zero();
		BurbleGen.Reset();
		
		bIsPlaying = false;
		bFinish = false;
	}

	void FBurbleGenOperator::Execute()
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::BurbleGenVertexNames::Execute);
		
		TriggerOnDone->AdvanceBlock();

		// zero output buffers
		FMemory::Memzero(OutputAudioView.GetData(), NumFramesPerBlock * sizeof(float));
			
		ExecuteSubBlocks();
	}

	void FBurbleGenOperator::ExecuteSubBlocks()
	{
		// Parse triggers and render audio
		int32 PlayTrigIndex = 0;
		int32 NextPlayFrame = 0;
		const int32 NumPlayTrigs = PlayTrigger->NumTriggeredInBlock();

		int32 NextAudioFrame = 0;
		int32 CurrAudioFrame = 0;
		const int32 LastAudioFrame = OperatorSettings.GetNumFramesPerBlock() - 1;
		const int32 NoTrigger = OperatorSettings.GetNumFramesPerBlock() << 1; //Same as multiply by 2

		while (NextPlayFrame < LastAudioFrame)
		{
			// get the next Play and Stop indices
			NextPlayFrame = PlayTrigIndex < NumPlayTrigs ? (*PlayTrigger)[PlayTrigIndex] : NoTrigger;   
			NextAudioFrame = FMath::Min(NextPlayFrame, OperatorSettings.GetNumFramesPerBlock());
				
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
		}
	}

	void FBurbleGenOperator::InitSynthesizers()
	{
		BurbleGen.Reset();
		
		BurbleGen = MakeUnique<FBurbleSoundGen>(SamplingRate, Seed, MaxNumBurbles);
		bIsPlaying = BurbleGen.IsValid();
	}

	void FBurbleGenOperator::RenderFrameRange(int32 StartFrame, int32 EndFrame)
	{
		if(!bIsPlaying)
			return;
			
		const int32 NumFramesToGenerate = EndFrame - StartFrame;
		if (NumFramesToGenerate <= 0)
		{
			UE_LOG(LogImpactSFXSynth, Warning, TEXT("BurbleGenNodes::RenderFrameRange: StartFrame = %d and EndFrame = %d are invalid!"), StartFrame, EndFrame);
			return;
		}

		BurbleGen->SetFreqAmpCurve(FreqAmpCurve->GetProxy());
		
		const float InSpawnRate = *SpawnRate;
		const float InSpawnChance = *SpawnChance;
		TArrayView<float>  BufferToGenerate = OutputAudioView.Slice(StartFrame, NumFramesToGenerate);
		FBurbleSoundSpawnParams SpawnParams = FBurbleSoundSpawnParams(InSpawnRate,
																	InSpawnChance,
															*RadiusDist,
															*RadiusMin * 1e-3f,
															*RadiusMax * 1e-3f,
															*RadiusOffset * 1e-3f,
															*PitchShift,
														*AmpDist,
														*RadiusToAmp,
														*AmpOffset,
														*Gain,
														*GainMax,
														*DecayToChirpRatio,
														*DecayScale,
														*DecayThresh); 

		BurbleGen->Generate(BufferToGenerate, SpawnParams);
		const bool bIsShouldStop = *bIsAutoStop && ((InSpawnRate <= 0.f) || (InSpawnChance <= 0.f));
		if(bIsShouldStop && BurbleGen->GetCurrentNumBurbles() == 0)
		{
			BurbleGen.Reset();
			bIsPlaying = false;
			TriggerOnDone->TriggerFrame(EndFrame);
		}
	}

	const FVertexInterface& FBurbleGenOperator::GetVertexInterface()
	{
		using namespace BurbleGenVertexNames;

		FDataVertexMetadata DecayScaleMeta = METASOUND_GET_PARAM_METADATA(InputDecayScale);
		DecayScaleMeta.bIsAdvancedDisplay = true;
		FDataVertexMetadata DecayThreshMeta = METASOUND_GET_PARAM_METADATA(InputDecayThresh);
		DecayThreshMeta.bIsAdvancedDisplay = true;
		FDataVertexMetadata FreqAmpCurveMeta = METASOUND_GET_PARAM_METADATA(InputFreqAmpCurve);
		FreqAmpCurveMeta.bIsAdvancedDisplay = true;
		
		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerPlay)),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputIsAutoStop), true),
				TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSeed), -1),
				TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMaxNumBurbles), 512),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSpawnRate), 1000.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSpawnChance), 0.9f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRadiusDist), 1.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRadiusMin), 0.15f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRadiusMax), 150.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRadiusOffset), 0.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPitchShift), 0.f),
				
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmpDist), 0.7634f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRadiusAmpFactor), 1.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmpOffset), 0.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputGain), 10.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputGainMax), 0.2f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDecayToChirpRatio), 0.1f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME(InputDecayScale), DecayScaleMeta, 1.f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME(InputDecayThresh), DecayThreshMeta, 1000.f),
				TInputDataVertex<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputFreqAmpCurve), FreqAmpCurveMeta)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnDone)),
				TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
			)
		);

		return Interface;
	}

	const FNodeClassMetadata& FBurbleGenOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { ImpactSFXSynthEngineNodes::Namespace, TEXT("Burble Sound Gen"), TEXT("") };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("Metasound_BurbleGenDisplayName", "Burble Sound Gen");
			Info.Description = METASOUND_LOCTEXT("Metasound_BurbleGenNodeDescription", "Generate multiple burble sounds for liquid SFX simulation.");
			Info.Author = TEXT("Le Binh Son");
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Generators);
			Info.Keywords = { METASOUND_LOCTEXT("ImpactSFXSyntKeyword", "Synthesis") };
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	TUniquePtr<IOperator> FBurbleGenOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		const FInputVertexInterfaceData& InputData = InParams.InputData;

		using namespace BurbleGenVertexNames;
		
		FTriggerReadRef InTriggerPlay = InputData.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerPlay), InParams.OperatorSettings);
		
		FBoolReadRef InIsAutoStop = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputIsAutoStop), InParams.OperatorSettings);
		int32 InSeed = InputData.GetOrCreateDefaultValue<int32>(METASOUND_GET_PARAM_NAME(InputSeed), InParams.OperatorSettings);
		int32 InMaxNumBurbles = InputData.GetOrCreateDefaultValue<int32>(METASOUND_GET_PARAM_NAME(InputMaxNumBurbles), InParams.OperatorSettings);
		FFloatReadRef InSpawnRate = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSpawnRate), InParams.OperatorSettings);
		FFloatReadRef InSpawnChance = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSpawnChance), InParams.OperatorSettings);
		FFloatReadRef InRadiusDist = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputRadiusDist), InParams.OperatorSettings);
		FFloatReadRef InRadiusMin = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputRadiusMin), InParams.OperatorSettings);
		FFloatReadRef InRadiusMax = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputRadiusMax), InParams.OperatorSettings);
		FFloatReadRef InRadiusOffset = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputRadiusOffset), InParams.OperatorSettings);
		FFloatReadRef InPitchShift = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputPitchShift), InParams.OperatorSettings);
		
		FFloatReadRef InAmpDist = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputAmpDist), InParams.OperatorSettings);
		FFloatReadRef InRadiusToAmp = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputRadiusAmpFactor), InParams.OperatorSettings);
		
		FFloatReadRef InAmpOffset = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputAmpOffset), InParams.OperatorSettings);
		FFloatReadRef InGain = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputGain), InParams.OperatorSettings);
		FFloatReadRef InGainMax = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputGainMax), InParams.OperatorSettings);		
		FFloatReadRef InDecayToChirpRatio = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDecayToChirpRatio), InParams.OperatorSettings);
		FFloatReadRef InDecayScale = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDecayScale), InParams.OperatorSettings);
		FFloatReadRef InDecayThresh = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDecayThresh), InParams.OperatorSettings);
		
		FRCurveExtendReadRef InFreqAmpCurve = InputData.GetOrCreateDefaultDataReadReference<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputFreqAmpCurve), InParams.OperatorSettings);
		
		return MakeUnique<FBurbleGenOperator>(InParams.OperatorSettings,
										       InTriggerPlay,
										       InIsAutoStop,
										       InSeed,
										       InMaxNumBurbles,
										       InSpawnRate,
										       InSpawnChance,
										       InRadiusDist,
										       InRadiusMin,
										       InRadiusMax,
										       InRadiusOffset,
										       InPitchShift,
										       InAmpDist,
										       InRadiusToAmp,
										       InAmpOffset,
										       InGain,
										       InGainMax,										       
										       InDecayToChirpRatio,
										       InDecayScale,
										       InDecayThresh,
										       InFreqAmpCurve);
	}

	class FBurbleGenNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		FBurbleGenNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FBurbleGenOperator>())
		{
		}
	};

	METASOUND_REGISTER_NODE(FBurbleGenNode)
}

#undef LOCTEXT_NAMESPACE
