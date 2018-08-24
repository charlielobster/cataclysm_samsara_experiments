// CATACLYSM 
#include "IndirectAppendBuffer.h"
#include "EnginePrivatePCH.h"

FIndirectAppendBuffer::FIndirectAppendBuffer(uint32 numElements, uint32 byteStride)// , uint8 SRVFormat) PF_R16G16_UINT Nice to have this for SRV
	: Count(0)
	, CurrentDispatchGroupCount(-1)
	, DataIsReady(false)
	, AppendBuffer(nullptr)
	, MaxNumElements(numElements)
{
	AppendBuffer = new FRWBufferStructured();
	AppendBuffer->Initialize(byteStride, numElements, 0, false, true);

	DispatchParameters = new FRWBuffer();
	DispatchParameters->Initialize(sizeof(uint32), 3, PF_R32_UINT,  BUF_DrawIndirect);

	CountBuffer = new FRWBufferStructured();
	CountBuffer->Initialize(sizeof(uint32), 1, 0);

	FRHIResourceCreateInfo CreateInfo;
	CountStagingBuffer = RHICreateStructuredBuffer(sizeof(uint32), sizeof(uint32),BUF_Staging, CreateInfo);
}

FIndirectAppendBuffer::~FIndirectAppendBuffer()
{
}

void FIndirectAppendBuffer::Destroy()
{
	if (AppendBuffer)
	{
		AppendBuffer->Release();
		AppendBuffer = nullptr;
	}
	if (DispatchParameters)
	{
		DispatchParameters->Release();
		DispatchParameters = nullptr;
	}
	if (CountBuffer)
	{
		CountBuffer->Release();
		CountBuffer = nullptr;
	}
	CountStagingBuffer.SafeRelease();

	delete this;
}

void FIndirectAppendBuffer::CopyCount()
{
	// REally not sure why we can't just RHICopyStructureCount(AppendBuffer->UAV, CountBuffer->Buffer);
	RHICopyStructureCount(AppendBuffer->UAV, CountStagingBuffer);
	RHICopyStructuredBuffer(CountStagingBuffer, CountBuffer->Buffer);
	DataIsReady = true;
}

uint32 FIndirectAppendBuffer::FetchCount()
{
	if (DataIsReady)
	{
		uint32* pCount = (uint32*)RHILockStructuredBuffer(CountStagingBuffer, 0, sizeof(uint32), RLM_ReadOnly);
		Count = *pCount;
		RHIUnlockStructuredBuffer(CountStagingBuffer);
	}
	check(Count < MaxNumElements);
	return Count;
}