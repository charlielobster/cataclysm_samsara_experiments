// CATACLYSM 
#pragma once

class FIndirectAppendBuffer
{
public:
		
	FIndirectAppendBuffer(uint32 numElements, uint32 byteStride);
	~FIndirectAppendBuffer();

	void Destroy();

	void CopyCount();
	uint32 FetchCount();

	FUnorderedAccessViewRHIRef GetUAV() { DataIsReady = false; CurrentDispatchGroupCount = -1; return AppendBuffer->UAV; }
	FShaderResourceViewRHIRef GetSRV() { return AppendBuffer->SRV; }

	uint32	Count;

	FRWBuffer* DispatchParameters;
	FRWBufferStructured* CountBuffer;
	// Really not sure why copying to a stagging buffer is necessary, but I can't get it to
	// get the count passed in correctly if I don't copy it after getting count in a stagging.
	FStructuredBufferRHIRef		CountStagingBuffer;

	int32 CurrentDispatchGroupCount; // Set this in the function that writes to DispatchParameters if you want to avoid extra work of doing the same dispatch size twice.

	bool DataIsReady;
private:

	FRWBufferStructured*		AppendBuffer;
	uint32 MaxNumElements;
};
