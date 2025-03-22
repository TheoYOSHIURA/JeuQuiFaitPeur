// Copyright 2023-2024, Le Binh Son, All Rights Reserved.

#include "Piano/PianoSynth.h"

#include "DSP/FloatArrayMath.h"
#include "ModalSynth.h"
#include "SHVirtualInstrumentLog.h"

namespace LBSVirtualInstrument
{
	TArray<float> FPianoSynth::AttackCurvePreSample {0.0f, 0.02720551f, 0.05071795f, 0.08245643f, 0.12529889f, 0.16313016f, 0.26119421f, 0.40656966f, 0.54881164f, 0.74081822f, 1.f};

	FPianoSynth::FPianoSynth(const FPianoModelAssetProxyPtr& InPianoModel,
							 const float InSamplingRate,
	                         const float SoundboardGain,
	                         const int32 SoundBoardQualityDown,
	                         const float InKeyInitDelay,
	                         const float HammerDuration)
		: SamplingRate(InSamplingRate), KeyInitDelay(InKeyInitDelay), CurrentSecond(0.f)
	{
		PianoModel = InPianoModel;
		VelocityRemapCurve = nullptr;
		NoteGainCurve = nullptr;
		HammerGainCurve = nullptr;
		
		const int32 MaxNumKeys = PianoModel->GetNumKeys();
		NoteOffMap.Empty(MaxNumKeys);
		NoteOnMap.Empty(MaxNumKeys);
		SosPedalSnapshot.Empty(MaxNumKeys);
		HammerQueue.Empty(MaxNumKeys);
		
		SecondStageBuffers.Empty(10);
		SecondStageSynths.Empty(10);
		
		const int32 HammerBufferSize = HammerDuration * SamplingRate;
		HammerBuffer.SetNumUninitialized(HammerBufferSize);
		FMemory::Memzero(HammerBuffer.GetData(), HammerBufferSize * sizeof(float));
		
		HammerSynth = MakeUnique<LBSImpactSFXSynth::FModalSynth>(SamplingRate, PianoModel->GetHammerObjProxy(), 0, HammerDuration);
		CurrentHammerBufferIndex = 0;
		
		SoundboardSynth = MakeUnique<FSoundBoardSynth>(SamplingRate, PianoModel->GetSoundboardObjProxy(),
													   SoundboardGain,
											0.022f, SoundBoardQualityDown);
		InitAttackBuffer();
	}

	void FPianoSynth::SetVelocityRemapCurve(const FRCurveExtendAssetProxyPtr& InCurve)
	{
		VelocityRemapCurve = InCurve;
	}

	void FPianoSynth::SetNoteGainCurve(const FRCurveExtendAssetProxyPtr& InCurve)
	{
		NoteGainCurve = InCurve;
	}

	void FPianoSynth::SetHammerGainCurve(const FRCurveExtendAssetProxyPtr& InCurve)
	{		
		HammerGainCurve = InCurve;
	}

	void FPianoSynth::InitAttackBuffer()
	{
		AttackCurveBuffer.SetNumUninitialized(SamplingRate / AttackCurveSamplingRate * (AttackCurvePreSample.Num() - 1));
		const float ResampleStep = AttackCurveSamplingRate / SamplingRate;
		float CurrentBin = 0;
		for(int i = 0; i < AttackCurveBuffer.Num(); i++)
		{
			const int32 LowIdx = static_cast<int32>(CurrentBin);
			const int32 UpIdx = LowIdx + 1;
			const float Percent = CurrentBin - LowIdx;
			AttackCurveBuffer[i] = AttackCurvePreSample[LowIdx] * (1.f - Percent) + AttackCurvePreSample[UpIdx] * Percent;
			CurrentBin += ResampleStep;
		}
	}
	
	void FPianoSynth::Synthesize(TArrayView<float>& OutAudio,
	                             TMap<FMidiVoiceId, FMidiNoteAction>& NotesOn,
	                             TArray<FMidiVoiceId>& NotesOff,
	                             const FPianoSynthParams& SynthParams,
	                             float InCurrentTimeMs)
	{
		if(NotesOn.Num() == 0 && NotesOff.Num() == 0 && IsNotRunning())
			return;

		CurrentSecond = InCurrentTimeMs / 1000.f;
		
		UpdateNotesMaps(NotesOn, NotesOff, SynthParams);
		
		SynthHammerIfNeeded(OutAudio);
		
		SynthNotesOff(OutAudio, SynthParams.IsSusPedalOn);
		
		const bool bIsNewNoteOnTrigger= NotesOn.Num() > 0;
		if(SynthParams.IsSusPedalOn)
			SynthNoteOnWithSusPedalOn(OutAudio);
		else
			SynthNoteOnWithSusPedalOff(OutAudio, bIsNewNoteOnTrigger);		
		
		SoundboardSynth->Synthesize(OutAudio, PianoModel->GetSoundboardObjProxy(), bIsNewNoteOnTrigger,
								 SynthParams.SoundboardGain);
		
		ArrayClampInPlace(OutAudio, -1.f, 1.f);
	}

