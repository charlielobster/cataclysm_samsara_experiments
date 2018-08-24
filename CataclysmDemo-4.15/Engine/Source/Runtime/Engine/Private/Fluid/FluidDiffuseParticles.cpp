// CATACLYSM 
#include "FluidDiffuseParticles.h"
#include "EnginePrivatePCH.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "FluidSimulationCommon.h"

DEFINE_STAT(STAT_FluidDP_Scan);
DEFINE_STAT(STAT_FluidDP_Compact);
DEFINE_STAT(STAT_FluidDP_Simulate);
DEFINE_STAT(STAT_FluidDP_Append);


BEGIN_UNIFORM_BUFFER_STRUCT(FDiffuseParticlesUniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, MaxParticlesCount)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, DispatchGridDim)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, DeltaSeconds)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, TrappedAirBias)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, TrappedAirScale)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, WaveCrestBias)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, WaveCrestScale)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, EnergyBias)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, EnergyScale)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, TrappedAirSamples)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, WaveCrestSamples)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, LifetimeMin)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, LifetimeMax)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, BubbleDrag)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, BubbleBuoyancy)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, FoamSurfaceDistThreshold)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, GenerateSurfaceDistThreshold)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, GenerateRadius)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, FluidParticleCount)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, Samples, [NUM_FOAM_SAMPLES])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, FadeinTime)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, FadeoutTime)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, SideVelocityScale)
END_UNIFORM_BUFFER_STRUCT(FDiffuseParticlesUniformParameters)

template <uint32 Phase>
class FDiffuseParticlesScanCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDiffuseParticlesScanCS, Global);

