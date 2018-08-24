// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include <xaudio2.h>
#include "HideWindowsPlatformTypes.h"

// Any platform defines
namespace Audio
{
	class FMixerPlatformXAudio2;

	/**
	* FXAudio2VoiceCallback
	* XAudio2 implementation of IXAudio2VoiceCallback
	* This callback class is used to get event notifications on buffer end (when a buffer has finished processing).
	* This is used to signal the I/O thread that it can request another buffer from the user callback.
	*/
	class FXAudio2VoiceCallback : public IXAudio2VoiceCallback
	{
	public:
		FXAudio2VoiceCallback() {}
		~FXAudio2VoiceCallback() {}

	private:
		void STDCALL OnVoiceProcessingPassStart(UINT32 BytesRequired) {}
		void STDCALL OnVoiceProcessingPassEnd() {}
		void STDCALL OnStreamEnd() {}
		void STDCALL OnBufferStart(void* BufferContext) {}
		void STDCALL OnLoopEnd(void* BufferContext) {}
		void STDCALL OnVoiceError(void* BufferContext, HRESULT Error) {}

		void STDCALL OnBufferEnd(void* BufferContext);

	};

	class FMixerPlatformXAudio2 : public IAudioMixerPlatformInterface
	{

	public:

		FMixerPlatformXAudio2();
		~FMixerPlatformXAudio2();

		//~ Begin IAudioMixerPlatformInterface
		EAudioMixerPlatformApi::Type GetPlatformApi() const override { return EAudioMixerPlatformApi::XAudio2; }
		bool InitializeHardware() override;
		bool CheckAudioDeviceChange() override;
		bool TeardownHardware() override;
		bool IsInitialized() const override;
		bool GetNumOutputDevices(uint32& OutNumOutputDevices) override;
		bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) override;
		bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
		bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override;
		bool CloseAudioStream() override;
		bool StartAudioStream() override;
		bool StopAudioStream() override;
		bool MoveAudioStreamToNewAudioDevice(const FString& InNewDeviceId) override;
		FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override;
		void SubmitBuffer(const TArray<float>& Buffer) override;
		FName GetRuntimeFormat(USoundWave* InSoundWave) override;
		bool HasCompressedAudioInfoClass(USoundWave* InSoundWave) override;
		ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* InSoundWave) override;
		FString GetDefaultDeviceName() override;
		//~ End IAudioMixerPlatformInterface

		//~ Begin IAudioMixerDeviceChangedLister
		void RegisterDeviceChangedListener() override;
		void UnRegisterDeviceChangedListener() override;
		void OnDefaultCaptureDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;
		void OnDefaultRenderDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;
		void OnDeviceAdded(const FString& DeviceId) override;
		void OnDeviceRemoved(const FString& DeviceId) override;
		void OnDeviceStateChanged(const FString& DeviceId, const EAudioDeviceState InState) override;
		//~ End IAudioMixerDeviceChangedLister

	private:

		const TCHAR* GetErrorString(HRESULT Result);

	private:
		typedef TArray<long> TChannelTypeMap;

		// Bool indicating that the default audio device changed
		// And that we need to restart the audio device.
		FThreadSafeBool bDeviceChanged;

		TChannelTypeMap ChannelTypeMap;
		IXAudio2* XAudio2System;
		IXAudio2MasteringVoice* OutputAudioStreamMasteringVoice;
		IXAudio2SourceVoice* OutputAudioStreamSourceVoice;
		FXAudio2VoiceCallback OutputVoiceCallback;
		FString OriginalAudioDeviceId;
		FString NewAudioDeviceId;
		FThreadSafeBool bMoveAudioStreamToNewAudioDevice;
		uint32 bIsComInitialized : 1;
		uint32 bIsInitialized : 1;
		uint32 bIsDeviceOpen : 1;

	};

}