	bool FPianoSynth::IsNotRunning() const
	{
		return IsHammerSynthFinished()
				&& !SoundboardSynth->IsRunning()
				&& NoteOnMap.Num() == 0
				&& NoteOffMap.Num() == 0
				&& HammerQueue.Num() == 0;
	}

	void FPianoSynth::OffAllNotes()
	{
		if(NoteOnMap.Num() > 0)
		{
			for(auto& NoteOn: NoteOnMap)
				NoteOffMap.Emplace(NoteOn.Key, NoteOn.Value);
		
			NoteOnMap.Reset();
		}
		
		SosPedalSnapshot.Reset();
	}

	void FPianoSynth::KillAllNotes()
	{
		NoteOnMap.Reset();
		NoteOffMap.Reset();
		SosPedalSnapshot.Reset();
		HammerQueue.Reset();
	}
	
	void FPianoSynth::UpdateNotesMaps(TMap<FMidiVoiceId, FMidiNoteAction>& NotesOn, TArray<FMidiVoiceId>& NotesOff,
	                                  const FPianoSynthParams& SynthParams)
	{
		UpdateAllMapIfSosPedalChanged(NotesOn, NotesOff, SynthParams);
		UpdateNotesOffMap(NotesOff);
		UpdateNotesOnMap(NotesOn, SynthParams);
		AdjustDynamicRangeIfNeeded(SynthParams);
	}

	void FPianoSynth::UpdateAllMapIfSosPedalChanged(TMap<FMidiVoiceId, FMidiNoteAction>& NotesOn, TArray<FMidiVoiceId>& NotesOff, const FPianoSynthParams& SynthParams)
	{
		if(SynthParams.SosPedal == EPedalState::TriggerOn)
		{
			// First, take a snapshot of current ON notes
			for(auto& NoteOn: NoteOnMap)
				SosPedalSnapshot.Emplace(NoteOn.Key, true);

			// Remember the off state of current notes to set them on trigger off
			for (auto NoteOff : NotesOff)
			{
				if(SosPedalSnapshot.Contains(NoteOff))
					SosPedalSnapshot[NoteOff] = false;
			}
			NotesOff.Reset();
			
			for(auto& NoteOn: NotesOn)
				SosPedalSnapshot.Emplace(NoteOn.Key, true);
		}
		else if(SynthParams.SosPedal == EPedalState::TriggerOff)
		{
			for(auto& Note: SosPedalSnapshot)
			{
				if(!Note.Value)
					NotesOff.Emplace(Note.Key);
			}
			SosPedalSnapshot.Reset();
		}
		else if (SosPedalSnapshot.Num() > 0)
		{
			for(int i = 0; i < NotesOff.Num(); )
			{
				const FMidiVoiceId OffKey = NotesOff[i];
				if(SosPedalSnapshot.Contains(OffKey))
				{
					SosPedalSnapshot[OffKey] = false;
					NotesOff.RemoveAtSwap(i, EAllowShrinking::No);
				}
				else
					i++;
			}
			
			// A note might be on before SosPedal Trigger on, then off while it holds, then on again
			for(auto& NoteOn: NotesOn)
			{
				if(SosPedalSnapshot.Contains(NoteOn.Key))
					SosPedalSnapshot[NoteOn.Key] = true;
			}
		}
	}

	void FPianoSynth::UpdateNotesOffMap(TArray<FMidiVoiceId>& NotesOff)
	{
		for (FMidiVoiceId NoteOff : NotesOff)
		{
			TSharedPtr<FPianoKeySynth>* KeyPtr = NoteOnMap.Find(NoteOff);
			if(KeyPtr == nullptr)
			{
				uint8 Channel;
				uint8 MidiNote;
				NoteOff.GetChannelAndNote(Channel, MidiNote);
				UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoSynth::Synthesize: Received OFF event of note %d with channel %d without a preceding ON event at time %f!"), MidiNote, Channel, CurrentSecond);
				continue;
			}
			TSharedPtr<FPianoKeySynth> Key = *KeyPtr;
			NoteOffMap.Emplace(NoteOff, Key);
			NoteOnMap.Remove(NoteOff);
		}
	}

