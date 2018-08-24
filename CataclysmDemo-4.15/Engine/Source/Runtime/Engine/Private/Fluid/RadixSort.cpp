// CATACLYSM 
#include "RadixSort.h"
#include "EnginePrivatePCH.h"
#include "FluidSimulation.h"
#include "FluidGlobalShaders.h"

DEFINE_STAT(STAT_FluidRadixSort);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FRadixSortUniformParameters, TEXT("RadixSortInfo"));

IMPLEMENT_SHADER_TYPE(template<>, FRadixSortCS<0>, TEXT("FluidRadixSort"), TEXT("RadixSortBlock"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRadixSortCS<1>, TEXT("FluidRadixSort"), TEXT("StreamCountKernel"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRadixSortCS<2>, TEXT("FluidRadixSort"), TEXT("PrefixScanKernel"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRadixSortCS<3>, TEXT("FluidRadixSort"), TEXT("SortAndScatterKeyValueKernel"), SF_Compute);


FRadixSort::FRadixSort(uint32 InMaxElemCount)
{
	MaxElemCount = InMaxElemCount;
	DispatchGridDim = 32;

	for (uint32 i = 0; i < 2; ++i)
	{
		Keys[i].Initialize(sizeof(uint32), MaxElemCount);
		Values[i].Initialize(sizeof(uint32), MaxElemCount);
	}
	Histogram.Initialize(sizeof(uint32), 16 * DispatchGridDim);
}

FRadixSort::~FRadixSort()
{
}

void FRadixSort::Destroy()
{
	for (uint32 i = 0; i < 2; ++i)
	{
		Keys[i].Release();
		Values[i].Release();
	}
	Histogram.Release();

	delete this;
}

void FRadixSort::Sort(FRHICommandListImmediate& RHICmdList, uint32 ElemCount, uint32 StartBit, uint32 EndBit,
	const FRWBufferStructured* &SortedKeys, const FRWBufferStructured* &SortedValues)
{
	SCOPE_CYCLE_COUNTER(STAT_FluidRadixSort);
	SCOPED_DRAW_EVENTF(RHICmdList, FluidRadixSort, TEXT("FluidRadixSort_%d"), ElemCount);

	FRadixSortUniformParameters RadixSortParams;
	RadixSortParams.ElemCount = ElemCount;
	RadixSortParams.GridDim = DispatchGridDim;

	uint32 BufferIndex = 0;
	if (ElemCount <= 16 * 32 * 4)
	{
		TShaderMapRef<FRadixSortCS<0> > RadixSortCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		RadixSortParams.StartBit = StartBit;
		RadixSortParams.EndBit = EndBit;
		FLocalUniformBuffer LocalUB = TUniformBufferRef<FRadixSortUniformParameters>::CreateLocalUniformBuffer(RHICmdList, RadixSortParams, UniformBuffer_SingleFrame);

		RHICmdList.SetComputeShader(RadixSortCS->GetComputeShader());

		RadixSortCS->SetParameters(RHICmdList, Keys[BufferIndex].SRV, Values[BufferIndex].SRV, FShaderResourceViewRHIParamRef(), LocalUB);
		RadixSortCS->SetOutput(RHICmdList, Keys[BufferIndex ^ 1].UAV, Values[BufferIndex ^ 1].UAV, FUnorderedAccessViewRHIParamRef());

		DispatchComputeShader(RHICmdList, *RadixSortCS, 1, 1, 1);
		RadixSortCS->UnbindBuffers(RHICmdList);

		BufferIndex ^= 1;
	}
	else
	{
		TShaderMapRef<FRadixSortCS<1> > RadixSortCS1(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FRadixSortCS<2> > RadixSortCS2(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FRadixSortCS<3> > RadixSortCS3(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		for (uint32 Bit = StartBit; Bit < EndBit; Bit += 4)
		{
			RadixSortParams.StartBit = Bit;
			RadixSortParams.EndBit = FMath::Min(Bit + 4, EndBit);
			FLocalUniformBuffer LocalUB = TUniformBufferRef<FRadixSortUniformParameters>::CreateLocalUniformBuffer(RHICmdList, RadixSortParams, UniformBuffer_SingleFrame);

			//pass 1
			RHICmdList.SetComputeShader(RadixSortCS1->GetComputeShader());

			RadixSortCS1->SetParameters(RHICmdList, Keys[BufferIndex].SRV, Values[BufferIndex].SRV, FShaderResourceViewRHIParamRef(), LocalUB);
			RadixSortCS1->SetOutput(RHICmdList, FUnorderedAccessViewRHIParamRef(), FUnorderedAccessViewRHIParamRef(), Histogram.UAV);

			DispatchComputeShader(RHICmdList, *RadixSortCS1, DispatchGridDim, 1, 1);
			RadixSortCS1->UnbindBuffers(RHICmdList);

			//pass 2
			RHICmdList.SetComputeShader(RadixSortCS2->GetComputeShader());

			RadixSortCS2->SetParameters(RHICmdList, FShaderResourceViewRHIParamRef(), FShaderResourceViewRHIParamRef(), FShaderResourceViewRHIParamRef(), LocalUB);
			RadixSortCS2->SetOutput(RHICmdList, FUnorderedAccessViewRHIParamRef(), FUnorderedAccessViewRHIParamRef(), Histogram.UAV);

			DispatchComputeShader(RHICmdList, *RadixSortCS2, 1, 1, 1);
			RadixSortCS2->UnbindBuffers(RHICmdList);

			//pass 3
			RHICmdList.SetComputeShader(RadixSortCS3->GetComputeShader());

			RadixSortCS3->SetParameters(RHICmdList, Keys[BufferIndex].SRV, Values[BufferIndex].SRV, Histogram.SRV, LocalUB);
			RadixSortCS3->SetOutput(RHICmdList, Keys[BufferIndex ^ 1].UAV, Values[BufferIndex ^ 1].UAV, FUnorderedAccessViewRHIParamRef());

			DispatchComputeShader(RHICmdList, *RadixSortCS3, DispatchGridDim, 1, 1);
			RadixSortCS3->UnbindBuffers(RHICmdList);

			BufferIndex ^= 1;
		}
	}

	SortedKeys = &Keys[BufferIndex];
	SortedValues = &Values[BufferIndex];
}

void FRadixSort::Test(FRHICommandListImmediate& RHICmdList)
{
	FRHIResourceCreateInfo CreateInfo;
	FStructuredBufferRHIRef KeysStagingBuffer = RHICreateStructuredBuffer(sizeof(uint32), sizeof(uint32) * MaxElemCount, BUF_Staging, CreateInfo);
	FStructuredBufferRHIRef ValuesStagingBuffer = RHICreateStructuredBuffer(sizeof(uint32), sizeof(uint32) * MaxElemCount, BUF_Staging, CreateInfo);

	TArray<uint32> InKeys;
	InKeys.SetNumUninitialized(MaxElemCount);
	TArray<uint32> ValueCounts;
	ValueCounts.SetNumUninitialized(MaxElemCount);

	for (uint32 i = 0; i < MaxElemCount; ++i)
	{
		InKeys[i] = FMath::RandHelper(INT_MAX);
	}

	for (uint32 ElemCount = 2048; ElemCount <= MaxElemCount; ElemCount *= 8)
	{
		uint32* InKeysData = (uint32*)RHILockStructuredBuffer(Keys[0].Buffer, 0, sizeof(uint32) * ElemCount, RLM_WriteOnly);
		uint32* InValuesData = (uint32*)RHILockStructuredBuffer(Values[0].Buffer, 0, sizeof(uint32) * ElemCount, RLM_WriteOnly);
		for (uint32 i = 0; i < ElemCount; ++i)
		{
			InKeysData[i] = InKeys[i];
			InValuesData[i] = i;
		}
		RHIUnlockStructuredBuffer(Values[0].Buffer);
		RHIUnlockStructuredBuffer(Keys[0].Buffer);

		const FRWBufferStructured* SortedKeys;
		const FRWBufferStructured* SortedValues;
		Sort(RHICmdList, ElemCount, 0, 32, SortedKeys, SortedValues);

		RHICopyStructuredBuffer(SortedKeys->Buffer, KeysStagingBuffer);
		RHICopyStructuredBuffer(SortedValues->Buffer, ValuesStagingBuffer);

		//read
		uint32* OutKeysData = (uint32*)RHILockStructuredBuffer(KeysStagingBuffer, 0, sizeof(uint32) * ElemCount, RLM_ReadOnly);
		uint32* OutValuesData = (uint32*)RHILockStructuredBuffer(ValuesStagingBuffer, 0, sizeof(uint32) * ElemCount, RLM_ReadOnly);

		for (uint32 i = 0; i < ElemCount; ++i)
		{
			ValueCounts[i] = 0;
		}
		for (uint32 i = 0; i < ElemCount; ++i)
		{
			uint32 Value = OutValuesData[i];
			check(Value < ElemCount);
			check(InKeys[Value] == OutKeysData[i]);
			ValueCounts[Value] += 1;

			if (i > 0)
			{
				check(OutKeysData[i] >= OutKeysData[i - 1]);
			}
		}
		for (uint32 i = 0; i < ElemCount; ++i)
		{
			check(ValueCounts[i] == 1);
		}

		RHIUnlockStructuredBuffer(ValuesStagingBuffer);
		RHIUnlockStructuredBuffer(KeysStagingBuffer);

		UE_LOG(CataclysmInfo, Log, TEXT("Tested RadixSort %d"), ElemCount);
	}

	ValuesStagingBuffer.SafeRelease();
	KeysStagingBuffer.SafeRelease();
}
