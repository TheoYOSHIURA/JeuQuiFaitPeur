 // Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "Runtime/Launch/Resources/Version.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "Piano/MetasoundPianoModel.h"
#include "MetasoundPrimitives.h"
#include "Extend\MetasoundRCurveExtend.h"
#include "MetasoundVertex.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundTrace.h"
#include "SHMidiNoteAction.h"
#include "VirtualInstrumentEngineNodesName.h"
#include "Piano/PianoSynth.h"
#include "SHVirtualInstrumentLog.h"

#include "HarmonixMetasound/MidiOps/StuckNoteGuard.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"


#define LOCTEXT_NAMESPACE "SHVirtualInstrumentSynthNodes_PianoSynthNode"

 namespace LBSVirtualInstrument
 {
 	using namespace Metasound;
 	using namespace HarmonixMetasound;
 	
 	namespace PianoSynthVertexNames
 	{
 		METASOUND_PARAM(InputIsEnable, "Enable", "Process incoming MIDI events if true.")

 		METASOUND_PARAM(InputMidiStream, "Midi Stream", "The MIDI stream to be used.")
 		METASOUND_PARAM(InputTrackIndex, "Track Index", "The index of the track to be used in the MIDI file. Default to track 1.")
 		
 		METASOUND_PARAM(InputPianoModel, "Piano Model", "The piano model to be used.")
 		METASOUND_PARAM(InputGain, "Gain", "The master gain applied to the whole system (keys, hammers, etc.). The audio output value is always clamped between [-1, 1].")
 		METASOUND_PARAM(InputKeyVelocityScale, "Velocity Scale", "Scale velocity of keys to make them brighter/darker. This also affects note loudness so you might want to adjust Key Gain pin too.")
 		METASOUND_PARAM(InputKeyGain, "Key Gain", "The gain of a key when it's played (Note On event.)")
 		METASOUND_PARAM(InputSymResonGain, "Sympathetic Resonance Gain", "Range [0, 5].")
 		METASOUND_PARAM(InputHammerGain, "Hammer Gain", "Range [0, 2]. The gain of the hammer sound when a key is played.")
 		METASOUND_PARAM(InputDynamicAdjust, "Dynamic Adjust", "Range [0, 2]. 0 = disable. Automatically adjust the dynamic range of new notes.")
 		METASOUND_PARAM(InputSoundboardGain, "Soundboard Gain", "Range [0, 5]. The gain of the soundboard.")
 		METASOUND_PARAM(InputSoundboardQualityDown, "Soundboard Quality Scale Down", "1 = highest quality. 4 = lowest quality.")
 		 
 		METASOUND_PARAM(InputVelocityRemapCurve, "Velocity Remap Curve", "Value range [0, 127]. The X axis is the velocity value from the Midi stream. If available, remap the velocity of all notes by using the specified curve before multiplying it with the Velocity Scale input above.")
 		METASOUND_PARAM(InputNoteGainCurve, "Note Gain Curve", "Value range [0, 1]. The X axis is the midi note number. If available, multiplying the gain of each key and its hammer noise with values from the specified curve.")
 		METASOUND_PARAM(InputHammerGainCurve, "Hammer Gain Curve", "Value range [0, 2]. The X axis is the midi note number. This allows you to adjust the gain of hammer noise for each key separately.")
 		
 		METASOUND_PARAM(OutputAudio, "Out Mono", "Output audio.")
 	}

 	class FPianoSynthOperator : public TExecutableOperator<FPianoSynthOperator>
 	{
 	public:

 		static const FNodeClassMetadata& GetNodeInfo();
 		static const FVertexInterface& GetVertexInterface();
 		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

 		FPianoSynthOperator(const FOperatorSettings& InSettings,
 							const FBoolReadRef& InIsEnable,
							const FMidiStreamReadRef& InMidiStream,
							const FInt32ReadRef& InTrackNumber,
							const FPianoModelReadRef& InPianoModel,
							const FFloatReadRef& InGain,
							const FFloatReadRef& InKeyVelScale,
							const FFloatReadRef& InKeyGain,
							const FFloatReadRef& InSymResonGain,
							const FFloatReadRef& InHammerGain,
							const FFloatReadRef& InDynamicAdjust,
							const FFloatReadRef& InSoundboardGain,
							const int32 InSoundboardQualityDown,
							const FRCurveExtendReadRef& InVelocityRemapCurve,
							const FRCurveExtendReadRef& InNoteGainCurve,
							const FRCurveExtendReadRef& InHammerGainCurve);

 		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override;
 		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override;
 		virtual FDataReferenceCollection GetInputs() const override;
 		virtual FDataReferenceCollection GetOutputs() const override;
 		void Reset(const IOperator::FResetParams& InParams);
 		void Execute();
 		void InitSynthesizers();
	    void OnStop();
	    FORCEINLINE bool IsValidMidiEvent(const FMidiMsg& MidiMessage, const int32 InTrackIndex) const;
	    void ProcessMidiEvents();

 		void HandleMidiMessage(FMidiVoiceId InVoiceId, uint8 InStatus, uint8 InData1, uint8 InData2, int32 InEventTick = 0, int32 InCurrentTick = 0, float InMsOffset = 0.0f);
 		
 	private:
 		const FOperatorSettings OperatorSettings;
 		
 		FBoolReadRef bEnable;

	    FMidiStreamReadRef MidiStream;
 		FInt32ReadRef TrackIndex;
 		FPianoModelReadRef PianoModel;
 		FFloatReadRef SystemGain;
 		FFloatReadRef KeyVelocityScale;
 		FFloatReadRef KeyGain;
 		FFloatReadRef SymResonGain;
 		FFloatReadRef HammerGain;
 		FFloatReadRef DynamicAdjust;
 		FFloatReadRef SoundboardGain; 		
		int32 SoundboardQualityDown;

 		FRCurveExtendReadRef VelocityRemapCurve;
 		FRCurveExtendReadRef NoteGainCurve;
 		FRCurveExtendReadRef HammerGainCurve;
 		
 		FAudioBufferWriteRef AudioOutput;

 		float SamplingRate;
 		int32 NumFramesPerBlock;
 		
 		TArrayView<float> OutputAudioView;
 		TUniquePtr<FPianoSynth> PianoSynth;
 		TUniquePtr<FMidiEventParser> MidiEventParser;
 		TUniquePtr<Harmonix::Midi::Ops::FStuckNoteGuard> StuckNoteGuard;
 		
 		bool bIsInit;
 		int32 CurrentTrackNumber;
 		FPianoModelAssetProxyPtr CurrentPianoModel = nullptr;
 	};

 	FPianoSynthOperator::FPianoSynthOperator(const FOperatorSettings& InSettings,
 		const FBoolReadRef& InIsEnable,
 		const FMidiStreamReadRef& InMidiStream,
 		const FInt32ReadRef& InTrackNumber,
 		const FPianoModelReadRef& InPianoModel,
 		const FFloatReadRef& InGain,
 		const FFloatReadRef& InKeyVelScale,
 		const FFloatReadRef& InKeyGain,
 		const FFloatReadRef& InSymResonGain,
 		const FFloatReadRef& InHammerGain,
 		const FFloatReadRef& InDynamicAdjust,
 		const FFloatReadRef& InSoundboardGain,
 		const int32 InSoundboardQualityDown,
 		const FRCurveExtendReadRef& InVelocityRemapCurve,
		const FRCurveExtendReadRef& InNoteGainCurve,
		const FRCurveExtendReadRef& InHammerGainCurve)
 		: OperatorSettings(InSettings)
 		, bEnable(InIsEnable)
 		, MidiStream(InMidiStream)
 		, TrackIndex(InTrackNumber)
 		, PianoModel(InPianoModel)
 	    , SystemGain(InGain)
 	    , KeyVelocityScale(InKeyVelScale)
 		, KeyGain(InKeyGain)
 		, SymResonGain(InSymResonGain)
 		, HammerGain(InHammerGain)
 		, DynamicAdjust(InDynamicAdjust)
 		, SoundboardGain(InSoundboardGain)
 		, SoundboardQualityDown(InSoundboardQualityDown)
 		, VelocityRemapCurve(InVelocityRemapCurve)
 		, NoteGainCurve(InNoteGainCurve)
 		, HammerGainCurve(InHammerGainCurve)
 		, AudioOutput(FAudioBufferWriteRef::CreateNew(InSettings))
 	{
 		SamplingRate = InSettings.GetSampleRate();
 		NumFramesPerBlock = InSettings.GetNumFramesPerBlock();
 		
 		OutputAudioView = TArrayView<float>(AudioOutput->GetData(), AudioOutput->Num()); 		
 		CurrentTrackNumber = *TrackIndex;
 		bIsInit = false;
 		
 		if(*bEnable)
 			InitSynthesizers();
 	}

 	void FPianoSynthOperator::BindInputs(FInputVertexInterfaceData& InOutVertexData)
 	{
 		using namespace PianoSynthVertexNames;

 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputIsEnable), bEnable);
 		
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMidiStream), MidiStream);
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrackIndex), TrackIndex);
 		
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPianoModel), PianoModel);
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputGain), SystemGain);
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputKeyVelocityScale), KeyVelocityScale);
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputKeyGain), KeyGain);
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSymResonGain), SymResonGain);
 		
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputHammerGain), HammerGain);
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDynamicAdjust), DynamicAdjust);
 		
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSoundboardGain), SoundboardGain);
 		InOutVertexData.SetValue(METASOUND_GET_PARAM_NAME(InputSoundboardQualityDown), SoundboardQualityDown);

 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputVelocityRemapCurve), VelocityRemapCurve);
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteGainCurve), NoteGainCurve);
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputHammerGainCurve), HammerGainCurve);
 	}

 	void FPianoSynthOperator::BindOutputs(FOutputVertexInterfaceData& InOutVertexData)
 	{
 		using namespace PianoSynthVertexNames;
 		
 		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputAudio), AudioOutput);
 	}

 	FDataReferenceCollection FPianoSynthOperator::GetInputs() const
 	{
 		// This should never be called. Bind(...) is called instead. This method
 		// exists as a stop-gap until the API can be deprecated and removed.
 		checkNoEntry();
 		return {};
 	}

 	FDataReferenceCollection FPianoSynthOperator::GetOutputs() const
 	{
 		// This should never be called. Bind(...) is called instead. This method
 		// exists as a stop-gap until the API can be deprecated and removed.
 		checkNoEntry();
 		return {};
 	}
 	
 	void FPianoSynthOperator::Reset(const IOperator::FResetParams& InParams)
 	{
 		AudioOutput->Zero();
 		OnStop();
 	}

 	void FPianoSynthOperator::Execute()
 	{
 		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::PianoSynthVertexNames::Execute);
 		
 		FMemory::Memzero(OutputAudioView.GetData(), NumFramesPerBlock * sizeof(float));
 		
 		if(!*bEnable)
 		{
 			if(bIsInit)
 			{
 				OnStop();
 			}
 			return;
 		}
 		
 		if(!bIsInit || CurrentPianoModel != PianoModel->GetProxy())
 			InitSynthesizers();

 		if(bIsInit)
 		{
 			PianoSynth->SetVelocityRemapCurve(VelocityRemapCurve->GetProxy());
 			PianoSynth->SetNoteGainCurve(NoteGainCurve->GetProxy());
 			PianoSynth->SetHammerGainCurve(HammerGainCurve->GetProxy());
 			
 			ProcessMidiEvents();
 		}
 	}
 	
 	void FPianoSynthOperator::InitSynthesizers()
 	{
 		PianoSynth.Reset();
 		StuckNoteGuard.Reset();
 		MidiEventParser.Reset();
 		
 		const FPianoModelAssetProxyPtr& PianoModelProxy = PianoModel->GetProxy();
 		CurrentPianoModel = PianoModelProxy;
 		if(PianoModelProxy)
 		{
 			PianoSynth = MakeUnique<FPianoSynth>(PianoModelProxy, SamplingRate, *SoundboardGain, SoundboardQualityDown);
 			MidiEventParser = MakeUnique<FMidiEventParser>(PianoModelProxy->GetStartMidiNote(), PianoModelProxy->GetNumKeys());
 			StuckNoteGuard = MakeUnique<Harmonix::Midi::Ops::FStuckNoteGuard>();
 		}
 		
 		bIsInit = PianoSynth.IsValid() && MidiEventParser.IsValid() && StuckNoteGuard.IsValid();
 	}

    void FPianoSynthOperator::OnStop()
    {
	    PianoSynth.Reset();
 		MidiEventParser.Reset();
 		StuckNoteGuard.Reset();
 		CurrentPianoModel = nullptr;
	    bIsInit = false;
    }
 	
    void FPianoSynthOperator::ProcessMidiEvents()
 	{
 		// This guards against changing MidiStream while playing.
 		// For looping and seeking, all note off/kill events are used to remove all current playing notes.
 		StuckNoteGuard->UnstickNotes(*MidiStream, [this](const FMidiStreamEvent& Event)
 		{
 			MidiEventParser->NoteOff(Event.GetVoiceId(), Event.MidiMessage.GetStdData1(), Event.MidiMessage.GetStdChannel());
 			UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoSynthOperator::ProcessMidiEvents: Unstuck note %d"), Event.MidiMessage.GetStdData1());
 		});
 		
 		int32 TrackIdx = *TrackIndex;
 		if (TrackIdx != CurrentTrackNumber)
 		{
 			MidiEventParser->ResetState();
 			PianoSynth->OffAllNotes();
 			CurrentTrackNumber = TrackIdx;
 		}

		int32 CurrentBlockFrameIndex = 0;

		// create an iterator for midi events in the block
		const TArray<FMidiStreamEvent>& MidiEvents = MidiStream->GetEventsInBlock();
		auto MidiEventIterator = MidiEvents.begin();
 		
		const TSharedPtr<const FMidiClock, ESPMode::NotThreadSafe> MidiClock = MidiStream->GetClock();
 		
 		int32 NumFramesToGenerate = OutputAudioView.Num();
 		float CurrentTime = -1.f;
		while (NumFramesToGenerate > 0)
		{
			int32 NumFrameToSynth = NumFramesToGenerate;
			
			while (MidiEventIterator != MidiEvents.end())
			{
				int32 BlockSampleFrameIndex = (*MidiEventIterator).BlockSampleFrameIndex;
				//Get events with the same tick in MIDI based on block rate duration
				if (BlockSampleFrameIndex <= CurrentBlockFrameIndex)
				{
					const FMidiMsg& MidiMessage = (*MidiEventIterator).MidiMessage;
					if (IsValidMidiEvent(MidiMessage, (*MidiEventIterator).TrackIndex))
					{
						HandleMidiMessage(
							(*MidiEventIterator).GetVoiceId(),
							MidiMessage.GetStdStatus(),
							MidiMessage.GetStdData1(),
							MidiMessage.GetStdData2(),
							(*MidiEventIterator).AuthoredMidiTick,
							(*MidiEventIterator).CurrentMidiTick,
							0.0f);
					}
					else if (MidiMessage.IsAllNotesOff())
					{
						PianoSynth->OffAllNotes();
						MidiEventParser->ResetState();
					}
					else if (MidiMessage.IsAllNotesKill())
					{
						PianoSynth->KillAllNotes();
						MidiEventParser->ResetState();
					}
					++MidiEventIterator;
				}
				else
				{
					NumFrameToSynth = FMath::Min(BlockSampleFrameIndex - CurrentBlockFrameIndex, NumFramesToGenerate);
					break;
				}
			}
			
			// UE 5.5: Copy from Fusion Sampler code.
			// Though, these variables currently aren't used in our code path
			if (MidiClock.IsValid())
			{
				const float ClockSpeed = MidiClock->GetSpeedAtBlockSampleFrame(CurrentBlockFrameIndex);
				MidiEventParser->SetSpeed(ClockSpeed);
				const float ClockTempo = MidiClock->GetTempoAtBlockSampleFrame(CurrentBlockFrameIndex);
				MidiEventParser->SetTempo(ClockTempo);

#if ENGINE_MINOR_VERSION > 4
				// This next thing is *close* to being accurate, but not really. 
				// It gets the last processed beat in this metasound render block, which may or may not
				// be correct for this specific sample in the block we are processing at the moment. 
				// It should be close enough. If not, we will need to ask the clock specifically what
				// its tick was at this point in the metasound rendering block
				const int32 Tick = MidiClock->GetNextMidiTickToProcess();
				const ISongMapEvaluator& ClocksMap = MidiClock->GetSongMapEvaluator();
				const float MidiQuarterNote = static_cast<float>(Tick) / ClocksMap.GetTicksPerQuarterNote();
				CurrentTime = MidiClock->GetCurrentSongPosMs();
#else
				const float MidiQuarterNote = MidiClock->GetQuarterNoteIncludingCountIn();
				CurrentTime = MidiClock->GetCurrentHiResMs();
#endif
				
				MidiEventParser->SetQuarterNote(MidiQuarterNote);
			}
			
			//Start synthesize based on event in the current requested frames
			TMap<FMidiVoiceId, FMidiNoteAction> NotesOn;
			TArray<FMidiVoiceId> NotesOff;
			MidiEventParser->ProcessNoteActions(NumFrameToSynth, NotesOn, NotesOff);
			TArrayView<float>  BufferToGenerate = OutputAudioView.Slice(CurrentBlockFrameIndex, NumFrameToSynth);
			FPianoSynthParams PianoSynthParams= FPianoSynthParams(
																 *SystemGain * MidiEventParser->GetVolumeWithExpression(),
																 FMath::Max(0.001f, *KeyVelocityScale),
																 *KeyGain,
																 FMath::Clamp(*HammerGain, 0.f, 2.f),
																 FMath::Clamp(*DynamicAdjust, 0.f, 2.f),
																 FMath::Clamp(*SoundboardGain, 0.f, 5.f),
																 FMath::Clamp(*SymResonGain, 0.f, 5.f),
																 MidiEventParser->GetIsSusPedalOn(),
																 MidiEventParser->GetAndValidateSosPedal()
																);
			
			PianoSynth->Synthesize(BufferToGenerate, NotesOn, NotesOff, PianoSynthParams, CurrentTime);
			
			NumFramesToGenerate -= NumFrameToSynth;
			CurrentBlockFrameIndex += NumFrameToSynth;
		}

		// There may be some remaining midi at the end of this block that we need to render in the next block
		while (MidiEventIterator != MidiEvents.end())
		{
			const FMidiMsg& MidiMessage = (*MidiEventIterator).MidiMessage;
			if (IsValidMidiEvent(MidiMessage, (*MidiEventIterator).TrackIndex))
			{
				UE_LOG(LogVirtualInstrument, Log, TEXT("FPianoSynthOperator::ProcessMidiEvents: Received out of block events at time %f"), CurrentTime);
				
				HandleMidiMessage(
					(*MidiEventIterator).GetVoiceId(),
					MidiMessage.GetStdStatus(),
					MidiMessage.GetStdData1(),
					MidiMessage.GetStdData2(),
					(*MidiEventIterator).AuthoredMidiTick,
					(*MidiEventIterator).CurrentMidiTick,
					0.0f);
			}
			++MidiEventIterator;
		}
 	}

 	bool FPianoSynthOperator::IsValidMidiEvent(const FMidiMsg& MidiMessage, const int32 InTrackIndex) const
    {
 		return MidiMessage.IsStd() && (InTrackIndex == CurrentTrackNumber || CurrentTrackNumber < 0);
 	}
 	
 	void FPianoSynthOperator::HandleMidiMessage(FMidiVoiceId InVoiceId, uint8 InStatus, uint8 InData1, uint8 InData2,
												int32 InEventTick, int32 InCurrentTick, float InMsOffset)
 	{
 		using namespace Harmonix::Midi::Constants;
 		int8 InChannel = InStatus & 0xF;
 		InStatus = InStatus & 0xF0;
 		switch (InStatus)
 		{
 		case GNoteOff:
 			MidiEventParser->NoteOff(InVoiceId, InData1, InChannel);
 			break;
 		case GNoteOn:
 			MidiEventParser->NoteOn(InVoiceId, InData1, InData2, InChannel, InEventTick, InCurrentTick, InMsOffset);
 			break;
 		case GControl:
 			MidiEventParser->SetHighOrLowControllerByte(static_cast<EControllerID>(InData1), InData2, InCurrentTick, InChannel);
 			break;
 		case GPolyPres:
 			UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoSynthOperator::HandleMidiMessage: Poly pressure control isn't supported"))
 			break;
 		case GChanPres:
 			UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoSynthOperator::HandleMidiMessage: Channel pressure control isn't supported"))
 			break;
 		case GPitch:
 			UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoSynthOperator::HandleMidiMessage: Pitch bend control isn't supported"))
 			break;
 		case GProgram:
 			break;
 		case GSystem:
 			break;
 		default:
 			UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoSynthOperator::HandleMidiMessage: unknown note status %d"), InStatus)
 			break;
 		}
 	}

    const FVertexInterface& FPianoSynthOperator::GetVertexInterface()
 	{
 		using namespace PianoSynthVertexNames;
 		
 		FDataVertexMetadata SoundBoardQualityDownMetadata = METASOUND_GET_PARAM_METADATA(InputSoundboardQualityDown);
 		SoundBoardQualityDownMetadata.bIsAdvancedDisplay = true;

 		FDataVertexMetadata VelocityRemapCurveMetadata = METASOUND_GET_PARAM_METADATA(InputVelocityRemapCurve);
 		VelocityRemapCurveMetadata.bIsAdvancedDisplay = true;
 		FDataVertexMetadata NoteGainCurveMetadata = METASOUND_GET_PARAM_METADATA(InputNoteGainCurve);
 		NoteGainCurveMetadata.bIsAdvancedDisplay = true;
 		FDataVertexMetadata HammerGainCurveMetadata = METASOUND_GET_PARAM_METADATA(InputHammerGainCurve);
 		HammerGainCurveMetadata.bIsAdvancedDisplay = true;
 		
 		static const FVertexInterface Interface(
 			FInputVertexInterface(
 				
 				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputIsEnable), true),
 				
 				TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMidiStream)),
 				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrackIndex), 1),

 				TInputDataVertex<FPianoModel>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPianoModel)),
 				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputGain), 1.f),
 				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKeyVelocityScale), 1.f),
 				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKeyGain), 1.f),
 				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSymResonGain), 1.f),
 				
 				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputHammerGain), 1.f),
 				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDynamicAdjust), 1.f),
 				
 				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSoundboardGain), 1.f),
 				TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME(InputSoundboardQualityDown), SoundBoardQualityDownMetadata, 2),
 				
 				TInputDataVertex<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputVelocityRemapCurve), VelocityRemapCurveMetadata),
 				TInputDataVertex<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputNoteGainCurve), NoteGainCurveMetadata),
 				TInputDataVertex<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputHammerGainCurve), HammerGainCurveMetadata)
 			),
 			FOutputVertexInterface(
 				TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
 			)
 		);

 		return Interface;
 	}

 	const FNodeClassMetadata& FPianoSynthOperator::GetNodeInfo()
 	{
 		auto InitNodeInfo = []() -> FNodeClassMetadata
 		{
 			FNodeClassMetadata Info;
 			Info.ClassName = { VirtualInstrumentEngineNodes::Namespace, TEXT("PM Piano Synth"), TEXT("") };
 			Info.MajorVersion = 1;
 			Info.MinorVersion = 1;
 			Info.DisplayName = METASOUND_LOCTEXT("Metasound_PMPianoSynthDisplayName", "PM Piano Synth");
 			Info.Description = METASOUND_LOCTEXT("Metasound_PMPianoSynthNodeDescription", "A piano synthesizer based on physical modelling.");
 			Info.Author = TEXT("Le Binh Son");
 			Info.PromptIfMissing = PluginNodeMissingPrompt;
 			Info.DefaultInterface = GetVertexInterface();
 			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
 			Info.Keywords = { };
 			return Info;
 		};

 		static const FNodeClassMetadata Info = InitNodeInfo();

 		return Info;
 	}

 	TUniquePtr<IOperator> FPianoSynthOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
 	{
 		const FInputVertexInterfaceData& InputData = InParams.InputData;

 		using namespace PianoSynthVertexNames;
 		
 		FBoolReadRef InEnable = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputIsEnable), InParams.OperatorSettings);

 		FMidiStreamReadRef InMidiStream = InputData.GetOrCreateDefaultDataReadReference<FMidiStream>(METASOUND_GET_PARAM_NAME(InputMidiStream), InParams.OperatorSettings);
 		FInt32ReadRef InTrackNumber = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputTrackIndex), InParams.OperatorSettings);
 		
 		FPianoModelReadRef InPianoModel = InputData.GetOrCreateDefaultDataReadReference<FPianoModel>(METASOUND_GET_PARAM_NAME(InputPianoModel), InParams.OperatorSettings);
 		FFloatReadRef InGain = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputGain), InParams.OperatorSettings);
 		FFloatReadRef InKeyVelScale = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputKeyVelocityScale), InParams.OperatorSettings);
 		FFloatReadRef InKeyGain = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputKeyGain), InParams.OperatorSettings);
 		FFloatReadRef InSymResonGain = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSymResonGain), InParams.OperatorSettings);
 		
 		FFloatReadRef InHammerGain = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputHammerGain), InParams.OperatorSettings);
 		FFloatReadRef InDynamicAdjust = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDynamicAdjust), InParams.OperatorSettings);
 		
 		FFloatReadRef InSoundboardGain = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSoundboardGain), InParams.OperatorSettings);
 		int32 InSoundboardQualityDown = InputData.GetOrCreateDefaultValue<int32>(METASOUND_GET_PARAM_NAME(InputSoundboardQualityDown), InParams.OperatorSettings);

 		FRCurveExtendReadRef InVelocityRemapCurve = InputData.GetOrCreateDefaultDataReadReference<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputVelocityRemapCurve), InParams.OperatorSettings);
 		FRCurveExtendReadRef InNoteGainCurve = InputData.GetOrCreateDefaultDataReadReference<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputNoteGainCurve), InParams.OperatorSettings);
 		FRCurveExtendReadRef InHammerGainCurve = InputData.GetOrCreateDefaultDataReadReference<FRCurveExtend>(METASOUND_GET_PARAM_NAME(InputHammerGainCurve), InParams.OperatorSettings);

 		return MakeUnique<FPianoSynthOperator>(InParams.OperatorSettings,
 													InEnable,
 													InMidiStream,
 													InTrackNumber,
 													InPianoModel,
 													InGain,
 													InKeyVelScale,
 													InKeyGain,
 													InSymResonGain,
 													InHammerGain,
 													InDynamicAdjust,
 													InSoundboardGain,
 													InSoundboardQualityDown,
 													InVelocityRemapCurve,
 													InNoteGainCurve,
 													InHammerGainCurve);
 	}

 	class FPianoSynthNode : public FNodeFacade
 	{
 	public:
 		/**
 		 * Constructor used by the Metasound Frontend.
 		 */
 		FPianoSynthNode(const FNodeInitData& InitData)
 			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FPianoSynthOperator>())
 		{
 		}
 	};

 	METASOUND_REGISTER_NODE(FPianoSynthNode)
 }

 #undef LOCTEXT_NAMESPACE