	void FPianoSynth::UpdateNotesOnMap(TMap<FMidiVoiceId, FMidiNoteAction>& NotesOn, const FPianoSynthParams& SynthParams)
	{
		const float GlobalKeyGain = SynthParams.KeyGain * SynthParams.SystemGain;
		const int32 StartMidi = PianoModel->GetStartMidiNote();
		const float VelocityStandard = PianoModel->GetVelocityStandard();
		const int32 NumKeys = PianoModel->GetNumKeys();
		const TArrayView<const float> AttackView = TArrayView<const float>(AttackCurveBuffer);
		const float SymResonScale = SynthParams.SymResonScale * PianoModel->GetSymResonRescale();
		const float NoteOffDecayDelta = PianoModel->GetNoteOffDecayDelta();
		float HammerVelocity = 0.f;
		for(auto& NoteOn: NotesOn)
		{
			const FMidiNoteAction& NoteAction = NoteOn.Value;

			uint8 NoteVelocity = NoteAction.Velocity;
			if(VelocityRemapCurve)
				NoteVelocity = VelocityRemapCurve->GetValueByTimeInterp(NoteVelocity);
			const float Velocity = FMath::Clamp(SynthParams.VelocityScale * NoteVelocity, 0.f, 127.f);
			const float KeyGain = (NoteGainCurve == nullptr) ? GlobalKeyGain : (GlobalKeyGain * GetNoteGainValueFromCurve(NoteAction.MidiNote)); 
			const int32 KeyIndex = NoteAction.MidiNote - StartMidi;
			if(KeyIndex < 0 || KeyIndex >= NumKeys)
			{
				UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoSynth::Synthesize: MIDI note %d is not supported in the specified piano model!"), NoteAction.MidiNote);
				continue;
			}
			
			UPianoKeyData* KeyData = PianoModel->GetPianoKeyData(KeyIndex);
			TSharedPtr<FPianoKeySynth>* KeyOffPtr =  NoteOffMap.Find(NoteOn.Key);
			TSharedPtr<FPianoKeySynth> KeySynth = nullptr;
			if(KeyOffPtr == nullptr)
			{
				TSharedPtr<FPianoKeySynth>* KeyPtr =  NoteOnMap.Find(NoteOn.Key);
				if(KeyPtr == nullptr)
				{
					KeySynth = MakeShared<FPianoKeySynth>(NoteAction.MidiNote, KeyData, SamplingRate, Velocity,
					                                      SynthParams.IsSusPedalOn, SymResonScale, VelocityStandard,
					                                      KeyInitDelay, KeyGain, AttackView, NoteOffDecayDelta);
					NoteOnMap.Add(NoteOn.Key, KeySynth);
				}
				else
				{
					if(SosPedalSnapshot.Num() == 0)
					{
						UE_LOG(LogVirtualInstrument, Warning, TEXT("FPianoSynth::Synthesize: Received another ON event before OFF event of note %d at time %f!"), NoteAction.MidiNote, CurrentSecond);
					}
					
					KeySynth = *KeyPtr;
					KeySynth->ReStrike(Velocity, KeyGain, SynthParams.IsSusPedalOn, SymResonScale);
				}
			}
			else
			{
				KeySynth = *KeyOffPtr;
				// Action midi note and synth midi note can be different if transpose is changed while playing
				if(KeySynth->GetMidiNote() == NoteAction.MidiNote)
				{ 
					KeySynth->ReStrike(Velocity, KeyGain, SynthParams.IsSusPedalOn, SymResonScale);
				}
				else
				{
					KeySynth = MakeShared<FPianoKeySynth>(NoteAction.MidiNote, KeyData, SamplingRate, Velocity,
					                                      SynthParams.IsSusPedalOn, SymResonScale, VelocityStandard,
					                                      KeyInitDelay, KeyGain, AttackView, NoteOffDecayDelta);
				}
				
				NoteOnMap.Add(NoteOn.Key, KeySynth);
				NoteOffMap.Remove(NoteOn.Key);
			}

			if(KeySynth)
				HammerVelocity += GetKeyHammerVelocity(KeySynth);
		}

		HammerVelocity = FMath::Min(HammerVelocity, 255.f) * SynthParams.HammerGain * SynthParams.SystemGain * HammerReScale;
		if(HammerVelocity > 0.f)
			HammerQueue.Emplace(FHammerState(HammerVelocity));
	}

