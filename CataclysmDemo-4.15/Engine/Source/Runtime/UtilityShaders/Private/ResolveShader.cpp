// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ResolveShader.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_SHADER_TYPE(, FResolveDepthPS, TEXT("ResolvePixelShader"), TEXT("MainDepth"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FResolveDepthNonMSPS, TEXT("ResolvePixelShader"), TEXT("MainDepthNonMS"), SF_Pixel);
// BEGIN CATACLYSM
//atatarinov_begin
IMPLEMENT_SHADER_TYPE(, FResolveDepthDownsampleNonMSPS, TEXT("ResolvePixelShader"), TEXT("MainDepthDownsampleNonMS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FResolveDownsampleNonMSPS, TEXT("ResolvePixelShader"), TEXT("MainDownsampleNonMS"), SF_Pixel);
//atatarinov_end
// END CATACLYSM
IMPLEMENT_SHADER_TYPE(, FResolveSingleSamplePS, TEXT("ResolvePixelShader"), TEXT("MainSingleSample"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FResolveVS, TEXT("ResolveVertexShader"), TEXT("Main"), SF_Vertex);

void FResolveSingleSamplePS::SetParameters(FRHICommandList& RHICmdList, uint32 SingleSampleIndexValue)
{
	SetShaderValue(RHICmdList, GetPixelShader(),SingleSampleIndex,SingleSampleIndexValue);
}
