// CATACLYSM 
#include "ReadbackBuffer.h"
#include "EnginePrivatePCH.h"
#include "FluidSimulationCommon.h"
#include "FluidSimulationCommon.h"

FReadbackBuffer::FReadbackBuffer(uint32 numElements, uint32 byteStride)
	: m_numElements(numElements)
	, m_byteStride(byteStride)
    , m_pActiveBuffer(nullptr)
	, DataIsReady(false)
{
	m_pCpuData = (uint8*)FMemory::Malloc(numElements * byteStride);
	m_CountOut = 0;

	m_pActiveBuffer = new FRWBufferStructured();
	m_pActiveBuffer->Initialize(m_byteStride, m_numElements, 0, false, true);

 	FRHIResourceCreateInfo CreateInfo;
    m_pStagingBuffer = RHICreateStructuredBuffer(m_byteStride, m_byteStride * m_numElements, BUF_Staging, CreateInfo);
    m_pCountStagingBuffer = RHICreateStructuredBuffer(sizeof(uint32), sizeof(uint32), BUF_Staging, CreateInfo);
}

FReadbackBuffer::~FReadbackBuffer()
{
	FMemory::Free(m_pCpuData);
}

void FReadbackBuffer::Destroy()
{

	if (m_pActiveBuffer)
	{
		m_pActiveBuffer->Release();
		m_pActiveBuffer = NULL;
	}
    m_pStagingBuffer.SafeRelease();
	m_pCountStagingBuffer.SafeRelease();

	delete this;
}

void FReadbackBuffer::CopyData()
{
	RHICopyStructuredBuffer(m_pActiveBuffer->Buffer, m_pStagingBuffer);
	RHICopyStructureCount(m_pActiveBuffer->UAV, m_pCountStagingBuffer);
	DataIsReady = true;
}

bool FReadbackBuffer::FetchData(void*& pData, uint32& countOut)
{
	m_CountOut = countOut = 0;
	if (DataIsReady)
	{
		DataIsReady = false;
		pData = m_pCpuData;

		// Get count first
		{
			uint32* Count = (uint32*)RHILockStructuredBuffer(m_pCountStagingBuffer, 0, sizeof(uint32), RLM_ReadOnly);
			m_CountOut = countOut = *Count;
			RHIUnlockStructuredBuffer(m_pCountStagingBuffer);
			if (countOut > m_numElements)
			{
				UE_LOG(CataclysmErrors, Error, TEXT("Readback buffer maximum passed: %d of %d"), countOut, m_numElements);
				pData = nullptr;
				return false;
			}
		}

		// Then get data, use count to fetch right amount of data.
		if (countOut)
		{
			uint8* Data = (uint8*)RHILockStructuredBuffer(m_pStagingBuffer, 0, m_byteStride * countOut, RLM_ReadOnly);
			FMemory::Memcpy(pData, Data, m_byteStride * countOut);
			RHIUnlockStructuredBuffer(m_pStagingBuffer);
			return true;
		}
	}
	return false;
}

bool FReadbackBuffer::GetData(void*& pData, uint32& countOut)
{
	countOut = m_CountOut;
	if (countOut)
	{
		pData = m_pCpuData;
		return true;
	}

	return false;
}

void FReadbackBuffer::ThreadSafeFetchData()
{
	uint32 OutCount = 0;
	if (DataIsReady)
	{
		DataIsReady = false;
		// Get count first
		{
			uint32* Count = (uint32*)RHILockStructuredBuffer(m_pCountStagingBuffer, 0, sizeof(uint32), RLM_ReadOnly);
			OutCount = *Count;
			RHIUnlockStructuredBuffer(m_pCountStagingBuffer);
			if (OutCount > m_numElements)
			{
				OutCount = m_numElements;
			}
		}

		// Then get data, use count to fetch right amount of data.
		if (OutCount)
		{
			uint8* Data = (uint8*)RHILockStructuredBuffer(m_pStagingBuffer, 0, m_byteStride * OutCount, RLM_ReadOnly);
			{
				FScopeLock ScopeLock(&CriticalSection);
				m_CountOut = OutCount;
				FMemory::Memcpy(m_pCpuData, Data, m_byteStride * OutCount);
			}
			RHIUnlockStructuredBuffer(m_pStagingBuffer);
		}
		else
		{
			FScopeLock ScopeLock(&CriticalSection);
			m_CountOut = 0;
		}
	}
}