	void FPianoSynth::AdjustDynamicRangeIfNeeded(const FPianoSynthParams& SynthParams)
	{
		if(SynthParams.DynamicAdjust <= 0.f)
			return;
		
		float CurrentTotalEnergy = 0;
		if(SynthParams.IsSusPedalOn)
		{ // When sustain pedal is on, there is no "actual" Note Off
			for(auto& NoteOff: NoteOffMap)
			{
				CurrentTotalEnergy += NoteOff.Value->EstimateCurrentEnergy();
			}
		}
		
		for(auto& NoteOn: NoteOnMap)
		{
			CurrentTotalEnergy += NoteOn.Value->EstimateCurrentEnergy(); 
		}

		const float AdjustValue = PianoModel->GetDynAdjustThreshold() * SynthParams.DynamicAdjust;
		if(CurrentTotalEnergy > AdjustValue)
		{
			const float MinCompress = PianoModel->GetDynAdjustFactorMin() * FMath::Min(SynthParams.DynamicAdjust, 1.f) / FMath::Max(1.f, SynthParams.SystemGain * SynthParams.KeyGain);
			const float Compress = FMath::Max(AdjustValue / CurrentTotalEnergy, MinCompress);
			for(auto& NoteOn: NoteOnMap)
			{
				if(NoteOn.Value->GetCurrentSampleIndex() > 0)
					continue;
		 
				NoteOn.Value->AdjustEnergy(Compress);
			}
		 
			for(FHammerState& HammerState : HammerQueue)
			{
				if(HammerState.CurrentIndex == 0)
					HammerState.Strength = HammerState.Strength * Compress;  
			}
			// UE_LOG(LogVirtualInstrument, Log, TEXT("%f sec: %f. Compress: %f"), CurrentSecond, CurrentTotalEnergy, Compress);
		}
	}

