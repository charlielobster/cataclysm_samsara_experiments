// Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.

#include "D3D11RHIPrivate.h"
#include "RHIStaticStates.h"

// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO

void FD3D11DynamicRHI::CreateHbaoInterface()
{
	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
	{
		check(HBAODLLHandle == 0);
		FString HBAOBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/GameWorks/GFSDK_SSAO/");
#if PLATFORM_64BITS
		FString HBAOPath(HBAOBinariesRoot + TEXT("GFSDK_SSAO_D3D11.win64.dll"));
#else
		FString HBAOPath(HBAOBinariesRoot + TEXT("GFSDK_SSAO_D3D11.win64.dll"));
#endif
		HBAODLLHandle = LoadLibraryW(*HBAOPath);
		check(HBAODLLHandle);

		GFSDK_SSAO_Status status;
		status = GFSDK_SSAO_CreateContext_D3D11(Direct3DDevice, &HBAOContext);
		check(status == GFSDK_SSAO_OK);

		GFSDK_SSAO_Version Version;
		status = GFSDK_SSAO_GetVersion(&Version);
		check(status == GFSDK_SSAO_OK);

		UE_LOG(LogD3D11RHI, Log, TEXT("HBAO+ %d.%d.%d.%d"), Version.Major, Version.Minor, Version.Branch, Version.Revision);
	}
}

void FD3D11DynamicRHI::ReleaseHbaoInterface()
{
	if (HBAOContext)
	{
		HBAOContext->Release();
		HBAOContext = NULL;
	}

	if (HBAODLLHandle)
	{
		FreeLibrary(HBAODLLHandle);
		HBAODLLHandle = 0;
	}
}

void FD3D11DynamicRHI::RHIRenderHBAO(
	const FTextureRHIParamRef SceneDepthTextureRHI,
	const FMatrix& ProjectionMatrix,
	const FTextureRHIParamRef SceneNormalTextureRHI,
	const FMatrix& ViewMatrix,
	const FTextureRHIParamRef SceneColorTextureRHI,
	const GFSDK_SSAO_Parameters& BaseParams
	)
{
	if (!HBAOContext)
	{
		return;
	}

	static const auto CVarHBAOGBufferNormals = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HBAO.GBufferNormals"));
	static const auto CVarHBAOVisualizeAO = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HBAO.VisualizeAO"));

	D3D11_VIEWPORT Viewport;
	uint32 NumViewports = 1;
	Direct3DDeviceIMContext->RSGetViewports(&NumViewports, &Viewport);

	FD3D11TextureBase* DepthTexture = GetD3D11TextureFromRHITexture(SceneDepthTextureRHI);
	ID3D11ShaderResourceView* DepthSRV = DepthTexture->GetShaderResourceView();

	FD3D11TextureBase* NewRenderTarget = GetD3D11TextureFromRHITexture(SceneColorTextureRHI);
	ID3D11RenderTargetView* RenderTargetView = NewRenderTarget->GetRenderTargetView(0, -1);

	GFSDK_SSAO_InputData_D3D11 Input;
	Input.DepthData.DepthTextureType = GFSDK_SSAO_HARDWARE_DEPTHS;
	Input.DepthData.pFullResDepthTextureSRV = DepthSRV;
	Input.DepthData.Viewport.Enable = true;
	Input.DepthData.Viewport.TopLeftX = uint32(Viewport.TopLeftX);
	Input.DepthData.Viewport.TopLeftY = uint32(Viewport.TopLeftY);
	Input.DepthData.Viewport.Width    = uint32(Viewport.Width);
	Input.DepthData.Viewport.Height   = uint32(Viewport.Height);
	Input.DepthData.ProjectionMatrix.Data = GFSDK_SSAO_Float4x4(&ProjectionMatrix.M[0][0]);
	Input.DepthData.ProjectionMatrix.Layout = GFSDK_SSAO_ROW_MAJOR_ORDER;
	Input.DepthData.MetersToViewSpaceUnits = 100.f;

	FD3D11TextureBase* NormalTexture = GetD3D11TextureFromRHITexture(SceneNormalTextureRHI);
	ID3D11ShaderResourceView* NormalSRV = NormalTexture->GetShaderResourceView();

	Input.NormalData.Enable = CVarHBAOGBufferNormals->GetValueOnRenderThread();
	Input.NormalData.pFullResNormalTextureSRV = NormalSRV;
	Input.NormalData.DecodeScale = 2.f;
	Input.NormalData.DecodeBias = -1.f;
	Input.NormalData.WorldToViewMatrix.Data = GFSDK_SSAO_Float4x4(&ViewMatrix.M[0][0]);
	Input.NormalData.WorldToViewMatrix.Layout = GFSDK_SSAO_ROW_MAJOR_ORDER;

	GFSDK_SSAO_Parameters Params;
	FMemory::Memcpy(&Params, &BaseParams, sizeof(BaseParams));

	GFSDK_SSAO_Output_D3D11 Output;
	Output.pRenderTargetView = RenderTargetView;
	Output.Blend.Mode = CVarHBAOVisualizeAO->GetValueOnRenderThread() ? GFSDK_SSAO_OVERWRITE_RGB : GFSDK_SSAO_MULTIPLY_RGB;
	Output.TwoPassBlend.Enable = false;

	GFSDK_SSAO_Status Status;
	Status = HBAOContext->RenderAO(Direct3DDeviceIMContext, Input, Params, Output);
	check(Status == GFSDK_SSAO_OK);
}

#endif
// NVCHANGE_END: Add HBAO+
