// CATACLYSM 
#pragma once

class FReadbackBuffer
{
public:
		
	FReadbackBuffer(uint32 numElements, uint32 byteStride);
	~FReadbackBuffer();

	void Destroy();

	void CopyData();
	bool FetchData(void*& pData, uint32& countOut);
	void ThreadSafeFetchData();
	bool GetData(void*& pData, uint32& countOut);
	void Clear() {m_CountOut = 0;}

	FUnorderedAccessViewRHIRef GetUAV() { return m_pActiveBuffer->UAV; }
	FShaderResourceViewRHIRef GetSRV() { return m_pActiveBuffer->SRV; }

	mutable FCriticalSection    CriticalSection;

private:
	FRWBufferStructured*		m_pActiveBuffer;
	FStructuredBufferRHIRef		m_pStagingBuffer;
	FStructuredBufferRHIRef		m_pCountStagingBuffer;

	uint8*	m_pCpuData;
	uint32	m_CountOut;
	uint32	m_numElements;
	uint32	m_byteStride;

	bool DataIsReady;
};