	void FPianoSynth::SynthHammerIfNeeded(TArrayView<float>& OutAudio)
	{
		const int32 NumOutFrame = OutAudio.Num();
		const int32 HammerBufferSize = HammerBuffer.Num();
		TArrayView<float> HammerBufferView = TArrayView<float>(HammerBuffer.GetData(), HammerBufferSize);
		if(!IsHammerSynthFinished())
		{
			FMultichannelBufferView MultiView;
			const int32 NumGenFrame = FMath::Min(NumOutFrame, HammerBufferSize - CurrentHammerBufferIndex);
			MultiView.Emplace(HammerBufferView.Slice(CurrentHammerBufferIndex, NumGenFrame));
			HammerSynth->Synthesize(MultiView, PianoModel->GetHammerObjProxy(), false, false);
			if(CurrentHammerBufferIndex < AttackCurveBuffer.Num())
			{
				TArrayView<const float> AttackView = TArrayView<const float>(AttackCurveBuffer);
				const int32 NumAttackFrame = FMath::Min(AttackView.Num() - CurrentHammerBufferIndex, NumGenFrame);
				ArrayMultiplyInPlace(AttackView.Slice(CurrentHammerBufferIndex, NumAttackFrame), HammerBufferView.Slice(CurrentHammerBufferIndex, NumAttackFrame));
			}
		
			CurrentHammerBufferIndex += NumGenFrame;
		}
		
		for(int i = 0; i < HammerQueue.Num(); )
		{
			FHammerState& State = HammerQueue[i];
			const int32 NumHammerSample = FMath::Min(NumOutFrame, HammerBuffer.Num() - State.CurrentIndex);
			ArrayMultiplyAddInPlace(HammerBufferView.Slice(State.CurrentIndex, NumHammerSample), State.Strength, OutAudio.Slice(0, NumHammerSample));
			State.CurrentIndex += NumHammerSample;
			if(State.CurrentIndex < HammerBufferSize)
				i++;
			else
				HammerQueue.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}

	void FPianoSynth::SynthNotesOff(TArrayView<float>& OutAudio, bool IsSusPedalOn)
	{
		if(NoteOffMap.Num() == 0)
			return;
		
		TArray<FMidiVoiceId> ToRemoveKeys;
		ToRemoveKeys.Empty(NoteOffMap.Num());
		for(auto& SynthNoteOff : NoteOffMap)
		{
			SynthNoteOff.Value->Synthesize(OutAudio, false, IsSusPedalOn, true);
			if(!SynthNoteOff.Value->IsFirstStageRunning())
				ToRemoveKeys.Emplace(SynthNoteOff.Key);
		}

		for(FMidiVoiceId RmKey : ToRemoveKeys)
			NoteOffMap.Remove(RmKey);
	}

	void FPianoSynth::SynthNoteOnWithSusPedalOn(TArrayView<float>& OutAudio)
	{
		if(NoteOnMap.Num() == 0)
			return;
		
		for(auto& SynthNoteOn : NoteOnMap)
		{
			SynthNoteOnFull(OutAudio, SynthNoteOn.Value, true);
		}
	}

	void FPianoSynth::SynthNoteOnFull(TArrayView<float>& OutAudio, TSharedPtr<FPianoKeySynth>& KeySynth, bool IsSusPedalOn)
	{
		KeySynth->Synthesize(OutAudio, true, IsSusPedalOn, true);
	}

	void FPianoSynth::SynthNoteOnWithSusPedalOff(TArrayView<float>& OutAudio, bool bIsNewNoteOnTrigger)
	{
		if(NoteOnMap.Num() == 0)
			return;

		const int32 AttackSize = AttackCurveBuffer.Num();
		const int32 OutBufferSize = OutAudio.Num() * sizeof(float);

		const int32 NumNoteOn = NoteOnMap.Num();
		for(auto& SynthNoteOn : NoteOnMap)
		{
			const int32 StartSynthIndex = SynthNoteOn.Value->GetCurrentSampleIndex();
			
			if(!SynthNoteOn.Value->HasDamper() || StartSynthIndex < AttackSize || NumNoteOn == 1 || SynthNoteOn.Value->GetSymResonScale() < 1e-5f)
			{
				// Note that this isn't entirely correct as the number of requested frames can be lager than the current remaining attack frames
				// But sympathetic resonance is more dominance at the tail of a note rather than its initial attack window (around 100ms even on the highest note) 
				// As block rate and attack window are usually very short (10ms), we can get away with this without degrading output quality.
				SynthNoteOnFull(OutAudio, SynthNoteOn.Value, false);
			}
			else
			{
				FAlignedFloatBuffer TempOnBuffer;
				TempOnBuffer.SetNumUninitialized(OutAudio.Num());
				FMemory::Memzero(TempOnBuffer.GetData(), OutBufferSize);
				TArrayView<float> TempBuffer = TArrayView<float>(TempOnBuffer);
				
				SynthNoteOn.Value->Synthesize(TempBuffer, true, false, false);
				ArrayAddInPlace(TempOnBuffer, OutAudio);
				SecondStageBuffers.Emplace(TempOnBuffer);
				SecondStageSynths.Emplace(SynthNoteOn.Value);
			}
		}

		//Synthesize second stage with SymReson
		if(SecondStageSynths.Num() > 0)
		{
			const int32 NumSymResonSynth = SecondStageBuffers.Num();
			for(int i = 0; i < NumSymResonSynth; i++)
				ArraySubtractInPlace1(OutAudio, SecondStageBuffers[i]);
		
			for(int i = 0; i < NumSymResonSynth; i++)
			{
				SecondStageSynths[i]->SynthesizeSecondStageAndSymReson(OutAudio, SecondStageBuffers[i], bIsNewNoteOnTrigger);
			}
		}

		SecondStageBuffers.Reset();
		SecondStageSynths.Reset();
	}

	float FPianoSynth::GetKeyHammerVelocity(const TSharedPtr<FPianoKeySynth>& KeySynth) const
	{
		float RealHammerVelocity = KeySynth->GetVelocity() * KeySynth->GetHammerVelScale();

		if(NoteGainCurve)
			RealHammerVelocity *= GetNoteGainValueFromCurve(KeySynth->GetMidiNote());
		
		if(HammerGainCurve)
			RealHammerVelocity *= FMath::Clamp(HammerGainCurve->GetValueByTimeInterp(KeySynth->GetMidiNote()), 0.f, 2.f);
		
		return RealHammerVelocity;
	}

	bool FPianoSynth::IsHammerSynthFinished() const
	{
		return CurrentHammerBufferIndex >= HammerBuffer.Num();
	}

	float FPianoSynth::GetNoteGainValueFromCurve(const uint8 MidiNote) const
	{
		return FMath::Clamp(NoteGainCurve->GetValueByTimeInterp(MidiNote), 0.f, 1.f);
	}
}

