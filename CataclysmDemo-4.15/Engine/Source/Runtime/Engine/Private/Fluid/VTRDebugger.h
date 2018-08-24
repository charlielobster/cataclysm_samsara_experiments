#pragma once
// CATACLYSM 

#include "FluidSimulationCommon.h"
#include "SparseGrid.h"

namespace FluidConsoleVariables
{
	extern int32 OutputDebugData;
}

class FVTRDebugger
{
public:

	FVTRDebugger();
	~FVTRDebugger();

private:

	static void generateOutputTxt(TArray<FFloat16Color>& data, 
		FString	VTRName, 
		uint32 sliceIndex, 
		EPixelFormat Format, 
		uint32 Width, 
		uint32 Height, 
		FString& outString);
public:

	static void GetBrickDataFromGPU(FRHICommandListImmediate& RHICmdList, 
		const TArray<FBrickIndex> &Bricks, 
		FSparseGrid::TypedVTRStructured* VTR, 
		uint32 idx,
		TArray<float>& OutBrickDatas);

	static void GetLSBrickDataFromGPU(FRHICommandListImmediate& RHICmdList,
		const TArray<FBrickIndex> &Bricks,
		FVTRStructured<USE_FAKE_VTR>* LS,
		TArray<float>& OutBrickDatas);
};

/**
* Bulk data interface for initializing a brick list buffer.
*/
class FBrickResourceArrayWrapper : public FResourceArrayInterface
{
public:
	FBrickResourceArrayWrapper(void* InData, uint32 InSize)
		: Data(InData)
		, Size(InSize)
	{
	}

	virtual const void* GetResourceData() const override { return Data; }
	virtual uint32 GetResourceDataSize() const override { return Size; }
	virtual void Discard() override { }
	virtual bool IsStatic() const override { return false; }
	virtual bool GetAllowCPUAccess() const override { return false; }
	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }

private:
	void* Data;
	uint32 Size;
};