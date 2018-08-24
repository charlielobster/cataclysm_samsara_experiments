// CATACLYSM 
#pragma once

class FCountAppendBuffer
{
public:
		
	FCountAppendBuffer(uint32 numElements, uint32 byteStride);
	~FCountAppendBuffer();

	void Destroy();

	void CopyCount();
	uint32 FetchCount();

	FUnorderedAccessViewRHIRef GetUAV() { return AppendBuffer->UAV; }
	FShaderResourceViewRHIRef GetSRV() { return AppendBuffer->SRV; }

	uint32	Count;

private:

	FRWBufferStructured*		AppendBuffer;
	FStructuredBufferRHIRef		CountStagingBuffer;

	bool DataIsReady;
};
