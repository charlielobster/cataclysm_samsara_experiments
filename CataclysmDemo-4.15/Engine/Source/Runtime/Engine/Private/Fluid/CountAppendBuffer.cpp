// CATACLYSM 
#include "CountAppendBuffer.h"
#include "EnginePrivatePCH.h"

FCountAppendBuffer::FCountAppendBuffer(uint32 numElements, uint32 byteStride)// , uint8 SRVFormat) PF_R16G16_UINT Nice to have this for SRV
	: Count(0)
	, AppendBuffer(nullptr)
	, DataIsReady(false)
{
	AppendBuffer = new FRWBufferStructured();
	AppendBuffer->Initialize(byteStride, numElements, 0, false, true);

 	FRHIResourceCreateInfo CreateInfo;
    CountStagingBuffer = RHICreateStructuredBuffer(sizeof(uint32), sizeof(uint32), BUF_Staging, CreateInfo);
}

FCountAppendBuffer::~FCountAppendBuffer()
{
}

void FCountAppendBuffer::Destroy()
{
	if (AppendBuffer)
	{
		AppendBuffer->Release();
		AppendBuffer = nullptr;
	}
	CountStagingBuffer.SafeRelease();

	delete this;
}

void FCountAppendBuffer::CopyCount()
{
	RHICopyStructureCount(AppendBuffer->UAV, CountStagingBuffer);
	DataIsReady = true;
}

uint32 FCountAppendBuffer::FetchCount()
{
	if (DataIsReady)
	{
		DataIsReady = false;
		uint32* pCount = (uint32*)RHILockStructuredBuffer(CountStagingBuffer, 0, sizeof(uint32), RLM_ReadOnly);
		Count = *pCount;
		RHIUnlockStructuredBuffer(CountStagingBuffer);
	}
	return Count;
}