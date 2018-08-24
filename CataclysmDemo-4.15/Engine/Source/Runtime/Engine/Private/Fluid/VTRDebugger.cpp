// CATACLYSM 
#include "VTRDebugger.h"
#include "EnginePrivatePCH.h"
#include "RHIStaticStates.h"
#include "SparseGrid.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "CountAppendBuffer.h"
#include "HighResScreenshot.h"
#include "ReadbackBuffer.h"
#include "FluidSimulation.h"
#include "FluidGlobalShaders.h"
#include "GlobalDistanceFieldParameters.h"
#include "../Particles/ParticleSimulationGPU.h"

#define OUTPUT_AS_PNG 0

IMPLEMENT_SHADER_TYPE(template<>, FGetBricksDataCS<0>, TEXT("FluidGetBricksDataShader"), TEXT("GetBricksData"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGetBricksDataCS<FSparseGrid::ETexDataType::TDT_FLOAT>, TEXT("FluidGetBricksDataShader"), TEXT("GetBricksData"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGetBricksDataCS<FSparseGrid::ETexDataType::TDT_FLOAT2>, TEXT("FluidGetBricksDataShader"), TEXT("GetBricksData"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGetBricksDataCS<FSparseGrid::ETexDataType::TDT_FLOAT4>, TEXT("FluidGetBricksDataShader"), TEXT("GetBricksData"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGetBricksDataCS<FSparseGrid::ETexDataType::TDT_UINT>, TEXT("FluidGetBricksDataShader"), TEXT("GetBricksData"), SF_Compute);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FBrickDataDebuggerUniformParameters, TEXT("BrickDebuggerInfo"));
typedef TUniformBufferRef<FBrickDataDebuggerUniformParameters> FBrickDataDebuggerUniformBufferRef;

FVTRDebugger::FVTRDebugger()
{
}

FVTRDebugger::~FVTRDebugger()
{
}

void FVTRDebugger::generateOutputTxt(
	TArray<FFloat16Color>& data, 
	FString VTRName,
	uint32 sliceIndex, 
	EPixelFormat Format, 
	uint32 Width, 
	uint32 Height, 
	FString& outString)
{
	outString.Append("====== Header ====== \n");	

	TArray<FStringFormatArg> headerArgs;
	FStringFormatArg vtrNameArg(VTRName);
	FStringFormatArg sliceIndexArg(sliceIndex);
	FStringFormatArg formatArg(GPixelFormats[Format].Name);
	FStringFormatArg widthArg(Width);
	FStringFormatArg heightArg(Height);
	headerArgs.Add(vtrNameArg);
	headerArgs.Add(sliceIndexArg);
	headerArgs.Add(formatArg);
	headerArgs.Add(widthArg);
	headerArgs.Add(heightArg);
	outString.Append(FString::Format(TEXT("VTR Name        : {0} \nVTR Slice Index : {1} \nPixel Format    : {2} \nTexture Width   : {3}\nTexture Height  : {4}\n"), headerArgs));

	outString.Append("\n");

	outString.Append("====== Data ====== \n");
	TArray<FStringFormatArg> args;
	for (int32 i = 0; i < data.Num(); i++)
	{
		FStringFormatArg arg_r(float(data[i].R));
		args.Add(arg_r);
		FStringFormatArg arg_g(float(data[i].G));
		args.Add(arg_g);
		FStringFormatArg arg_b(float(data[i].B));
		args.Add(arg_b);
		FStringFormatArg arg_a(float(data[i].A));
		args.Add(arg_a);

		if (0 != i && i % Width == 0)
		{
			outString.Append("\n");
		}

		outString.Append(FString::Format(TEXT("{0},{1},{2},{3}  "), args));
	}
}

void FVTRDebugger::GetBrickDataFromGPU(FRHICommandListImmediate& RHICmdList, 
	const TArray<FBrickIndex> &Bricks, 
	FSparseGrid::TypedVTRStructured* VTR, 
	uint32 idx, 
	TArray<float>& OutBrickDatas)
{
	// Create buffer with bricks' index list
	FBrickResourceArrayWrapper bulkData((void*)Bricks.GetData(), Bricks.Num() * sizeof(FBrickIndex));
	FRHIResourceCreateInfo CreateInfo(&bulkData);
	FStructuredBufferRHIRef pBrickListBuffer = RHICreateStructuredBuffer(sizeof(FBrickIndex), sizeof(FBrickIndex) * Bricks.Num(), BUF_ShaderResource, CreateInfo);
	FShaderResourceViewRHIRef pBrickListBufferSRV = RHICreateShaderResourceView(pBrickListBuffer);

	// Set up the uniform buffer.
	FBrickDataDebuggerUniformParameters BrickDebuggerParameters;
	FBrickDataDebuggerUniformBufferRef BrickDebuggerUniformBuffer;

	// Create the uniform buffer.
	BrickDebuggerParameters.IndexOfComponent = idx;
	BrickDebuggerParameters.BricksNum = Bricks.Num();
	BrickDebuggerUniformBuffer = FBrickDataDebuggerUniformBufferRef::CreateUniformBufferImmediate(BrickDebuggerParameters, UniformBuffer_SingleDraw);

	// Create buffer to recieve output data
	uint32 numOutputData = Bricks.Num() * BRICK_SIZE_X * BRICK_SIZE_Y * BRICK_SIZE_Z;
	FReadbackBuffer* OutputDataFromGPU = new FReadbackBuffer(numOutputData, sizeof(float) * 2);

	// Do computer shader
#define GET_BRICKS_DATA(TDT) {\
	TShaderMapRef<FGetBricksDataCS<TDT>> GetBricksDataCS(GetGlobalShaderMap(GMaxRHIFeatureLevel)); \
	RHICmdList.SetComputeShader(GetBricksDataCS->GetComputeShader());\
	GetBricksDataCS->SetOutput(RHICmdList, OutputDataFromGPU->GetUAV());\
	GetBricksDataCS->SetParameters(RHICmdList,BrickDebuggerUniformBuffer,pBrickListBufferSRV,VTR->Texture3D);\
	\
	const uint32 GroupCount = FMath::Max<uint32>(1, (Bricks.Num() + GET_BRICK_DATA_THREAD_COUNT - 1) / GET_BRICK_DATA_THREAD_COUNT);\
	DispatchComputeShader(\
		RHICmdList,\
		*GetBricksDataCS,\
		GroupCount,\
		1,\
		1);\
	GetBricksDataCS->UnbindBuffers(RHICmdList);}

	if (VTR->TDT == FSparseGrid::TDT_FLOAT) GET_BRICKS_DATA(FSparseGrid::TDT_FLOAT)
	else if (VTR->TDT == FSparseGrid::TDT_FLOAT2) GET_BRICKS_DATA(FSparseGrid::TDT_FLOAT2)
	else if (VTR->TDT == FSparseGrid::TDT_FLOAT4) GET_BRICKS_DATA(FSparseGrid::TDT_FLOAT4)
	else if (VTR->TDT == FSparseGrid::TDT_UINT) GET_BRICKS_DATA(FSparseGrid::TDT_UINT)
	else check(0 && "NEED TO ADD A TDT HERE!");
#undef GET_BRICKS_DATA

	OutputDataFromGPU->CopyData();

	uint32 BrickListNum = 0;
	void* BrickListData = nullptr;
	OutputDataFromGPU->FetchData(BrickListData, BrickListNum);

	// Sort Output Datas
	TMap<uint32, TArray<float>> SortedDatas;
	for (uint32 index = 0; index < BrickListNum; index ++)
	{
		uint32 brickId = ((float*)BrickListData)[2*index];
		float data = ((float*)BrickListData)[2 * index + 1];		

		if (SortedDatas.Contains(brickId))
		{
			SortedDatas[brickId].Add(data);
		}
		else
		{
			TArray<float>& arrayData = SortedDatas.Add(brickId);
			arrayData.Add(data);
		}
	}

	// bricks need to be sorted by the input index order, not the numeric order of the brick address.
	for (int i = 0; i < Bricks.Num(); ++i)
	{
		uint32 bidx = Bricks[i].Index;
		if (SortedDatas.Contains(bidx))
		{
			const TArray<float>& brickData = SortedDatas[bidx];
			for (int j = 0; j < brickData.Num(); ++j)
			{
				OutBrickDatas.Add(brickData[j]);
			}
		}
		else
		{
			check(0);
		}
	}

	// Release resources
	if(BrickDebuggerUniformBuffer)
		BrickDebuggerUniformBuffer.SafeRelease();

	if(pBrickListBufferSRV) 
		pBrickListBufferSRV.SafeRelease();
	if(pBrickListBuffer)
		pBrickListBuffer.SafeRelease();

	if (OutputDataFromGPU)
	{
		OutputDataFromGPU->Destroy();
		OutputDataFromGPU = nullptr;
	}	
}


void FVTRDebugger::GetLSBrickDataFromGPU(FRHICommandListImmediate& RHICmdList,
	const TArray<FBrickIndex> &Bricks,
	FVTRStructured<USE_FAKE_VTR>* LS,
	TArray<float>& OutBrickDatas)
{
	// Create buffer with bricks' index list
	FBrickResourceArrayWrapper bulkData((void*)Bricks.GetData(), Bricks.Num() * sizeof(FBrickIndex));
	FRHIResourceCreateInfo CreateInfo(&bulkData);
	FStructuredBufferRHIRef pBrickListBuffer = RHICreateStructuredBuffer(sizeof(FBrickIndex), sizeof(FBrickIndex) * Bricks.Num(), BUF_ShaderResource, CreateInfo);
	FShaderResourceViewRHIRef pBrickListBufferSRV = RHICreateShaderResourceView(pBrickListBuffer);

	// Set up the uniform buffer.
	FBrickDataDebuggerUniformParameters BrickDebuggerParameters;
	FBrickDataDebuggerUniformBufferRef BrickDebuggerUniformBuffer;

	// Create the uniform buffer.
	BrickDebuggerParameters.IndexOfComponent = 0;
	BrickDebuggerParameters.BricksNum = Bricks.Num();
	BrickDebuggerUniformBuffer = FBrickDataDebuggerUniformBufferRef::CreateUniformBufferImmediate(BrickDebuggerParameters, UniformBuffer_SingleDraw);

	// Create buffer to receive output data
	uint32 numOutputData = Bricks.Num() * LS_BRICK_SIZE_X * LS_BRICK_SIZE_Y * LS_BRICK_SIZE_Z;
	FReadbackBuffer* OutputDataFromGPU = new FReadbackBuffer(numOutputData, sizeof(float) * 2);

	// Do computer shader
	TShaderMapRef<FGetBricksDataCS<0>> GetBricksDataCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(GetBricksDataCS->GetComputeShader());
	GetBricksDataCS->SetOutput(RHICmdList, OutputDataFromGPU->GetUAV());
	GetBricksDataCS->SetParameters(RHICmdList,BrickDebuggerUniformBuffer,pBrickListBufferSRV,LS->Texture3D);

	const uint32 GroupCount = FMath::Max<uint32>(1, (Bricks.Num() + GET_BRICK_DATA_THREAD_COUNT - 1) / GET_BRICK_DATA_THREAD_COUNT);
	DispatchComputeShader(
		RHICmdList,
		*GetBricksDataCS,
		GroupCount,
		1,
		1);
	GetBricksDataCS->UnbindBuffers(RHICmdList);

	OutputDataFromGPU->CopyData();

	uint32 BrickListNum = 0;
	void* BrickListData = nullptr;
	OutputDataFromGPU->FetchData(BrickListData, BrickListNum);

	// Sort Output Data
	TMap<uint32, TArray<float>> SortedDatas;
	for (uint32 index = 0; index < BrickListNum; index++)
	{
		uint32 brickId = ((float*)BrickListData)[2 * index];
		float data = ((float*)BrickListData)[2 * index + 1];

		if (SortedDatas.Contains(brickId))
		{
			SortedDatas[brickId].Add(data);
		}
		else
		{
			TArray<float>& arrayData = SortedDatas.Add(brickId);
			arrayData.Add(data);
		}
	}

	// bricks need to be sorted by the input index order, not the numeric order of the brick address.
	for (int i = 0; i < Bricks.Num(); ++i)
	{
		uint32 bidx = Bricks[i].Index;
		if (SortedDatas.Contains(bidx))
		{
			const TArray<float>& brickData = SortedDatas[bidx];
			for (int j = 0; j < brickData.Num(); ++j)
			{
				OutBrickDatas.Add(brickData[j]);
			}
		}
		else
		{
			check(0);
		}
	}

	// Release resources
	if (BrickDebuggerUniformBuffer)
		BrickDebuggerUniformBuffer.SafeRelease();

	if (pBrickListBufferSRV)
		pBrickListBufferSRV.SafeRelease();
	if (pBrickListBuffer)
		pBrickListBuffer.SafeRelease();

	if (OutputDataFromGPU)
	{
		OutputDataFromGPU->Destroy();
		OutputDataFromGPU = nullptr;
	}
}

namespace FluidConsoleVariables
{
	int32 OutputDebugData = 0;
	FAutoConsoleVariableRef CVarOutputDebugData(
		TEXT("Fluid.OutputDebugData"),
		OutputDebugData,
		TEXT("Output one frame of data for whatever debug points are set, and reset the OutputDebugData flag to 0.\n"),
		ECVF_Cheat
		);
}