public:
	static const int NVExtensionSlot = 7;

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NV_SHADER_EXTN_SLOT"), *FString::Printf(TEXT("u%d"), NVExtensionSlot));
	}

	FDiffuseParticlesScanCS()
	{
	}

	explicit FDiffuseParticlesScanCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		LastCount.Bind(Initializer.ParameterMap, TEXT("LastCount"));
		TargetCountRW.Bind(Initializer.ParameterMap, TEXT("TargetCountRW"));
		ScanIn.Bind(Initializer.ParameterMap, TEXT("ScanIn"));
		ScanOut.Bind(Initializer.ParameterMap, TEXT("ScanOut"));
		ScanTemp.Bind(Initializer.ParameterMap, TEXT("ScanTemp"));

		NVExtensionUAV.Bind(Initializer.ParameterMap, TEXT("g_NvidiaExt"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LastCount;
		Ar << TargetCountRW;
		Ar << ScanIn;
		Ar << ScanOut;
		Ar << ScanTemp;
		Ar << NVExtensionUAV;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef TargetCountUAV,
		FUnorderedAccessViewRHIParamRef ScanOutUAV,
		FUnorderedAccessViewRHIParamRef ScanTempUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (TargetCountRW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, TargetCountRW.GetBaseIndex(), TargetCountUAV);
		}
		if (ScanOut.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, ScanOut.GetBaseIndex(), ScanOutUAV);
		}
		if (ScanTemp.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, ScanTemp.GetBaseIndex(), ScanTempUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef LastCountSRV,
		FShaderResourceViewRHIParamRef ScanInSRV,
		FUniformBufferRHIParamRef UniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDiffuseParticlesUniformParameters>(), UniformBuffer);

		if (LastCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, LastCount.GetBaseIndex(), LastCountSRV);
		}
		if (ScanIn.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ScanIn.GetBaseIndex(), ScanInSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (TargetCountRW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, TargetCountRW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (ScanOut.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, ScanOut.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (ScanTemp.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, ScanTemp.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

	virtual int GetNvShaderExtnSlot() override
	{
		return NVExtensionSlot;
	}

private:
	FShaderResourceParameter LastCount;
	FShaderResourceParameter TargetCountRW;
	FShaderResourceParameter ScanIn;
	FShaderResourceParameter ScanOut;
	FShaderResourceParameter ScanTemp;

	/** For using the nv extensions. */
	FShaderResourceParameter NVExtensionUAV;
};

class FDiffuseParticlesCompactCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDiffuseParticlesCompactCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	FDiffuseParticlesCompactCS()
	{
	}

	explicit FDiffuseParticlesCompactCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		LastCount.Bind(Initializer.ParameterMap, TEXT("LastCount"));
		TargetCount.Bind(Initializer.ParameterMap, TEXT("TargetCount"));
		CompactIn.Bind(Initializer.ParameterMap, TEXT("CompactIn"));
		CompactOut.Bind(Initializer.ParameterMap, TEXT("CompactOut"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LastCount;
		Ar << TargetCount;
		Ar << CompactIn;
		Ar << CompactOut;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef CompactOutUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (CompactOut.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, CompactOut.GetBaseIndex(), CompactOutUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef LastCountSRV,
		FShaderResourceViewRHIParamRef TargetCountSRV,
		FShaderResourceViewRHIParamRef CompactInSRV,
		FUniformBufferRHIParamRef InUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDiffuseParticlesUniformParameters>(), InUniformBuffer);

		if (LastCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, LastCount.GetBaseIndex(), LastCountSRV);
		}
		if (TargetCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, TargetCount.GetBaseIndex(), TargetCountSRV);
		}
		if (CompactIn.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, CompactIn.GetBaseIndex(), CompactInSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (CompactOut.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, CompactOut.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter LastCount;
	FShaderResourceParameter TargetCount;
	FShaderResourceParameter CompactIn;
	FShaderResourceParameter CompactOut;
};

class FDiffuseParticlesSimulateCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDiffuseParticlesSimulateCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	FDiffuseParticlesSimulateCS()
	{
	}

	explicit FDiffuseParticlesSimulateCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TargetCount.Bind(Initializer.ParameterMap, TEXT("TargetCount"));
		HoleScanSum.Bind(Initializer.ParameterMap, TEXT("HoleScanSum"));
		MoveIndices.Bind(Initializer.ParameterMap, TEXT("MoveIndices"));
		PositionBuffer.Bind(Initializer.ParameterMap, TEXT("PositionBuffer"));
		VelocityBuffer.Bind(Initializer.ParameterMap, TEXT("VelocityBuffer"));
		RenderAttrBuffer.Bind(Initializer.ParameterMap, TEXT("RenderAttrBuffer"));

		LiquidSurface.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTexture"));
		LiquidSurfaceSampler.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTextureSampler"));
		SolidBoundary.Bind(Initializer.ParameterMap, TEXT("SolidBoundaryTexture"));
		SolidBoundarySampler.Bind(Initializer.ParameterMap, TEXT("SolidBoundaryTextureSampler"));
		GridVelocityU.Bind(Initializer.ParameterMap, TEXT("GridVelocityUTexture"));
		GridVelocityUSampler.Bind(Initializer.ParameterMap, TEXT("GridVelocityUTextureSampler"));
		GridVelocityV.Bind(Initializer.ParameterMap, TEXT("GridVelocityVTexture"));
		GridVelocityVSampler.Bind(Initializer.ParameterMap, TEXT("GridVelocityVTextureSampler"));
		GridVelocityW.Bind(Initializer.ParameterMap, TEXT("GridVelocityWTexture"));
		GridVelocityWSampler.Bind(Initializer.ParameterMap, TEXT("GridVelocityWTextureSampler"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TargetCount;
		Ar << HoleScanSum;
		Ar << MoveIndices;
		Ar << PositionBuffer;
		Ar << VelocityBuffer;
		Ar << RenderAttrBuffer;

		Ar << LiquidSurface;
		Ar << LiquidSurfaceSampler;
		Ar << SolidBoundary;
		Ar << SolidBoundarySampler;
		Ar << GridVelocityU;
		Ar << GridVelocityUSampler;
		Ar << GridVelocityV;
		Ar << GridVelocityVSampler;
		Ar << GridVelocityW;
		Ar << GridVelocityWSampler;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef HoleScanSumUAV,
		FUnorderedAccessViewRHIParamRef PositionBufferUAV,
		FUnorderedAccessViewRHIParamRef VelocityBufferUAV,
		FUnorderedAccessViewRHIParamRef RenderAttrBufferUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (HoleScanSum.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, HoleScanSum.GetBaseIndex(), HoleScanSumUAV);
		}
		if (PositionBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, PositionBuffer.GetBaseIndex(), PositionBufferUAV);
		}
		if (VelocityBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, VelocityBuffer.GetBaseIndex(), VelocityBufferUAV);
		}
		if (RenderAttrBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, RenderAttrBuffer.GetBaseIndex(), RenderAttrBufferUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef TargetCountSRV,
		FShaderResourceViewRHIParamRef MoveIndicesSRV,
		FUniformBufferRHIParamRef InUniformBuffer,
		FTexture3DRHIParamRef LiquidSurfaceRHI,
		FTexture3DRHIParamRef SolidBoundaryRHI,
		FTexture3DRHIParamRef GridVelocityURHI,
		FTexture3DRHIParamRef GridVelocityVRHI,
		FTexture3DRHIParamRef GridVelocityWRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDiffuseParticlesUniformParameters>(), InUniformBuffer);

		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);

		FSamplerStateRHIParamRef SamplerStateTrilinear = TStaticSamplerState<SF_Trilinear>::GetRHI();
		if (LiquidSurface.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, LiquidSurface, LiquidSurfaceSampler, SamplerStateTrilinear, LiquidSurfaceRHI);
		if (SolidBoundary.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, SolidBoundary, SolidBoundarySampler, SamplerStateTrilinear, SolidBoundaryRHI);
		if (GridVelocityU.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, GridVelocityU, GridVelocityUSampler, SamplerStateTrilinear, GridVelocityURHI);
		if (GridVelocityV.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, GridVelocityV, GridVelocityVSampler, SamplerStateTrilinear, GridVelocityVRHI);
		if (GridVelocityW.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, GridVelocityW, GridVelocityWSampler, SamplerStateTrilinear, GridVelocityWRHI);

		if (TargetCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, TargetCount.GetBaseIndex(), TargetCountSRV);
		}
		if (MoveIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, MoveIndices.GetBaseIndex(), MoveIndicesSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (HoleScanSum.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, HoleScanSum.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (PositionBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, PositionBuffer.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (VelocityBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, VelocityBuffer.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (RenderAttrBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, RenderAttrBuffer.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter TargetCount;
	FShaderResourceParameter HoleScanSum;
	FShaderResourceParameter MoveIndices;
	FShaderResourceParameter PositionBuffer;
	FShaderResourceParameter VelocityBuffer;
	FShaderResourceParameter RenderAttrBuffer;

	FShaderResourceParameter LiquidSurface;
	FShaderResourceParameter LiquidSurfaceSampler;
	FShaderResourceParameter SolidBoundary;
	FShaderResourceParameter SolidBoundarySampler;
	FShaderResourceParameter GridVelocityU;
	FShaderResourceParameter GridVelocityUSampler;
	FShaderResourceParameter GridVelocityV;
	FShaderResourceParameter GridVelocityVSampler;
	FShaderResourceParameter GridVelocityW;
	FShaderResourceParameter GridVelocityWSampler;
};

class FDiffuseParticlesAppendCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDiffuseParticlesAppendCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	FDiffuseParticlesAppendCS()
	{
	}

	explicit FDiffuseParticlesAppendCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TargetCountRW.Bind(Initializer.ParameterMap, TEXT("TargetCountRW"));
		PositionBuffer.Bind(Initializer.ParameterMap, TEXT("PositionBuffer"));
		VelocityBuffer.Bind(Initializer.ParameterMap, TEXT("VelocityBuffer"));
		RenderAttrBuffer.Bind(Initializer.ParameterMap, TEXT("RenderAttrBuffer"));
		AppendIndices.Bind(Initializer.ParameterMap, TEXT("AppendIndices"));

		LiquidSurface.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTexture"));
		LiquidSurfaceSampler.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTextureSampler"));
		SolidBoundary.Bind(Initializer.ParameterMap, TEXT("SolidBoundaryTexture"));
		SolidBoundarySampler.Bind(Initializer.ParameterMap, TEXT("SolidBoundaryTextureSampler"));
		GridVelocityU.Bind(Initializer.ParameterMap, TEXT("GridVelocityUTexture"));
		GridVelocityUSampler.Bind(Initializer.ParameterMap, TEXT("GridVelocityUTextureSampler"));
		GridVelocityV.Bind(Initializer.ParameterMap, TEXT("GridVelocityVTexture"));
		GridVelocityVSampler.Bind(Initializer.ParameterMap, TEXT("GridVelocityVTextureSampler"));
		GridVelocityW.Bind(Initializer.ParameterMap, TEXT("GridVelocityWTexture"));
		GridVelocityWSampler.Bind(Initializer.ParameterMap, TEXT("GridVelocityWTextureSampler"));
		ParticleIndices.Bind(Initializer.ParameterMap, TEXT("ParticleIndices"));
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TargetCountRW;
		Ar << PositionBuffer;
		Ar << VelocityBuffer;
		Ar << RenderAttrBuffer;
		Ar << AppendIndices;

		Ar << LiquidSurface;
		Ar << LiquidSurfaceSampler;
		Ar << SolidBoundary;
		Ar << SolidBoundarySampler;
		Ar << GridVelocityU;
		Ar << GridVelocityUSampler;
		Ar << GridVelocityV;
		Ar << GridVelocityVSampler;
		Ar << GridVelocityW;
		Ar << GridVelocityWSampler;
		Ar << ParticleIndices;
		Ar << PositionTexture;
		Ar << VelocityTexture;

		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef TargetCountUAV,
		FUnorderedAccessViewRHIParamRef PositionBufferUAV,
		FUnorderedAccessViewRHIParamRef VelocityBufferUAV,
		FUnorderedAccessViewRHIParamRef RenderAttrBufferUAV,
		FUnorderedAccessViewRHIParamRef AppendIndicesUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (TargetCountRW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, TargetCountRW.GetBaseIndex(), TargetCountUAV);
		}
		if (PositionBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, PositionBuffer.GetBaseIndex(), PositionBufferUAV);
		}
		if (VelocityBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, VelocityBuffer.GetBaseIndex(), VelocityBufferUAV);
		}
		if (RenderAttrBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, RenderAttrBuffer.GetBaseIndex(), RenderAttrBufferUAV);
		}
		if (AppendIndices.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, AppendIndices.GetBaseIndex(), AppendIndicesUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FUniformBufferRHIParamRef InUniformBuffer,
		FTexture3DRHIParamRef LiquidSurfaceRHI,
		FTexture3DRHIParamRef SolidBoundaryRHI,
		FTexture3DRHIParamRef GridVelocityURHI,
		FTexture3DRHIParamRef GridVelocityVRHI,
		FTexture3DRHIParamRef GridVelocityWRHI,
		FShaderResourceViewRHIParamRef ParticleIndicesSRV,
		FTexture2DRHIParamRef PositionTextureRHI,
		FTexture2DRHIParamRef VelocityTextureRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDiffuseParticlesUniformParameters>(), InUniformBuffer);

		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);

		FSamplerStateRHIParamRef SamplerStateTrilinear = TStaticSamplerState<SF_Trilinear>::GetRHI();
		if (LiquidSurface.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, LiquidSurface, LiquidSurfaceSampler, SamplerStateTrilinear, LiquidSurfaceRHI);
		if (SolidBoundary.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, SolidBoundary, SolidBoundarySampler, SamplerStateTrilinear, SolidBoundaryRHI);
		if (GridVelocityU.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, GridVelocityU, GridVelocityUSampler, SamplerStateTrilinear, GridVelocityURHI);
		if (GridVelocityV.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, GridVelocityV, GridVelocityVSampler, SamplerStateTrilinear, GridVelocityVRHI);
		if (GridVelocityW.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, GridVelocityW, GridVelocityWSampler, SamplerStateTrilinear, GridVelocityWRHI);

		if (ParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleIndices.GetBaseIndex(), ParticleIndicesSRV);
		}
		if (PositionTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
		if (VelocityTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, VelocityTexture.GetBaseIndex(), VelocityTextureRHI);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (TargetCountRW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, TargetCountRW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (PositionBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, PositionBuffer.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (VelocityBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, VelocityBuffer.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (RenderAttrBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, RenderAttrBuffer.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (AppendIndices.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, AppendIndices.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter TargetCountRW;
	FShaderResourceParameter PositionBuffer;
	FShaderResourceParameter VelocityBuffer;
	FShaderResourceParameter RenderAttrBuffer;
	FShaderResourceParameter AppendIndices;

	FShaderResourceParameter LiquidSurface;
	FShaderResourceParameter LiquidSurfaceSampler;
	FShaderResourceParameter SolidBoundary;
	FShaderResourceParameter SolidBoundarySampler;
	FShaderResourceParameter GridVelocityU;
	FShaderResourceParameter GridVelocityUSampler;
	FShaderResourceParameter GridVelocityV;
	FShaderResourceParameter GridVelocityVSampler;
	FShaderResourceParameter GridVelocityW;
	FShaderResourceParameter GridVelocityWSampler;
	FShaderResourceParameter ParticleIndices;
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter VelocityTexture;
};

class FDiffuseParticlesFinalizeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDiffuseParticlesFinalizeCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	FDiffuseParticlesFinalizeCS()
	{
	}

	explicit FDiffuseParticlesFinalizeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TargetCount.Bind(Initializer.ParameterMap, TEXT("TargetCount"));
		LastCountRW.Bind(Initializer.ParameterMap, TEXT("LastCountRW"));
		DrawArgs.Bind(Initializer.ParameterMap, TEXT("DrawArgs"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TargetCount;
		Ar << LastCountRW;
		Ar << DrawArgs;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef LastCountUAV,
		FUnorderedAccessViewRHIParamRef DrawArgsUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (LastCountRW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, LastCountRW.GetBaseIndex(), LastCountUAV);
		}
		if (DrawArgs.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, DrawArgs.GetBaseIndex(), DrawArgsUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef TargetCountSRV,
		FUniformBufferRHIParamRef InUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDiffuseParticlesUniformParameters>(), InUniformBuffer);

		if (TargetCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, TargetCount.GetBaseIndex(), TargetCountSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (LastCountRW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, LastCountRW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (DrawArgs.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, DrawArgs.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter TargetCount;
	FShaderResourceParameter LastCountRW;
	FShaderResourceParameter DrawArgs;
};


IMPLEMENT_UNIFORM_BUFFER_STRUCT(FDiffuseParticlesUniformParameters, TEXT("DiffuseParticles"));


IMPLEMENT_SHADER_TYPE(template<>, FDiffuseParticlesScanCS<1>, TEXT("FluidDiffuseParticles"), TEXT("ScanPhase1Kernel"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FDiffuseParticlesScanCS<2>, TEXT("FluidDiffuseParticles"), TEXT("ScanPhase2Kernel"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FDiffuseParticlesScanCS<3>, TEXT("FluidDiffuseParticles"), TEXT("ScanPhase3Kernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FDiffuseParticlesCompactCS, TEXT("FluidDiffuseParticles"), TEXT("CompactKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FDiffuseParticlesSimulateCS, TEXT("FluidDiffuseParticles"), TEXT("SimulateKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FDiffuseParticlesAppendCS, TEXT("FluidDiffuseParticles"), TEXT("AppendKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FDiffuseParticlesFinalizeCS, TEXT("FluidDiffuseParticles"), TEXT("FinalizeKernel"), SF_Compute);


FFluidDiffuseParticles::FFluidDiffuseParticles(uint32 InMaxParticlesCount)
{
	MaxParticlesCount = InMaxParticlesCount;
	DispatchGridDim = 32; //TODO: set optimal value based on SM count!

	Indices[0].Initialize(sizeof(uint32), MaxParticlesCount, PF_R32_UINT);
	Indices[1].Initialize(sizeof(uint32), MaxParticlesCount, PF_R32_UINT);
	ScanTemp.Initialize(sizeof(uint32), 32 * DispatchGridDim, PF_R32_UINT);
	LastCount.Initialize(sizeof(uint32), 1, PF_R32_UINT);
	TargetCount.Initialize(sizeof(uint32), 1, PF_R32_UINT);

	PositionBuffer.Initialize(sizeof(float) * 4, MaxParticlesCount, PF_A32B32G32R32F);
	VelocityBuffer.Initialize(sizeof(float) * 4, MaxParticlesCount, PF_A32B32G32R32F);
	RenderAttrBuffer.Initialize(sizeof(float), MaxParticlesCount, PF_R32_FLOAT);

	DrawArgs.Initialize(sizeof(uint32), 4, PF_R32_UINT, BUF_DrawIndirect);

	FrameIndex = -1;
}

FFluidDiffuseParticles::~FFluidDiffuseParticles()
{
}

void FFluidDiffuseParticles::Destroy()
{
	DrawArgs.Release();
	RenderAttrBuffer.Release();
	VelocityBuffer.Release();
	PositionBuffer.Release();
	TargetCount.Release();
	LastCount.Release();
	ScanTemp.Release();
	Indices[1].Release();
	Indices[0].Release();

	delete this;
}

#define TEST 0

void FFluidDiffuseParticles::Simulate(FRHICommandListImmediate& RHICmdList, float DeltaSeconds,
	uint32 FluidParticleCount,
	FTexture3DRHIParamRef LiquidSurfaceRHI,
	FTexture3DRHIParamRef SolidBoundaryRHI,
	FTexture3DRHIParamRef GridVelocityURHI,
	FTexture3DRHIParamRef GridVelocityVRHI,
	FTexture3DRHIParamRef GridVelocityWRHI,
	FShaderResourceViewRHIParamRef ParticleIndicesSRV,
	FTexture2DRHIParamRef PositionTextureRHI,
	FTexture2DRHIParamRef VelocityTextureRHI,
	FUniformBufferRHIParamRef VoxelUniformBuffer
	)
{
	FDiffuseParticlesUniformParameters DiffuseParticlesParameters;
	DiffuseParticlesParameters.MaxParticlesCount = MaxParticlesCount;
	DiffuseParticlesParameters.DispatchGridDim = DispatchGridDim;
	DiffuseParticlesParameters.DeltaSeconds = DeltaSeconds;

	DiffuseParticlesParameters.TrappedAirBias = -Parameters.TrappedAirMin;
	DiffuseParticlesParameters.TrappedAirScale = 1.0f / (Parameters.TrappedAirMax - Parameters.TrappedAirMin);
	DiffuseParticlesParameters.WaveCrestBias = -Parameters.WaveCrestMin;
	DiffuseParticlesParameters.WaveCrestScale = 1.0f / (Parameters.WaveCrestMax - Parameters.WaveCrestMin);
	DiffuseParticlesParameters.EnergyBias = -Parameters.EnergyMin;
	DiffuseParticlesParameters.EnergyScale = 1.0f / (Parameters.EnergyMax - Parameters.EnergyMin);
	DiffuseParticlesParameters.TrappedAirSamples = Parameters.TrappedAirSamples;
	DiffuseParticlesParameters.WaveCrestSamples = Parameters.WaveCrestSamples;
	DiffuseParticlesParameters.LifetimeMin = Parameters.LifetimeMin;
	DiffuseParticlesParameters.LifetimeMax = Parameters.LifetimeMax;
	DiffuseParticlesParameters.FluidParticleCount = FluidParticleCount;
	for (uint32 SampleIndex = 0; SampleIndex < NUM_FOAM_SAMPLES;)
	{
		float X = FMath::FRand() * 2.0f - 1.0f;
		float Y = FMath::FRand() * 2.0f - 1.0f;
		float Z = FMath::FRand() * 2.0f - 1.0f;
		float R = FMath::Sqrt(X*X + Y*Y + Z*Z);
		if (R >= 0.1f && R <= 0.9f)
		{
			float W = (1.0f - R);
			DiffuseParticlesParameters.Samples[SampleIndex] = FVector4(X, Y, Z, W);
			++SampleIndex;
		}
	}
	DiffuseParticlesParameters.BubbleDrag = Parameters.BubbleDrag;
	DiffuseParticlesParameters.BubbleBuoyancy = Parameters.BubbleBuoyancy;
	DiffuseParticlesParameters.FoamSurfaceDistThreshold = Parameters.FoamSurfaceDistThreshold;
	DiffuseParticlesParameters.GenerateSurfaceDistThreshold = Parameters.GenerateSurfaceDistThreshold;
	DiffuseParticlesParameters.GenerateRadius = Parameters.GenerateRadius;
	DiffuseParticlesParameters.FadeinTime = Parameters.FadeinTime;
	DiffuseParticlesParameters.FadeoutTime = Parameters.FadeoutTime;
	DiffuseParticlesParameters.SideVelocityScale = Parameters.SideVelocityScale;


	auto DiffuseParticlesUniformBuffer =
		TUniformBufferRef<FDiffuseParticlesUniformParameters>::CreateUniformBufferImmediate(DiffuseParticlesParameters, UniformBuffer_SingleFrame);

	if (FrameIndex < 0) //first call check
	{
		uint32 ClearValues[4] = { 0 };
		RHICmdList.ClearUAV(LastCount.UAV, ClearValues);

		FrameIndex = 0;
	}

#if TEST
	FRHIResourceCreateInfo CreateInfo;
	static FStructuredBufferRHIRef StagingIndices0 = RHICreateStructuredBuffer(sizeof(uint32), sizeof(uint32) * MaxParticlesCount, BUF_Staging, CreateInfo);
	static FStructuredBufferRHIRef StagingIndices1 = RHICreateStructuredBuffer(sizeof(uint32), sizeof(uint32) * MaxParticlesCount, BUF_Staging, CreateInfo);
	static FStructuredBufferRHIRef StagingCount = RHICreateStructuredBuffer(sizeof(uint32), sizeof(uint32) * 1, BUF_Staging, CreateInfo);

	RHICopyStructuredBuffer(LastCount.Buffer, StagingCount);
	RHICopyStructuredBuffer(Indices[FrameIndex].Buffer, StagingIndices0);

	const uint32* Data = (const uint32*)RHILockStructuredBuffer(StagingCount, 0, sizeof(uint32) * 1, RLM_ReadOnly);
	uint32 LastCountCopy = Data[0];
	RHIUnlockStructuredBuffer(StagingCount);
#endif

	//scan
	// input: CompactIndices
	// output: ScanHoles
	{
		SCOPE_CYCLE_COUNTER(STAT_FluidDP_Scan);
		SCOPED_DRAW_EVENT(RHICmdList, FluidDP_Scan);

		FShaderResourceViewRHIRef ScanIn = Indices[FrameIndex].SRV;
		FUnorderedAccessViewRHIRef ScanOut = Indices[FrameIndex ^ 1].UAV;

		TShaderMapRef<FDiffuseParticlesScanCS<1> > ScanCS1(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FDiffuseParticlesScanCS<2> > ScanCS2(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FDiffuseParticlesScanCS<3> > ScanCS3(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		//phase 1
		RHICmdList.SetComputeShader(ScanCS1->GetComputeShader());

		ScanCS1->SetParameters(RHICmdList, LastCount.SRV, ScanIn, DiffuseParticlesUniformBuffer);
		ScanCS1->SetOutput(RHICmdList, TargetCount.UAV, ScanOut, ScanTemp.UAV);

		DispatchComputeShader(RHICmdList, *ScanCS1, DispatchGridDim, 1, 1);
		ScanCS1->UnbindBuffers(RHICmdList);

		//phase 2
		RHICmdList.SetComputeShader(ScanCS2->GetComputeShader());

		ScanCS2->SetParameters(RHICmdList, LastCount.SRV, ScanIn, DiffuseParticlesUniformBuffer);
		ScanCS2->SetOutput(RHICmdList, TargetCount.UAV, ScanOut, ScanTemp.UAV);

		DispatchComputeShader(RHICmdList, *ScanCS2, 1, 1, 1);
		ScanCS2->UnbindBuffers(RHICmdList);

		//phase 3
		RHICmdList.SetComputeShader(ScanCS3->GetComputeShader());

		ScanCS3->SetParameters(RHICmdList, LastCount.SRV, ScanIn, DiffuseParticlesUniformBuffer);
		ScanCS3->SetOutput(RHICmdList, TargetCount.UAV, ScanOut, ScanTemp.UAV);

		DispatchComputeShader(RHICmdList, *ScanCS3, DispatchGridDim, 1, 1);
		ScanCS3->UnbindBuffers(RHICmdList);
	}

#if TEST
	RHICopyStructuredBuffer(TargetCount.Buffer, StagingCount);
	RHICopyStructuredBuffer(Indices[FrameIndex ^ 1].Buffer, StagingIndices1);

	Data = (const uint32*)RHILockStructuredBuffer(StagingCount, 0, sizeof(uint32) * 1, RLM_ReadOnly);
	uint32 TargetCountCopy = Data[0];
	RHIUnlockStructuredBuffer(StagingCount);

	const uint32* ScanInData = (const uint32*)RHILockStructuredBuffer(StagingIndices0, 0, sizeof(uint32) * LastCountCopy, RLM_ReadOnly);
	const uint32* ScanOutData = (const uint32*)RHILockStructuredBuffer(StagingIndices1, 0, sizeof(uint32) * LastCountCopy, RLM_ReadOnly);

	uint32 HolesSum = 0;
	for (uint32 i = 0; i < LastCountCopy; ++i)
	{
		bool bIsHole = (ScanInData[i] != 0);
		uint32 ScanSum = ScanOutData[i];
		if (bIsHole)
		{
			check((ScanSum & 0x80000000u) != 0);
			++HolesSum;
		}
		else
		{
			check((ScanSum & 0x80000000u) == 0);
		}
		ScanSum &= 0x7FFFFFFFu;
		check(ScanSum == HolesSum);
	}
	check(LastCountCopy - HolesSum == TargetCountCopy);

	RHIUnlockStructuredBuffer(StagingIndices1);
	RHIUnlockStructuredBuffer(StagingIndices0);

#endif

	//compact
	{
		SCOPE_CYCLE_COUNTER(STAT_FluidDP_Compact);
		SCOPED_DRAW_EVENT(RHICmdList, FluidDP_Compact);

		FShaderResourceViewRHIRef CompactIn = Indices[FrameIndex ^ 1].SRV;
		FUnorderedAccessViewRHIRef CompactOut = Indices[FrameIndex].UAV;

		TShaderMapRef<FDiffuseParticlesCompactCS > CompactCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		RHICmdList.SetComputeShader(CompactCS->GetComputeShader());

		CompactCS->SetParameters(RHICmdList, LastCount.SRV, TargetCount.SRV, CompactIn, DiffuseParticlesUniformBuffer);
		CompactCS->SetOutput(RHICmdList, CompactOut);

		DispatchComputeShader(RHICmdList, *CompactCS, DispatchGridDim, 1, 1);
		CompactCS->UnbindBuffers(RHICmdList);
	}

#if TEST
	RHICopyStructuredBuffer(Indices[FrameIndex].Buffer, StagingIndices0);

	const uint32* CompactOutData = (const uint32*)RHILockStructuredBuffer(StagingIndices0, 0, sizeof(uint32) * LastCountCopy, RLM_ReadOnly);
	const uint32* CompactInData = (const uint32*)RHILockStructuredBuffer(StagingIndices1, 0, sizeof(uint32) * LastCountCopy, RLM_ReadOnly);

	for (uint32 k = 0, i = TargetCountCopy; i < LastCountCopy; ++i)
	{
		uint32 ScanSum = CompactInData[i];
		if ((ScanSum & 0x80000000u) == 0)
		{
			check(CompactOutData[k] == i);
			++k;
		}
	}

	RHIUnlockStructuredBuffer(StagingIndices1);
	RHIUnlockStructuredBuffer(StagingIndices0);

#endif

	//simulate
	{
		SCOPE_CYCLE_COUNTER(STAT_FluidDP_Simulate);
		SCOPED_DRAW_EVENT(RHICmdList, FluidDP_Simulate);

		FShaderResourceViewRHIRef MoveIndices = Indices[FrameIndex].SRV;
		FUnorderedAccessViewRHIRef HoleScanSum = Indices[FrameIndex ^ 1].UAV;

		TShaderMapRef<FDiffuseParticlesSimulateCS > SimulateCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		RHICmdList.SetComputeShader(SimulateCS->GetComputeShader());

		SimulateCS->SetParameters(RHICmdList, TargetCount.SRV, MoveIndices, DiffuseParticlesUniformBuffer,
			LiquidSurfaceRHI, SolidBoundaryRHI, GridVelocityURHI, GridVelocityVRHI, GridVelocityWRHI, VoxelUniformBuffer);
		SimulateCS->SetOutput(RHICmdList, HoleScanSum, PositionBuffer.UAV, VelocityBuffer.UAV, RenderAttrBuffer.UAV);

		DispatchComputeShader(RHICmdList, *SimulateCS, DispatchGridDim, 1, 1);
		SimulateCS->UnbindBuffers(RHICmdList);
	}

	//append
	{
		SCOPE_CYCLE_COUNTER(STAT_FluidDP_Append);
		SCOPED_DRAW_EVENT(RHICmdList, FluidDP_Append);

		FUnorderedAccessViewRHIRef AppendIndices = Indices[FrameIndex ^ 1].UAV;

		TShaderMapRef<FDiffuseParticlesAppendCS > AppendCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		RHICmdList.SetComputeShader(AppendCS->GetComputeShader());

		AppendCS->SetParameters(RHICmdList, DiffuseParticlesUniformBuffer,
			LiquidSurfaceRHI,
			SolidBoundaryRHI,
			GridVelocityURHI,
			GridVelocityVRHI,
			GridVelocityWRHI,
			ParticleIndicesSRV,
			PositionTextureRHI,
			VelocityTextureRHI,
			VoxelUniformBuffer
			);
		AppendCS->SetOutput(RHICmdList, TargetCount.UAV, PositionBuffer.UAV, VelocityBuffer.UAV, RenderAttrBuffer.UAV, AppendIndices);

		const uint32 GroupCount = FMath::Max<uint32>(1, (FluidParticleCount + DPARTICLES_APPEND_WG_SIZE - 1) / DPARTICLES_APPEND_WG_SIZE);
		DispatchComputeShader(RHICmdList, *AppendCS, GroupCount, 1, 1);
		AppendCS->UnbindBuffers(RHICmdList);
	}

	//finalize
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidDP_Finalize);
		TShaderMapRef<FDiffuseParticlesFinalizeCS > FinalizeCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		RHICmdList.SetComputeShader(FinalizeCS->GetComputeShader());

		FinalizeCS->SetParameters(RHICmdList, TargetCount.SRV, DiffuseParticlesUniformBuffer);
		FinalizeCS->SetOutput(RHICmdList, LastCount.UAV, DrawArgs.UAV);

		DispatchComputeShader(RHICmdList, *FinalizeCS, 1, 1, 1);
		FinalizeCS->UnbindBuffers(RHICmdList);
	}

	FrameIndex = FrameIndex ^ 1;
}
