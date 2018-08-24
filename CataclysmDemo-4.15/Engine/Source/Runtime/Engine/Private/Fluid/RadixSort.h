#pragma once
// CATACLYSM 
#include "FluidSimulationCommon.h"

DECLARE_CYCLE_STAT_EXTERN(TEXT("Radix Sort"), STAT_FluidRadixSort, STATGROUP_Fluid, );

class FRadixSort
{
public:
	FRadixSort(uint32 InMaxElemCount);
	~FRadixSort();

	void Destroy();

	void GetInputKeysAndValues(const FRWBufferStructured* &OutKeys, const FRWBufferStructured* &OutValues) const
	{
		OutKeys = &Keys[0];
		OutValues = &Values[0];
	}

	void Sort(FRHICommandListImmediate& RHICmdList, uint32 ElemCount, uint32 StartBit, uint32 EndBit, 
		const FRWBufferStructured* &SortedKeys, const FRWBufferStructured* &SortedValues);

	void Test(FRHICommandListImmediate& RHICmdList);

private:
	uint32 MaxElemCount;
	uint32 DispatchGridDim;

	FRWBufferStructured Keys[2];
	FRWBufferStructured Values[2];

	FRWBufferStructured Histogram;
};
