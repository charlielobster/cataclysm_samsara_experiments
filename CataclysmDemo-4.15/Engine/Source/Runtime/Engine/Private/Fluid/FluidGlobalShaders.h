// CATACLYSM
#pragma once
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"
#include "GlobalDistanceFieldParameters.h"

BEGIN_UNIFORM_BUFFER_STRUCT( FFluidTriggerUniformParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FMatrix, VolumeToWorld, [MAX_FLUID_TRIGGERS])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FMatrix, WorldToVolume, [MAX_FLUID_TRIGGERS])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY( FVector, VolumeSize, [MAX_FLUID_TRIGGERS] )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY( FVector, CenterPos, [MAX_FLUID_TRIGGERS] )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, Count )
END_UNIFORM_BUFFER_STRUCT( FFluidTriggerUniformParameters )

BEGIN_UNIFORM_BUFFER_STRUCT(FBrickDataDebuggerUniformParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, IndexOfComponent)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, BricksNum)
END_UNIFORM_BUFFER_STRUCT(FBrickDataDebuggerUniformParameters)

BEGIN_UNIFORM_BUFFER_STRUCT(FRadixSortUniformParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, ElemCount)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, GridDim)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, StartBit)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, EndBit)
END_UNIFORM_BUFFER_STRUCT(FRadixSortUniformParameters)

BEGIN_UNIFORM_BUFFER_STRUCT(FPrepareFFTUniformParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Bits)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, Step)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, Time)
END_UNIFORM_BUFFER_STRUCT(FPrepareFFTUniformParameters)

BEGIN_UNIFORM_BUFFER_STRUCT(FExecuteFFTUniformParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Bits)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Bit)
END_UNIFORM_BUFFER_STRUCT(FExecuteFFTUniformParameters)
/**
* We first id all the bricks that have a particle in it, but the splat radius of particles can go 
* beyond the bricks that they are in, so we add a skirt of bricks so that we know we have at least
* the width of the brick extra to splat into. One could tighten the brick use but this balances 
* ease of coding and utility.
*/
class FAddSkirtCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FAddSkirtCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FAddSkirtCS()
	{
	}

	explicit FAddSkirtCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BricksWithParticles.Bind(Initializer.ParameterMap, TEXT("BricksWithParticles"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		BrickMap.Bind(Initializer.ParameterMap, TEXT("BrickMap"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BricksWithParticles;
		Ar << InCount;
		Ar << BrickMap;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef BrickMapUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (BrickMap.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, BrickMap.GetBaseIndex(), BrickMapUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef BricksWithParticlesRHI,
		FShaderResourceViewRHIParamRef InCountRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (BricksWithParticles.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, BricksWithParticles.GetBaseIndex(), BricksWithParticlesRHI);
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (BrickMap.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, BrickMap.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (BricksWithParticles.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, BricksWithParticles.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter BricksWithParticles;
	FShaderResourceParameter InCount;

	FShaderResourceParameter BrickMap;
};

class FBuildSolidBoundaryCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildSolidBoundaryCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	/** Default constructor. */
	FBuildSolidBoundaryCS()
	{
	}

	/** Initialization constructor. */
	explicit FBuildSolidBoundaryCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		// Input
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);

		// Output
		OutSolidBoundary.Bind(Initializer.ParameterMap, TEXT("OutSolidBoundary"));
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << ActiveBricksList;
		Ar << GlobalDistanceFieldParameters;
		Ar << OutSolidBoundary;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutSolidBoundaryUAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutSolidBoundary.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSolidBoundary.GetBaseIndex(), OutSolidBoundaryUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI,
		const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);
		GlobalDistanceFieldParameters.Set(RHICmdList, ComputeShaderRHI, *GlobalDistanceFieldParameterData);

		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		
		if ( ActiveBricksList.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutSolidBoundary.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSolidBoundary.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter ActiveBricksList;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;

	FShaderResourceParameter OutSolidBoundary;
};

/**
* The Variational boundary technique is very sensitive to solving the liquid near boundaries.  We 
* extend the levelset into the boundary so that we get accurate boundary conditions with it.
*/
class FExtendLiquidIntoSolidCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FExtendLiquidIntoSolidCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FExtendLiquidIntoSolidCS()
	{
	}

	explicit FExtendLiquidIntoSolidCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));
		InSolidBoundary.Bind(Initializer.ParameterMap, TEXT("InSolidBoundary"));

		OutLiquidBoundary.Bind(Initializer.ParameterMap, TEXT("OutLiquidBoundary"));
	}

	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << ActiveBricksList;
		Ar << InSolidBoundary;
		Ar << OutLiquidBoundary;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutLiquidBoundaryUAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutLiquidBoundary.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidBoundary.GetBaseIndex(), OutLiquidBoundaryUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI,
		FTexture3DRHIParamRef InSolidBoundaryRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}
		if (InSolidBoundary.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InSolidBoundary.GetBaseIndex(), InSolidBoundaryRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutLiquidBoundary.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidBoundary.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter ActiveBricksList;
	FShaderResourceParameter InSolidBoundary;

	FShaderResourceParameter OutLiquidBoundary;
};

class FCalculateBricksCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCalculateBricksCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FCalculateBricksCS()
	{
	}

	/** Initialization constructor. */
	explicit FCalculateBricksCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CurrentBrickMapTexture.Bind(Initializer.ParameterMap, TEXT("CurrentBrickMapTexture"));
		PrevBrickMapTexture.Bind(Initializer.ParameterMap, TEXT("PrevBrickMapTexture"));
		OutChangedBricks.Bind(Initializer.ParameterMap, TEXT("OutChangedBricks"));
		OutActiveBricks.Bind(Initializer.ParameterMap, TEXT("OutActiveBricks"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CurrentBrickMapTexture;
		Ar << PrevBrickMapTexture;
		Ar << OutChangedBricks;
		Ar << OutActiveBricks;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutChangedBricksUAV, FUnorderedAccessViewRHIParamRef OutActiveBricksUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutChangedBricks.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutChangedBricks.GetBaseIndex(), OutChangedBricksUAV, 0);
		}
		if (OutActiveBricks.IsBound())
		{
			// The active brick list is recreated from scratch each frame here.  So we need to reset it's size.
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutActiveBricks.GetBaseIndex(), OutActiveBricksUAV, 0);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef CurrentBrickMapTextureRHI,
		FTexture3DRHIParamRef PrevBrickMapTextureRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (CurrentBrickMapTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, CurrentBrickMapTexture.GetBaseIndex(), CurrentBrickMapTextureRHI);
		}
		if (PrevBrickMapTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PrevBrickMapTexture.GetBaseIndex(), PrevBrickMapTextureRHI);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutChangedBricks.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutChangedBricks.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}

		if (OutActiveBricks.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutActiveBricks.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** Input buffer. */
	FShaderResourceParameter CurrentBrickMapTexture;
	FShaderResourceParameter PrevBrickMapTexture;

	/** Output buffers. */
	FShaderResourceParameter OutChangedBricks;
	FShaderResourceParameter OutActiveBricks;
};

class FCreateBrickMapCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCreateBrickMapCS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	/** Default constructor. */
	FCreateBrickMapCS()
	{
	}

	/** Initialization constructor. */
	explicit FCreateBrickMapCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		// The GPU particles.
		InParticleIndices.Bind( Initializer.ParameterMap, TEXT("InParticleIndices") );
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));

		BrickMapUAV.Bind(Initializer.ParameterMap, TEXT("BrickMapRW"));
		OutParticleIndices.Bind(Initializer.ParameterMap, TEXT("OutParticleIndices"));
		OutBricksWithParticles.Bind(Initializer.ParameterMap, TEXT("OutBricksWithParticles"));
		OutParticlesPerVoxel.Bind(Initializer.ParameterMap, TEXT("OutParticlesPerVoxel"));
		OutParticlesToSort.Bind(Initializer.ParameterMap, TEXT("OutParticlesToSort"));
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << InParticleIndices;
		Ar << PositionTexture;
		Ar << BrickMapUAV;
		Ar << OutParticleIndices;
		Ar << OutBricksWithParticles;
		Ar << OutParticlesPerVoxel;
		Ar << OutParticlesToSort;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef BrickMapUAVRef,
		FUnorderedAccessViewRHIParamRef OutBricksWithParticlesUAV,
		FUnorderedAccessViewRHIParamRef OutParticlesIndicesUAV,
		FUnorderedAccessViewRHIParamRef OutParticlesToSortUAV,
		FUnorderedAccessViewRHIParamRef OutParticlesPerVoxelUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (BrickMapUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, BrickMapUAV.GetBaseIndex(), BrickMapUAVRef);
		}
		if (OutParticleIndices.IsBound())
		{
			// note 0 here sets the count to 0 for the append buffer.
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParticleIndices.GetBaseIndex(), OutParticlesIndicesUAV, 0);
		}
		if (OutBricksWithParticles.IsBound())
		{
			// note 0 here sets the count to 0 for the append buffer.
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutBricksWithParticles.GetBaseIndex(), OutBricksWithParticlesUAV, 0);
		}
		if (OutParticlesPerVoxel.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParticlesPerVoxel.GetBaseIndex(), OutParticlesPerVoxelUAV);
		}
		if (OutParticlesToSort.IsBound())
		{
			// note 0 here sets the count to 0 for the append buffer.
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParticlesToSort.GetBaseIndex(), OutParticlesToSortUAV, 0);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture2DRHIParamRef PositionTextureRHI,
		FShaderResourceViewRHIParamRef InIndicesSRV,
		FUniformBufferRHIParamRef VoxelUniformBuffer,
		FUniformBufferRHIParamRef ParticleCountUniformBuffer,
		FUniformBufferRHIParamRef SurfaceSculptingUniformBuffer)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FSurfaceSculptingUniformParameters>(), SurfaceSculptingUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FParticleCountUniformParameters>(), ParticleCountUniformBuffer);

		if ( PositionTexture.IsBound() )
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
		if ( InParticleIndices.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), InIndicesSRV);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( InParticleIndices.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( BrickMapUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, BrickMapUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutParticleIndices.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParticleIndices.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutBricksWithParticles.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutBricksWithParticles.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutParticlesPerVoxel.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParticlesPerVoxel.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutParticlesToSort.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParticlesToSort.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	/** Input buffer containing particle indices. */
	FShaderResourceParameter InParticleIndices;
	/** The particle position texture parameter. */
	FShaderResourceParameter PositionTexture;

	/** The VTR map texture created by final output positions. */
	FShaderResourceParameter BrickMapUAV;
	/** Output Append buffer for particle indices in uint. */
	FShaderResourceParameter OutParticleIndices;
	/** append buffer to attach bricks with at least one particle in them.*/
	FShaderResourceParameter OutBricksWithParticles;
	FShaderResourceParameter OutParticlesPerVoxel;
	FShaderResourceParameter OutParticlesToSort;

};

class FDivergenceLiquidCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDivergenceLiquidCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FDivergenceLiquidCS()
	{
	}

	explicit FDivergenceLiquidCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		InVoxels.Bind(Initializer.ParameterMap, TEXT("InVoxels"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		InU.Bind(Initializer.ParameterMap, TEXT("InU"));
		InV.Bind(Initializer.ParameterMap, TEXT("InV"));
		InW.Bind(Initializer.ParameterMap, TEXT("InW"));
		LiquidBoundary.Bind(Initializer.ParameterMap, TEXT("LiquidBoundary"));
		Weights.Bind(Initializer.ParameterMap, TEXT("Weights"));

		OutPressureSolverParams.Bind(Initializer.ParameterMap, TEXT("OutPressureSolverParams"));
	}

	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << InVoxels;
		Ar << InCount;
		Ar << InU;
		Ar << InV;
		Ar << InW;
		Ar << LiquidBoundary;
		Ar << Weights;
		Ar << OutPressureSolverParams;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutPressureSolverParamsUAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutPressureSolverParams.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutPressureSolverParams.GetBaseIndex(), OutPressureSolverParamsUAV);
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef InVoxelsRHI,
		FShaderResourceViewRHIParamRef InCountRHI,
		FTexture3DRHIParamRef InURHI,
		FTexture3DRHIParamRef InVRHI,
		FTexture3DRHIParamRef InWRHI,
		FTexture3DRHIParamRef LiquidBoundaryRHI,
		FTexture3DRHIParamRef WeightsRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);
		if (InVoxels.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InVoxels.GetBaseIndex(), InVoxelsRHI);
		if (InCount.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		if (InU.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InU.GetBaseIndex(), InURHI);
		if (InV.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InV.GetBaseIndex(), InVRHI);
		if (InW.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InW.GetBaseIndex(), InWRHI);
		if (LiquidBoundary.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, LiquidBoundary.GetBaseIndex(), LiquidBoundaryRHI);
		if (Weights.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, Weights.GetBaseIndex(), WeightsRHI);
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InVoxels.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InVoxels.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InCount.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (OutPressureSolverParams.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutPressureSolverParams.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}

private:
	FShaderResourceParameter InVoxels;
	FShaderResourceParameter InCount;

	FShaderResourceParameter InU;
	FShaderResourceParameter InV;
	FShaderResourceParameter InW;
	FShaderResourceParameter LiquidBoundary;
	FShaderResourceParameter Weights;

	FShaderResourceParameter OutPressureSolverParams;
};

template<bool UseAniso>
class FLevelsetSplatCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLevelsetSplatCS,Global);

public:
	static const int NVExtensionSlot = 7;
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine(TEXT("NV_SHADER_EXTN_SLOT"), *FString::Printf(TEXT("u%d"), NVExtensionSlot));
		OutEnvironment.SetDefine(TEXT("USE_ANISO"), (uint32)(UseAniso ? 1 : 0));
	}

	FLevelsetSplatCS()
	{
	}

	explicit FLevelsetSplatCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		ParticleVoxels.Bind(Initializer.ParameterMap, TEXT("ParticleVoxels"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		SortedParticleIndices.Bind(Initializer.ParameterMap, TEXT("SortedParticleIndices"));
		InPosInVox.Bind(Initializer.ParameterMap, TEXT("InPosInVox"));

		InSmoothDensity.Bind(Initializer.ParameterMap, TEXT("InSmoothDensity"));
		InSmoothDensitySampler.Bind(Initializer.ParameterMap, TEXT("InSmoothDensitySampler"));
		LiquidBoundary.Bind(Initializer.ParameterMap, TEXT("LiquidBoundary"));
		LiquidBoundarySampler.Bind(Initializer.ParameterMap, TEXT("LiquidBoundarySampler"));

		OutLiquidSurface.Bind(Initializer.ParameterMap, TEXT("OutLiquidSurface"));
		NVExtensionUAV.Bind(Initializer.ParameterMap, TEXT("g_NvidiaExt"));

	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << ParticleVoxels;
		Ar << InCount;
		Ar << VelocityTexture;
		Ar << SortedParticleIndices;
		Ar << InPosInVox;
		Ar << InSmoothDensity;
		Ar << InSmoothDensitySampler;
		Ar << LiquidBoundary;
		Ar << LiquidBoundarySampler;
		Ar << OutLiquidSurface;
		Ar << NVExtensionUAV;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutLiquidSurfaceUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutLiquidSurface.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidSurface.GetBaseIndex(), OutLiquidSurfaceUAV);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ParticleVoxelsRHI,
		FShaderResourceViewRHIParamRef InCountRHI,
		FTexture3DRHIParamRef InSmoothDensityRHI,
		FTexture3DRHIParamRef LiquidBoundaryRHI,
		FTexture2DRHIParamRef VelocityTextureRHI,
		FShaderResourceViewRHIParamRef SortedParticleIndicesSRV,
		FShaderResourceViewRHIParamRef InPosInVoxSRV,
		FUniformBufferRHIParamRef SurfaceSculptingUniformBuffer,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FSurfaceSculptingUniformParameters>(), SurfaceSculptingUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);


		if (SortedParticleIndices.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), SortedParticleIndicesSRV);
		if (VelocityTexture.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, VelocityTexture.GetBaseIndex(), VelocityTextureRHI);
		if (InPosInVox.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPosInVox.GetBaseIndex(), InPosInVoxSRV);
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), ParticleVoxelsRHI);
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		}
		if (InSmoothDensity.IsBound())
		{
			FSamplerStateRHIParamRef SamplerStateTrilinear = TStaticSamplerState<SF_Trilinear>::GetRHI();
			SetTextureParameter(RHICmdList, ComputeShaderRHI, InSmoothDensity, InSmoothDensitySampler, SamplerStateTrilinear, InSmoothDensityRHI);
		}
		if (LiquidBoundary.IsBound())
		{
			FSamplerStateRHIParamRef SamplerStateTrilinear = TStaticSamplerState<SF_Trilinear>::GetRHI();
			SetTextureParameter(RHICmdList, ComputeShaderRHI, LiquidBoundary, LiquidBoundarySampler, SamplerStateTrilinear, LiquidBoundaryRHI);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (SortedParticleIndices.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InPosInVox.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPosInVox.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutLiquidSurface.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidSurface.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

	virtual int GetNvShaderExtnSlot() override
	{
		return NVExtensionSlot;
	}

private:
	FShaderResourceParameter ParticleVoxels;
	FShaderResourceParameter InCount;

	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter SortedParticleIndices;
	FShaderResourceParameter InPosInVox;

	FShaderResourceParameter InSmoothDensitySampler;
	FShaderResourceParameter InSmoothDensity;
	FShaderResourceParameter LiquidBoundarySampler;
	FShaderResourceParameter LiquidBoundary;

	FShaderResourceParameter OutLiquidSurface;
	FShaderResourceParameter NVExtensionUAV;
};

/**
* To speed up rendering, we make a texture of all the bricks through which the surface could pass.
*/
class FMarkLevelsetCrossingsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMarkLevelsetCrossingsCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FMarkLevelsetCrossingsCS()
	{
	}

	explicit FMarkLevelsetCrossingsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		InLevelset.Bind(Initializer.ParameterMap, TEXT("InLevelset"));
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));
		BrickMapRW.Bind(Initializer.ParameterMap, TEXT("BrickMapRW"));
	}

	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << InLevelset;
		Ar << ActiveBricksList;
		Ar << BrickMapRW;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef BrickMapRWUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (BrickMapRW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, BrickMapRW.GetBaseIndex(), BrickMapRWUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InLevelsetRHI,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (InLevelset.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InLevelset.GetBaseIndex(), InLevelsetRHI);
		}
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( BrickMapRW.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, BrickMapRW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	FShaderResourceParameter InLevelset;
	FShaderResourceParameter ActiveBricksList;

	FShaderResourceParameter BrickMapRW;
};

class FPrepLevelsetSplatCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPrepLevelsetSplatCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FPrepLevelsetSplatCS()
	{
	}

	explicit FPrepLevelsetSplatCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		LiquidBoundary.Bind(Initializer.ParameterMap, TEXT("LiquidBoundary"));
#if LS_MULTIPLIER == 2
		LiquidBoundarySampler.Bind(Initializer.ParameterMap, TEXT("LiquidBoundarySampler"));
#endif
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));

		OutLiquidSurfaceUint32.Bind(Initializer.ParameterMap, TEXT("OutLiquidSurfaceUint32"));

	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << LiquidBoundary;
#if LS_MULTIPLIER == 2
		Ar << LiquidBoundarySampler;
#endif
		Ar << ActiveBricksList;
		Ar << OutLiquidSurfaceUint32;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutLiquidSurfaceUint32UAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutLiquidSurfaceUint32.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidSurfaceUint32.GetBaseIndex(), OutLiquidSurfaceUint32UAV);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef LiquidBoundaryRHI,
		FUniformBufferRHIParamRef SurfaceSculptingUniformBuffer,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FSurfaceSculptingUniformParameters>(), SurfaceSculptingUniformBuffer);

#if LS_MULTIPLIER == 2
		if (LiquidBoundary.IsBound())
		{
			FSamplerStateRHIParamRef SamplerStateTrilinear = TStaticSamplerState<SF_Trilinear>::GetRHI();
			SetTextureParameter(RHICmdList, ComputeShaderRHI, LiquidBoundary, LiquidBoundarySampler, SamplerStateTrilinear, LiquidBoundaryRHI);
		}
#else

		if (LiquidBoundary.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, LiquidBoundary.GetBaseIndex(), LiquidBoundaryRHI);
		}
#endif
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutLiquidSurfaceUint32.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidSurfaceUint32.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
#if LS_MULTIPLIER == 2
	FShaderResourceParameter LiquidBoundarySampler;
#endif
	FShaderResourceParameter LiquidBoundary;
	FShaderResourceParameter ActiveBricksList;

	FShaderResourceParameter OutLiquidSurfaceUint32;
};

class FPressureJacobiCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPressureJacobiCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FPressureJacobiCS()
	{
	}

	explicit FPressureJacobiCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		InPressure.Bind(Initializer.ParameterMap, TEXT("InPressure"));

		InPressureSolverParams.Bind(Initializer.ParameterMap, TEXT("InPressureSolverParams"));
		InVoxels.Bind(Initializer.ParameterMap, TEXT("InVoxels"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));

		OutPressure.Bind(Initializer.ParameterMap, TEXT("OutPressure"));
	}

	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << InPressure;
		Ar << InPressureSolverParams;
		Ar << InVoxels;
		Ar << InCount;
		Ar << OutPressure;
		return bShaderHasOutdatedParameters;
	}
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutPressureUAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutPressure.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutPressure.GetBaseIndex(), OutPressureUAV);
	}
	// per iteration input
	void SetInput(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InPressureRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InPressure.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InPressure.GetBaseIndex(), InPressureRHI);
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef InPressureSolverParamsRHI,
		FShaderResourceViewRHIParamRef InVoxelsRHI,
		FShaderResourceViewRHIParamRef InCountRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InPressureSolverParams.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPressureSolverParams.GetBaseIndex(), InPressureSolverParamsRHI);
		if (InVoxels.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InVoxels.GetBaseIndex(), InVoxelsRHI);
		if (InCount.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutPressure.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutPressure.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}
	void UnbindSRV(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InPressureSolverParams.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPressureSolverParams.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InVoxels.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InVoxels.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InCount.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
	}

private:
	FShaderResourceParameter InPressure;

	FShaderResourceParameter InPressureSolverParams;
	FShaderResourceParameter InVoxels;
	FShaderResourceParameter InCount;

	FShaderResourceParameter OutPressure;
};

class FProjectVelocityCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FProjectVelocityCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FProjectVelocityCS()
	{
	}

	/** Initialization constructor. */
	explicit FProjectVelocityCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Input
		LiquidBoundary.Bind(Initializer.ParameterMap, TEXT("LiquidBoundary"));
		Pressure.Bind(Initializer.ParameterMap, TEXT("Pressure"));
		Weights.Bind(Initializer.ParameterMap, TEXT("Weights"));
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));

		// Output
		OutU.Bind(Initializer.ParameterMap, TEXT("OutU"));
		OutV.Bind(Initializer.ParameterMap, TEXT("OutV"));
		OutW.Bind(Initializer.ParameterMap, TEXT("OutW"));
		OutValidFlags.Bind(Initializer.ParameterMap, TEXT("OutValidFlags"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LiquidBoundary;
		Ar << Pressure;
		Ar << Weights;
		Ar << ActiveBricksList;
		Ar << OutU;
		Ar << OutV;
		Ar << OutW;
		Ar << OutValidFlags;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutUUAV,
		FUnorderedAccessViewRHIParamRef OutVUAV,
		FUnorderedAccessViewRHIParamRef OutWUAV,
		FUnorderedAccessViewRHIParamRef OutValidFlagsUAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutU.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutU.GetBaseIndex(), OutUUAV);
		if (OutV.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutV.GetBaseIndex(), OutVUAV);
		if (OutW.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutW.GetBaseIndex(), OutWUAV);
		if (OutValidFlags.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValidFlags.GetBaseIndex(), OutValidFlagsUAV);
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef LiquidBoundaryRHI,
		FTexture3DRHIParamRef PressureRHI,
		FTexture3DRHIParamRef WeightsRHI,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);

		if (LiquidBoundary.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, LiquidBoundary.GetBaseIndex(), LiquidBoundaryRHI);
		if (Pressure.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, Pressure.GetBaseIndex(), PressureRHI);
		}
		if (Weights.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, Weights.GetBaseIndex(), WeightsRHI);
		}
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}

	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutU.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutU.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		if (OutV.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		if (OutW.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		if (OutValidFlags.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValidFlags.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}

private:
	FShaderResourceParameter LiquidBoundary;
	FShaderResourceParameter Pressure;
	FShaderResourceParameter Weights;
	FShaderResourceParameter ActiveBricksList;

	FShaderResourceParameter OutU;
	FShaderResourceParameter OutV;
	FShaderResourceParameter OutW;
	FShaderResourceParameter OutValidFlags;
};

/**
* Move the levelset toward a smooth signed distance field where length( grad(phi)) == 1.
*/
template<int32 Mode>
class FRedistanceLevelsetCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRedistanceLevelsetCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GRID_IS_LEVELSET"), (uint32)((Mode & RL_Levelset) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("INPUT_IS_UINT"), (uint32)((Mode & RL_InUint) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("OUTPUT_IS_UINT"), (uint32)((Mode & RL_OutUint) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("USE_SURFACE_OFFSET"), (uint32)((Mode & RL_IgnoreOffset) ? 0 : 1));
//#define CRUDE_SCHEME 0
//#define GODUNOVS_SCHEME 1
//#define ENGQUIST_OSHER_SCHEME 2
		OutEnvironment.SetDefine(TEXT("UPWIND_SCHEME"), (uint32)((Mode & RL_UseCrude) ? 0 : 1));
	}

	FRedistanceLevelsetCS()
	{
	}

	explicit FRedistanceLevelsetCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Input
		InLevelset.Bind(Initializer.ParameterMap, TEXT("InLevelset"));
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));

		// Output
		OutLevelset.Bind(Initializer.ParameterMap, TEXT("OutLevelset"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InLevelset;
		Ar << ActiveBricksList;
		Ar << OutLevelset;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutLevelsetUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutLevelset.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLevelset.GetBaseIndex(), OutLevelsetUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InLevelsetRHI,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (InLevelset.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InLevelset.GetBaseIndex(), InLevelsetRHI);
		}
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}

	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutLevelset.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLevelset.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter InLevelset;
	FShaderResourceParameter ActiveBricksList;

	FShaderResourceParameter OutLevelset;
};

class FNormalizeVelocityAndSplatFieldsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FNormalizeVelocityAndSplatFieldsCS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FNormalizeVelocityAndSplatFieldsCS()
	{
	}

	explicit FNormalizeVelocityAndSplatFieldsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));
		InSplatWeightU.Bind(Initializer.ParameterMap, TEXT("InSplatWeightU"));
		InSplatWeightV.Bind(Initializer.ParameterMap, TEXT("InSplatWeightV"));
		InSplatWeightW.Bind(Initializer.ParameterMap, TEXT("InSplatWeightW"));
		OutSplatU.Bind(Initializer.ParameterMap, TEXT("OutSplatU"));
		OutSplatV.Bind(Initializer.ParameterMap, TEXT("OutSplatV"));
		OutSplatW.Bind(Initializer.ParameterMap, TEXT("OutSplatW"));
		OutValidFlags.Bind(Initializer.ParameterMap, TEXT("OutValidFlags"));
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << ActiveBricksList;
		Ar << InSplatWeightU;
		Ar << InSplatWeightV;
		Ar << InSplatWeightW;
		Ar << OutSplatU;
		Ar << OutSplatV;
		Ar << OutSplatW;
		Ar << OutValidFlags;

		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutSplatUUAV,
		FUnorderedAccessViewRHIParamRef OutSplatVUAV,
		FUnorderedAccessViewRHIParamRef OutSplatWUAV,
		FUnorderedAccessViewRHIParamRef OutValidFlagsUAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutSplatU.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSplatU.GetBaseIndex(), OutSplatUUAV);
		}
		if (OutSplatV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSplatV.GetBaseIndex(), OutSplatVUAV);
		}
		if (OutSplatW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSplatW.GetBaseIndex(), OutSplatWUAV);
		}
		if (OutValidFlags.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValidFlags.GetBaseIndex(), OutValidFlagsUAV);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InSplatVelRHI,
		FTexture3DRHIParamRef InSplatWeightRHI,
		FTexture3DRHIParamRef InSplatWeightWRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);

		if (InSplatWeightU.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InSplatWeightU.GetBaseIndex(), InSplatVelRHI);
		}
		if (InSplatWeightV.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InSplatWeightV.GetBaseIndex(), InSplatWeightRHI);
		}
		if (InSplatWeightW.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InSplatWeightW.GetBaseIndex(), InSplatWeightWRHI);
		}
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutSplatU.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSplatU.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutSplatV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSplatV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutSplatW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSplatW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutValidFlags.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValidFlags.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter ActiveBricksList;
	FShaderResourceParameter InSplatWeightU;
	FShaderResourceParameter InSplatWeightV;
	FShaderResourceParameter InSplatWeightW;
	FShaderResourceParameter OutSplatU;
	FShaderResourceParameter OutSplatV;
	FShaderResourceParameter OutSplatW;
	FShaderResourceParameter OutValidFlags;
};

template <uint16 dataMode>
class FZeroBricksCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FZeroBricksCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine(TEXT("ZERO_UNMAPPED_ONLY"), (uint32)((dataMode & ZBD_unmapped) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("IS_FLOAT"), (uint32)((dataMode & FSparseGrid::TDT_FLOAT) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("IS_FLOAT2"), (uint32)((dataMode & FSparseGrid::TDT_FLOAT2) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("IS_FLOAT4"), (uint32)((dataMode & FSparseGrid::TDT_FLOAT4) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("IS_UINT"), (uint32)((dataMode & FSparseGrid::TDT_UINT) ? 1 : 0));
	}

	FZeroBricksCS()
	{
	}

	/** Initialization constructor. */
	explicit FZeroBricksCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		// Input
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));

		// Output
		OutData.Bind(Initializer.ParameterMap, TEXT("OutData"));
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << ActiveBricksList;
		Ar << OutData;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutDataUAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutData.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutData.GetBaseIndex(), OutDataUAV);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}

	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		
		if ( ActiveBricksList.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutData.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutData.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	FShaderResourceParameter ActiveBricksList;

	FShaderResourceParameter OutData;
};

template <uint16 dataMode>
class FZeroLevelsetBricksCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FZeroLevelsetBricksCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ZERO_UNMAPPED_ONLY"), (uint32)((dataMode & ZBD_unmapped) ? 1 : 0));
	}

	FZeroLevelsetBricksCS()
	{
	}

	/** Initialization constructor. */
	explicit FZeroLevelsetBricksCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Input
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));

		// Output
		OutFloat0.Bind(Initializer.ParameterMap, TEXT("OutFloat0"));
		OutUint0.Bind(Initializer.ParameterMap, TEXT("OutUint0"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ActiveBricksList;
		Ar << OutFloat0;
		Ar << OutUint0;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutFloat0UAV,
		FUnorderedAccessViewRHIParamRef OutUint0UAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutFloat0.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFloat0.GetBaseIndex(), OutFloat0UAV);
		}
		if (OutUint0.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutUint0.GetBaseIndex(), OutUint0UAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}

	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutFloat0.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFloat0.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutUint0.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutUint0.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	FShaderResourceParameter ActiveBricksList;

	FShaderResourceParameter OutFloat0;
	FShaderResourceParameter OutUint0;
};

// Get Brick Data
template <uint16 dataMode>
class FGetBricksDataCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGetBricksDataCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("IS_FLOAT"), (uint32)((dataMode & FSparseGrid::TDT_FLOAT) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("IS_FLOAT2"), (uint32)((dataMode & FSparseGrid::TDT_FLOAT2) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("IS_FLOAT4"), (uint32)((dataMode & FSparseGrid::TDT_FLOAT4) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("IS_UINT"), (uint32)((dataMode & FSparseGrid::TDT_UINT) ? 1 : 0));

		OutEnvironment.SetDefine(TEXT("IS_LEVELSET"), (uint32)((dataMode == 0) ? 1 : 0));
	}

	FGetBricksDataCS()
	{
	}

	/** Initialization constructor. */
	explicit FGetBricksDataCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BricksIdListBuffer.Bind(Initializer.ParameterMap, TEXT("BricksIdList"));
		InputVTR.Bind(Initializer.ParameterMap, TEXT("InputVTR"));;
		OutBricksDataBuffer.Bind(Initializer.ParameterMap, TEXT("OutBricksData"));;
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BricksIdListBuffer;
		Ar << InputVTR;
		Ar << OutBricksDataBuffer;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutBricksDataUAVRef)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutBricksDataBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutBricksDataBuffer.GetBaseIndex(), OutBricksDataUAVRef, 0);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FUniformBufferRHIParamRef BrickDebuggerUniformBuffer,
		FShaderResourceViewRHIParamRef BrickIdListBufRHI,
		FTexture3DRHIParamRef VTRTextureRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FBrickDataDebuggerUniformParameters>(), BrickDebuggerUniformBuffer);
		if (BricksIdListBuffer.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, BricksIdListBuffer.GetBaseIndex(), BrickIdListBufRHI);
		}
		if (InputVTR.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InputVTR.GetBaseIndex(), VTRTextureRHI);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (BricksIdListBuffer.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, BricksIdListBuffer.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutBricksDataBuffer.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, OutBricksDataBuffer.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

private:

	/** Input buffer. */
	FShaderResourceParameter BricksIdListBuffer;
	FShaderResourceParameter InputVTR;

	/** Output buffers. */
	FShaderResourceParameter OutBricksDataBuffer;
};

// Trigger
class FTriggerOverlapCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTriggerOverlapCS,Global);

public:
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	/** Default constructor. */
	FTriggerOverlapCS()
	{
	}

	/** Initialization constructor. */
	explicit FTriggerOverlapCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		// Input
		LiquidSurface.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTexture"));
		LiquidSurfaceSampler.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTextureSampler"));
		OverlapState.Bind(Initializer.ParameterMap, TEXT("OverlapState"));
		// Output
		OutOverlapState.Bind(Initializer.ParameterMap, TEXT("OutOverlapState"));
		OutChangedOverlapState.Bind(Initializer.ParameterMap, TEXT("OutChangedOverlapState"));
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << LiquidSurface;
		Ar << LiquidSurfaceSampler;
		Ar << OverlapState;
		Ar << OutOverlapState;
		Ar << OutChangedOverlapState;

		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutOverlapStateUAV,
		FUnorderedAccessViewRHIParamRef OutChangedOverlapStateUAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutOverlapState.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutOverlapState.GetBaseIndex(), OutOverlapStateUAV);
		}

		if (OutChangedOverlapState.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutChangedOverlapState.GetBaseIndex(), OutChangedOverlapStateUAV, 0);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef LiquidSurfaceRHI,
		FShaderResourceViewRHIParamRef OverlapStateRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer,
		FUniformBufferRHIParamRef TriggerUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FFluidTriggerUniformParameters>(), TriggerUniformBuffer);

		if (LiquidSurface.IsBound())
		{
			FSamplerStateRHIParamRef SamplerStateTrilinear = TStaticSamplerState<SF_Trilinear>::GetRHI();
			SetTextureParameter(RHICmdList, ComputeShaderRHI, LiquidSurface, LiquidSurfaceSampler, SamplerStateTrilinear, LiquidSurfaceRHI);
		}

		if (OverlapState.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, OverlapState.GetBaseIndex(), OverlapStateRHI);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		
		if ( OutOverlapState.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutOverlapState.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}

		if ( OutChangedOverlapState.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutChangedOverlapState.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}

		if (OverlapState.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, OverlapState.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter LiquidSurfaceSampler;
	FShaderResourceParameter LiquidSurface;
	FShaderResourceParameter OverlapState;

	FShaderResourceParameter OutOverlapState;
	FShaderResourceParameter OutChangedOverlapState;
};

template <uint32 pass>
class FRadixSortCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRadixSortCS, Global);

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

	FRadixSortCS()
	{
	}

	explicit FRadixSortCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SrcKeys.Bind(Initializer.ParameterMap, TEXT("gSrc"));
		SrcValues.Bind(Initializer.ParameterMap, TEXT("gSrcVal"));
		DstKeys.Bind(Initializer.ParameterMap, TEXT("gDst"));
		DstValues.Bind(Initializer.ParameterMap, TEXT("gDstVal"));

		HistogramIn.Bind(Initializer.ParameterMap, TEXT("HistogramIn"));
		HistogramOut.Bind(Initializer.ParameterMap, TEXT("HistogramOut"));

		NVExtensionUAV.Bind(Initializer.ParameterMap, TEXT("g_NvidiaExt"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SrcKeys;
		Ar << SrcValues;
		Ar << DstKeys;
		Ar << DstValues;
		Ar << HistogramIn;
		Ar << HistogramOut;
		Ar << NVExtensionUAV;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef DstKeysUAV,
		FUnorderedAccessViewRHIParamRef DstValuesUAV,
		FUnorderedAccessViewRHIParamRef HistogramOutUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (DstKeys.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, DstKeys.GetBaseIndex(), DstKeysUAV);
		}
		if (DstValues.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, DstValues.GetBaseIndex(), DstValuesUAV);
		}
		if (HistogramOut.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, HistogramOut.GetBaseIndex(), HistogramOutUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef SrcKeysSRV,
		FShaderResourceViewRHIParamRef SrcValuesSRV,
		FShaderResourceViewRHIParamRef HistogramInSRV,
		const FLocalUniformBuffer& RadixSortUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetLocalUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FRadixSortUniformParameters>(), RadixSortUniformBuffer);

		if (SrcKeys.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SrcKeys.GetBaseIndex(), SrcKeysSRV);
		}
		if (SrcValues.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SrcValues.GetBaseIndex(), SrcValuesSRV);
		}
		if (HistogramIn.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, HistogramIn.GetBaseIndex(), HistogramInSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (SrcKeys.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SrcKeys.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (SrcValues.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SrcValues.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (HistogramIn.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, HistogramIn.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}

		if (DstKeys.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, DstKeys.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (DstValues.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, DstValues.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (HistogramOut.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, HistogramOut.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

	virtual int GetNvShaderExtnSlot() override
	{
		return NVExtensionSlot;
	}

private:
	FShaderResourceParameter SrcKeys;
	FShaderResourceParameter SrcValues;
	FShaderResourceParameter DstKeys;
	FShaderResourceParameter DstValues;

	FShaderResourceParameter HistogramIn;
	FShaderResourceParameter HistogramOut;

	/** For using the nv extensions. */
	FShaderResourceParameter NVExtensionUAV;
};

class FGenerateKeysAndValuesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGenerateKeysAndValuesCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FGenerateKeysAndValuesCS()
	{
	}

	/** Initialization constructor. */
	explicit FGenerateKeysAndValuesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// The GPU particles.
		InParticleIndices.Bind(Initializer.ParameterMap, TEXT("InParticleIndices"));
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));

		OutKeys.Bind(Initializer.ParameterMap, TEXT("OutKeys"));
		OutValues.Bind(Initializer.ParameterMap, TEXT("OutValues"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InParticleIndices;
		Ar << PositionTexture;
		Ar << OutKeys;
		Ar << OutValues;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutKeysUAV,
		FUnorderedAccessViewRHIParamRef OutValuesUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutKeys.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutKeys.GetBaseIndex(), OutKeysUAV);
		}
		if (OutValues.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValues.GetBaseIndex(), OutValuesUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture2DRHIParamRef PositionTextureRHI,
		FShaderResourceViewRHIParamRef InIndicesSRV,
		FUniformBufferRHIParamRef VoxelUniformBuffer,
		FUniformBufferRHIParamRef ParticleCountUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FParticleCountUniformParameters>(), ParticleCountUniformBuffer);

		if (PositionTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
		if (InParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), InIndicesSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutKeys.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutKeys.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutValues.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValues.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	/** Input buffer containing particle indices. */
	FShaderResourceParameter InParticleIndices;
	/** The particle position texture parameter. */
	FShaderResourceParameter PositionTexture;

	/** Output keys and values. */
	FShaderResourceParameter OutKeys;
	FShaderResourceParameter OutValues;
};

class FGenerateCellRangesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGenerateCellRangesCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FGenerateCellRangesCS()
	{
	}

	/** Initialization constructor. */
	explicit FGenerateCellRangesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SortedKeys.Bind(Initializer.ParameterMap, TEXT("SortedKeys"));
		OutFirstParticleInVoxel.Bind(Initializer.ParameterMap, TEXT("OutFirstParticleInVoxel"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SortedKeys;
		Ar << OutFirstParticleInVoxel;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutFirstParticleInVoxelUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutFirstParticleInVoxel.IsBound())
		{
			// note 0 here sets the count to 0 for the append buffer.
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFirstParticleInVoxel.GetBaseIndex(), OutFirstParticleInVoxelUAV, 0);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef SortedKeysSRV,
		FUniformBufferRHIParamRef ParticleCountUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FParticleCountUniformParameters>(), ParticleCountUniformBuffer);

		if (SortedKeys.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedKeys.GetBaseIndex(), SortedKeysSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (SortedKeys.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedKeys.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutFirstParticleInVoxel.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFirstParticleInVoxel.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter SortedKeys;

	FShaderResourceParameter OutFirstParticleInVoxel;
	FShaderResourceParameter OutFirstParticleInSubVoxel;
};

class FConstrainGetFacesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FConstrainGetFacesCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GET_FACES_TO_CONSTRAIN"), 1);
	}

	FConstrainGetFacesCS()
	{
	}

	explicit FConstrainGetFacesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));
		InWeights.Bind(Initializer.ParameterMap, TEXT("InWeights"));
		InValidFlags.Bind(Initializer.ParameterMap, TEXT("InValidFlags"));
		OutFaces.Bind(Initializer.ParameterMap, TEXT("OutFaces"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ActiveBricksList;
		Ar << InWeights;
		Ar << InValidFlags;
		Ar << OutFaces;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutFacesUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutFaces.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFaces.GetBaseIndex(), OutFacesUAV, 0);
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InValidFlagsRHI,
		FTexture3DRHIParamRef InWeightsRHI,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InValidFlags.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InValidFlags.GetBaseIndex(), InValidFlagsRHI);
		if (InWeights.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InWeights.GetBaseIndex(), InWeightsRHI);
		if (ActiveBricksList.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (ActiveBricksList.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (OutFaces.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFaces.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}

private:
	FShaderResourceParameter ActiveBricksList;
	FShaderResourceParameter InWeights;
	FShaderResourceParameter InValidFlags;
	FShaderResourceParameter OutFaces;
};

class FConstrainFillVelocityBufferCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FConstrainFillVelocityBufferCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("FILL_VELOCITY_BUFFER"), 1);
	}

	FConstrainFillVelocityBufferCS()
	{
	}

	explicit FConstrainFillVelocityBufferCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InFaces.Bind(Initializer.ParameterMap, TEXT("InFaces"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		InU.Bind(Initializer.ParameterMap, TEXT("InU"));
		InV.Bind(Initializer.ParameterMap, TEXT("InV"));
		InW.Bind(Initializer.ParameterMap, TEXT("InW"));
		InValidFlags.Bind(Initializer.ParameterMap, TEXT("InValidFlags"));
		OutConstrainVelocityParams.Bind(Initializer.ParameterMap, TEXT("OutConstrainVelocityParams"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InFaces;
		Ar << InCount;
		Ar << InU;
		Ar << InV;
		Ar << InW;
		Ar << InValidFlags;
		Ar << OutConstrainVelocityParams;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutConstrainVelocityParamsUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutConstrainVelocityParams.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutConstrainVelocityParams.GetBaseIndex(), OutConstrainVelocityParamsUAV);
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef InFacesRHI,
		FShaderResourceViewRHIParamRef InCountRHI,
		FTexture3DRHIParamRef InURHI,
		FTexture3DRHIParamRef InVRHI,
		FTexture3DRHIParamRef InWRHI,
		FTexture3DRHIParamRef InValidFlagsRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InFaces.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InFaces.GetBaseIndex(), InFacesRHI);
		if (InCount.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		if (InU.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InU.GetBaseIndex(), InURHI);
		if (InV.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InV.GetBaseIndex(), InVRHI);
		if (InW.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InW.GetBaseIndex(), InWRHI);
		if (InValidFlags.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InValidFlags.GetBaseIndex(), InValidFlagsRHI);
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InFaces.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InFaces.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InCount.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (OutConstrainVelocityParams.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutConstrainVelocityParams.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}

private:
	FShaderResourceParameter InFaces;
	FShaderResourceParameter InCount;

	FShaderResourceParameter InU;
	FShaderResourceParameter InV;
	FShaderResourceParameter InW;
	FShaderResourceParameter InValidFlags;

	FShaderResourceParameter OutConstrainVelocityParams;
};

class FConstrainScalarVelocityWithSolidBoundaryCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FConstrainScalarVelocityWithSolidBoundaryCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CONSTRAIN_SCALAR_VELOCITY"), 1);
	}

	FConstrainScalarVelocityWithSolidBoundaryCS()
	{
	}

	explicit FConstrainScalarVelocityWithSolidBoundaryCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InConstrainVelocityParams.Bind(Initializer.ParameterMap, TEXT("InConstrainVelocityParams"));
		InFaces.Bind(Initializer.ParameterMap, TEXT("InFaces"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		InSolidBoundary.Bind(Initializer.ParameterMap, TEXT("InSolidBoundary"));
		OutU.Bind(Initializer.ParameterMap, TEXT("OutU"));
		OutV.Bind(Initializer.ParameterMap, TEXT("OutV"));
		OutW.Bind(Initializer.ParameterMap, TEXT("OutW"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InConstrainVelocityParams;
		Ar << InFaces;
		Ar << InCount;
		Ar << InSolidBoundary;
		Ar << OutU;
		Ar << OutV;
		Ar << OutW;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutUUAV,
		FUnorderedAccessViewRHIParamRef OutVUAV,
		FUnorderedAccessViewRHIParamRef OutWUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutU.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutU.GetBaseIndex(), OutUUAV);
		if (OutV.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutV.GetBaseIndex(), OutVUAV);
		if (OutW.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutW.GetBaseIndex(), OutWUAV);
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef InConstrainVelocityParamsRHI,
		FShaderResourceViewRHIParamRef InFacesRHI,
		FShaderResourceViewRHIParamRef InCountRHI,
		FTexture3DRHIParamRef InSolidBoundaryRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InConstrainVelocityParams.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InConstrainVelocityParams.GetBaseIndex(), InConstrainVelocityParamsRHI);
		if (InFaces.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InFaces.GetBaseIndex(), InFacesRHI);
		if (InCount.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		if (InSolidBoundary.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InSolidBoundary.GetBaseIndex(), InSolidBoundaryRHI);
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InConstrainVelocityParams.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InConstrainVelocityParams.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InFaces.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InFaces.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InCount.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (OutU.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutU.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		if (OutV.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		if (OutW.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}

private:
	FShaderResourceParameter InConstrainVelocityParams;
	FShaderResourceParameter InFaces;
	FShaderResourceParameter InCount;

	FShaderResourceParameter InSolidBoundary;
	FShaderResourceParameter OutU;
	FShaderResourceParameter OutV;
	FShaderResourceParameter OutW;
};

template <uint32 ValidFlag>
class FExtendScalarVelocityCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FExtendScalarVelocityCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VALID_FLAG"), ValidFlag);
	}

	FExtendScalarVelocityCS()
	{
	}

	explicit FExtendScalarVelocityCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));
		InFloat.Bind(Initializer.ParameterMap, TEXT("InFloat"));
		InValidFlags.Bind(Initializer.ParameterMap, TEXT("InValidFlags"));
		OutFloat.Bind(Initializer.ParameterMap, TEXT("OutFloat"));
		OutValidFlags.Bind(Initializer.ParameterMap, TEXT("OutValidFlags"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ActiveBricksList;
		Ar << InFloat;
		Ar << InValidFlags;
		Ar << OutFloat;
		Ar << OutValidFlags;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutFloatUAV,
		FUnorderedAccessViewRHIParamRef OutValidFlagsUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutFloat.IsBound())	RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFloat.GetBaseIndex(), OutFloatUAV);
		if (OutValidFlags.IsBound())	RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValidFlags.GetBaseIndex(), OutValidFlagsUAV);
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InFloatRHI,
		FTexture3DRHIParamRef InValidFlagsRHI,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InFloat.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InFloat.GetBaseIndex(), InFloatRHI);
		if (InValidFlags.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, InValidFlags.GetBaseIndex(), InValidFlagsRHI);
		if (ActiveBricksList.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (ActiveBricksList.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (OutFloat.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFloat.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		if (OutValidFlags.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutValidFlags.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}

private:
	FShaderResourceParameter ActiveBricksList;
	FShaderResourceParameter InFloat;
	FShaderResourceParameter InValidFlags;
	FShaderResourceParameter OutFloat;
	FShaderResourceParameter OutValidFlags;
};

template<bool UseAniso>
class FParticlesToGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticlesToGridCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_ANISO"), (uint32)(UseAniso ? 1 : 0));
	}

	FParticlesToGridCS()
	{
	}

	explicit FParticlesToGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ParticleVoxels.Bind(Initializer.ParameterMap, TEXT("ParticleVoxels"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		SortedParticleIndices.Bind(Initializer.ParameterMap, TEXT("SortedParticleIndices"));
		InPosInVox.Bind(Initializer.ParameterMap, TEXT("InPosInVox"));
		InSmoothDensity.Bind(Initializer.ParameterMap, TEXT("InSmoothDensity"));
		InSmoothDensitySampler.Bind(Initializer.ParameterMap, TEXT("InSmoothDensitySampler"));
		OutUint32.Bind(Initializer.ParameterMap, TEXT("OutUint32"));

	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ParticleVoxels;
		Ar << InCount;
		Ar << VelocityTexture;
		Ar << SortedParticleIndices;
		Ar << InPosInVox;
		Ar << InSmoothDensity;
		Ar << InSmoothDensitySampler;
		Ar << OutUint32;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutUint32UAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutUint32.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutUint32.GetBaseIndex(), OutUint32UAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ParticleVoxelsRHI,
		FShaderResourceViewRHIParamRef InCountRHI,
		FTexture2DRHIParamRef VelocityTextureRHI,
		FShaderResourceViewRHIParamRef SortedParticleIndicesSRV,
		FShaderResourceViewRHIParamRef InPosInVoxSRV,
		FUniformBufferRHIParamRef SurfaceSculptingUniformBuffer,
		FUniformBufferRHIParamRef VoxelUniformBuffer,
		FTexture3DRHIParamRef InSmoothDensityRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FSurfaceSculptingUniformParameters>(), SurfaceSculptingUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);


		if (SortedParticleIndices.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), SortedParticleIndicesSRV);
		if (VelocityTexture.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, VelocityTexture.GetBaseIndex(), VelocityTextureRHI);
		if (InPosInVox.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPosInVox.GetBaseIndex(), InPosInVoxSRV);
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), ParticleVoxelsRHI);
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		}
		if (InSmoothDensity.IsBound())
		{
			FSamplerStateRHIParamRef SamplerStateTrilinear = TStaticSamplerState<SF_Trilinear>::GetRHI();
			SetTextureParameter(RHICmdList, ComputeShaderRHI, InSmoothDensity, InSmoothDensitySampler, SamplerStateTrilinear, InSmoothDensityRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (SortedParticleIndices.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InPosInVox.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPosInVox.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutUint32.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutUint32.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter ParticleVoxels;
	FShaderResourceParameter InCount;

	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter SortedParticleIndices;
	FShaderResourceParameter InPosInVox;

	FShaderResourceParameter InSmoothDensitySampler;
	FShaderResourceParameter InSmoothDensity;

	FShaderResourceParameter OutUint32;
};

class FPrepareFFTCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPrepareFFTCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPrepareFFTCS()
	{
	}

	/** Initialization constructor. */
	explicit FPrepareFFTCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InSpectrum.Bind(Initializer.ParameterMap, TEXT("InSpectrum"));
		OutTexture.Bind(Initializer.ParameterMap, TEXT("OutTexture"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InSpectrum;
		Ar << OutTexture;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutTextureUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutTexture.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexture.GetBaseIndex(), OutTextureUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture2DRHIParamRef SpectrumTexture,
		const FLocalUniformBuffer& LocalUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetLocalUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FPrepareFFTUniformParameters>(), LocalUniformBuffer);

		if (InSpectrum.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InSpectrum.GetBaseIndex(), SpectrumTexture);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InSpectrum.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InSpectrum.GetBaseIndex(), FTexture2DRHIParamRef());
		}
		if (OutTexture.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexture.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter InSpectrum;
	FShaderResourceParameter OutTexture;
};

class FExecuteFFTCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FExecuteFFTCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FExecuteFFTCS()
	{
	}

	/** Initialization constructor. */
	explicit FExecuteFFTCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"));
		OutTexture.Bind(Initializer.ParameterMap, TEXT("OutTexture"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << OutTexture;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutTextureUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutTexture.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexture.GetBaseIndex(), OutTextureUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture2DRHIParamRef Texture,
		const FLocalUniformBuffer& LocalUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetLocalUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FExecuteFFTUniformParameters>(), LocalUniformBuffer);

		if (InTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InTexture.GetBaseIndex(), Texture);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InTexture.GetBaseIndex(), FTexture2DRHIParamRef());
		}
		if (OutTexture.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexture.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter OutTexture;
};


/**
* Compute shader used to cover spray particles
*/
class FCoverSprayParticleCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCoverSprayParticleCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FCoverSprayParticleCS()
	{
	}

	/** Initialization constructor. */
	explicit FCoverSprayParticleCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// The GPU particles.
		InVertexBuffer.Bind(Initializer.ParameterMap, TEXT("InVertexBuffer"));
		NewSprayParticles.Bind(Initializer.ParameterMap, TEXT("NewSprayParticles"));

		OutVertexBuffer.Bind(Initializer.ParameterMap, TEXT("OutVertexBuffer"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InVertexBuffer;
		Ar << OutVertexBuffer;
		Ar << NewSprayParticles;

		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutVetexBufferUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutVertexBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVertexBuffer.GetBaseIndex(), OutVetexBufferUAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef InVetexBufferSRV,
		FShaderResourceViewRHIParamRef InNewSprayParticlesSRV,
		FUniformBufferRHIParamRef SprayInfoUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FSprayParticleInfoUniformParameters>(), SprayInfoUniformBuffer);

		if (InVertexBuffer.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InVertexBuffer.GetBaseIndex(), InVetexBufferSRV);
		}
		if (NewSprayParticles.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, NewSprayParticles.GetBaseIndex(), InNewSprayParticlesSRV);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InVertexBuffer.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InVertexBuffer.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}

		if (OutVertexBuffer.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVertexBuffer.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}

		if (NewSprayParticles.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, NewSprayParticles.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

private:
	/** Input buffer containing particle vertex. */
	FShaderResourceParameter InVertexBuffer;
	/** Input buffer containing particle vertex. */
	FShaderResourceParameter OutVertexBuffer;

	FShaderResourceParameter NewSprayParticles;
};

/**
* The Velocity weigths are the fractions of the face which are inside the liquid.  Used for the
* Immersed boundary technique, a liquid fraction of 1 is all the way in the liquid, and 0 is all
* the way inside a boundary.
*/
class FComputeVelocityWeightsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeVelocityWeightsCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FComputeVelocityWeightsCS()
	{
	}

	explicit FComputeVelocityWeightsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));
		InSolidBoundary.Bind(Initializer.ParameterMap, TEXT("InSolidBoundary"));
		OutWeights.Bind(Initializer.ParameterMap, TEXT("OutWeights"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ActiveBricksList;
		Ar << InSolidBoundary;
		Ar << OutWeights;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutWeightsUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutWeights.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutWeights.GetBaseIndex(), OutWeightsUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InSolidBoundaryRHI,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (InSolidBoundary.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InSolidBoundary.GetBaseIndex(), InSolidBoundaryRHI);
		}
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutWeights.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutWeights.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter ActiveBricksList;
	FShaderResourceParameter InSolidBoundary;
	FShaderResourceParameter OutWeights;
};

/**
* Compute shader used to generate texcoords.
*/
class FGenerateTexCoordsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGenerateTexCoordsCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FGenerateTexCoordsCS()
	{
	}

	/** Initialization constructor. */
	explicit FGenerateTexCoordsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// The GPU particles.
		InParticleIndices.Bind(Initializer.ParameterMap, TEXT("InParticleIndices"));
		InExtraIndices.Bind(Initializer.ParameterMap, TEXT("InExtraIndices"));
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));

		OutTexCoord0.Bind(Initializer.ParameterMap, TEXT("OutTexCoord0"));
		OutTexCoord1.Bind(Initializer.ParameterMap, TEXT("OutTexCoord1"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InParticleIndices;
		Ar << InExtraIndices;
		Ar << PositionTexture;
		Ar << VelocityTexture;
		Ar << OutTexCoord0;
		Ar << OutTexCoord1;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutTexCoord0UAV,
		FUnorderedAccessViewRHIParamRef OutTexCoord1UAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutTexCoord0.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexCoord0.GetBaseIndex(), OutTexCoord0UAV);
		}
		if (OutTexCoord1.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexCoord1.GetBaseIndex(), OutTexCoord1UAV);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture2DRHIParamRef PositionTextureRHI,
		FTexture2DRHIParamRef VelocityTextureRHI,
		FShaderResourceViewRHIParamRef InExtraIndicesSRV,
		FShaderResourceViewRHIParamRef InParticleIndicesSRV,
		FUniformBufferRHIParamRef GenerateTexCoordsUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FGenerateTexCoordsUniformParameters>(), GenerateTexCoordsUniformBuffer);

		if (PositionTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
		if (VelocityTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, VelocityTexture.GetBaseIndex(), VelocityTextureRHI);
		}
		if (InParticleIndices.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), InParticleIndicesSRV);
		if (InExtraIndices.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InExtraIndices.GetBaseIndex(), InExtraIndicesSRV);
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutTexCoord0.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexCoord0.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutTexCoord1.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexCoord1.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	/** Input buffer containing particle indices. */
	FShaderResourceParameter InParticleIndices;
	FShaderResourceParameter InExtraIndices;
	/** The particle position texture parameter. */
	FShaderResourceParameter PositionTexture;
	/** The particle velocity texture parameter. */
	FShaderResourceParameter VelocityTexture;

	FShaderResourceParameter OutTexCoord0;
	FShaderResourceParameter OutTexCoord1;
};

template <uint32 GroupCount>
class FIndirectDispatchFillParameterCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FIndirectDispatchFillParameterCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GROUP_COUNT"), GroupCount);
	}

	FIndirectDispatchFillParameterCS()
	{
	}

	explicit FIndirectDispatchFillParameterCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		GroupMultiplier.Bind(Initializer.ParameterMap, TEXT("GroupMultiplier"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		OutParameters.Bind(Initializer.ParameterMap, TEXT("OutParameters"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << GroupMultiplier;
		Ar << InCount;
		Ar << OutParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutParametersUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutParameters.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParameters.GetBaseIndex(), OutParametersUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		float InGroupMultiplier,
		FShaderResourceViewRHIParamRef InCountRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetShaderValue(RHICmdList, ComputeShaderRHI, GroupMultiplier, InGroupMultiplier);

		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutParameters.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutParameters.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter InCount;
	FShaderResourceParameter OutParameters;
	FShaderParameter GroupMultiplier;
};

class FVelocityToGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVelocityToGridCS, Global);

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

	FVelocityToGridCS()
	{
	}

	explicit FVelocityToGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ParticleVoxels.Bind(Initializer.ParameterMap, TEXT("ParticleVoxels"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		SortedParticleIndices.Bind(Initializer.ParameterMap, TEXT("SortedParticleIndices"));
		InPosInVox.Bind(Initializer.ParameterMap, TEXT("InPosInVox"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		OutVelocity.Bind(Initializer.ParameterMap, TEXT("OutVelocity"));
		OutWeight.Bind(Initializer.ParameterMap, TEXT("OutWeight"));
		OutVelocityV.Bind(Initializer.ParameterMap, TEXT("OutVelocityV"));
		OutWeightV.Bind(Initializer.ParameterMap, TEXT("OutWeightV"));
		OutVelocityW.Bind(Initializer.ParameterMap, TEXT("OutVelocityW"));
		OutWeightW.Bind(Initializer.ParameterMap, TEXT("OutWeightW"));
		NVExtensionUAV.Bind(Initializer.ParameterMap, TEXT("g_NvidiaExt"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ParticleVoxels;
		Ar << InCount;
		Ar << SortedParticleIndices;
		Ar << InPosInVox;
		Ar << VelocityTexture;
		Ar << OutVelocity;
		Ar << OutWeight;
		Ar << OutVelocityV;
		Ar << OutWeightV;
		Ar << OutVelocityW;
		Ar << OutWeightW;
		Ar << NVExtensionUAV;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutVelocityUAV,
		FUnorderedAccessViewRHIParamRef OutWeightUAV,
		FUnorderedAccessViewRHIParamRef OutVelocityVUAV,
		FUnorderedAccessViewRHIParamRef OutWeightVUAV,
		FUnorderedAccessViewRHIParamRef OutVelocityWUAV,
		FUnorderedAccessViewRHIParamRef OutWeightWUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutVelocity.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVelocity.GetBaseIndex(), OutVelocityUAV);
		}
		if (OutWeight.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutWeight.GetBaseIndex(), OutWeightUAV);
		}
		if (OutVelocityV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVelocityV.GetBaseIndex(), OutVelocityVUAV);
		}
		if (OutWeightV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutWeightV.GetBaseIndex(), OutWeightVUAV);
		}
		if (OutVelocityW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVelocityW.GetBaseIndex(), OutVelocityWUAV);
		}
		if (OutWeightW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutWeightW.GetBaseIndex(), OutWeightWUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ParticleVoxelsRHI,
		FShaderResourceViewRHIParamRef InCountRHI,
		FShaderResourceViewRHIParamRef SortedParticleIndicesSRV,
		FShaderResourceViewRHIParamRef InPosInVoxSRV,
		FTexture2DRHIParamRef VelocityTextureRHI,
		FUniformBufferRHIParamRef SurfaceSculptingUniformBuffer,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FSurfaceSculptingUniformParameters>(), SurfaceSculptingUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);

		if (SortedParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), SortedParticleIndicesSRV);
		}
		if (InPosInVox.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPosInVox.GetBaseIndex(), InPosInVoxSRV);
		}
		if (VelocityTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, VelocityTexture.GetBaseIndex(), VelocityTextureRHI);
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), ParticleVoxelsRHI);
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (SortedParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (InPosInVox.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPosInVox.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutVelocity.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVelocity.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutWeight.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutWeight.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutVelocityV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVelocityV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutWeightV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutWeightV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutVelocityW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVelocityW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (OutWeightW.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutWeightW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

	virtual int GetNvShaderExtnSlot() override
	{
		return NVExtensionSlot;
	}

private:
	FShaderResourceParameter ParticleVoxels;
	FShaderResourceParameter InCount;
	FShaderResourceParameter SortedParticleIndices;

	FShaderResourceParameter InPosInVox;
	FShaderResourceParameter VelocityTexture;

	FShaderResourceParameter OutVelocity;
	FShaderResourceParameter OutWeight;
	FShaderResourceParameter OutVelocityV;
	FShaderResourceParameter OutWeightV;
	FShaderResourceParameter OutVelocityW;
	FShaderResourceParameter OutWeightW;
	FShaderResourceParameter NVExtensionUAV;
};

template<bool UseAniso>
class FDensityToGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDensityToGridCS, Global);

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
		OutEnvironment.SetDefine(TEXT("USE_ANISO"), (uint32)(UseAniso ? 1 : 0));
	}

	FDensityToGridCS()
	{
	}

	explicit FDensityToGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ParticleVoxels.Bind(Initializer.ParameterMap, TEXT("ParticleVoxels"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		SortedParticleIndices.Bind(Initializer.ParameterMap, TEXT("SortedParticleIndices"));
		InPosInVox.Bind(Initializer.ParameterMap, TEXT("InPosInVox"));
		OutSmoothDensity.Bind(Initializer.ParameterMap, TEXT("OutSmoothDensity"));

		NVExtensionUAV.Bind(Initializer.ParameterMap, TEXT("g_NvidiaExt"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ParticleVoxels;
		Ar << InCount;
		Ar << VelocityTexture;
		Ar << SortedParticleIndices;
		Ar << InPosInVox;
		Ar << OutSmoothDensity;
		Ar << NVExtensionUAV;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutSmoothDensityUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutSmoothDensity.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSmoothDensity.GetBaseIndex(), OutSmoothDensityUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ParticleVoxelsRHI,
		FShaderResourceViewRHIParamRef InCountRHI,
		FTexture2DRHIParamRef VelocityTextureRHI,
		FShaderResourceViewRHIParamRef SortedParticleIndicesSRV,
		FShaderResourceViewRHIParamRef InPosInVoxSRV,
		FUniformBufferRHIParamRef SurfaceSculptingUniformBuffer,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FSurfaceSculptingUniformParameters>(), SurfaceSculptingUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);


		if (SortedParticleIndices.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), SortedParticleIndicesSRV);
		if (VelocityTexture.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, VelocityTexture.GetBaseIndex(), VelocityTextureRHI);
		if (InPosInVox.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPosInVox.GetBaseIndex(), InPosInVoxSRV);
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), ParticleVoxelsRHI);
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (SortedParticleIndices.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (InPosInVox.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InPosInVox.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutSmoothDensity.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutSmoothDensity.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

	virtual int GetNvShaderExtnSlot() override
	{
		return NVExtensionSlot;
	}

private:
	FShaderResourceParameter ParticleVoxels;
	FShaderResourceParameter InCount;

	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter SortedParticleIndices;
	FShaderResourceParameter InPosInVox;

	FShaderResourceParameter OutSmoothDensity;

	FShaderResourceParameter NVExtensionUAV;
};

/**
* This shaders creates a levelset from a density field at a given value.
*/
class FAndoBoundaryCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FAndoBoundaryCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FAndoBoundaryCS()
	{
	}

	explicit FAndoBoundaryCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));
		InSmoothDensity.Bind(Initializer.ParameterMap, TEXT("InSmoothDensity"));
		OutLiquidBoundary.Bind(Initializer.ParameterMap, TEXT("OutLiquidBoundary"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ActiveBricksList;
		Ar << InSmoothDensity;
		Ar << OutLiquidBoundary;

		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutLiquidBoundaryUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (OutLiquidBoundary.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidBoundary.GetBaseIndex(), OutLiquidBoundaryUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InSmoothDensityRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);

		if (InSmoothDensity.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InSmoothDensity.GetBaseIndex(), InSmoothDensityRHI);
		}
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutLiquidBoundary.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidBoundary.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter ActiveBricksList;
	FShaderResourceParameter InSmoothDensity;

	FShaderResourceParameter OutLiquidBoundary;
};

/**
* When the splat kernel for particles is small, the sphere may not splat values outside itself
* to the grid.  In that case, the nearest postive value may be -SURFACE_LEVELSET_AT, instead of 
* a small positive value near the sphere.  This shader attempts to fix that problem.
*/
template<int32 Mode>
class FRepairLevelsetCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRepairLevelsetCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GRID_IS_LEVELSET"), (uint32)((Mode & RL_Levelset) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("INPUT_IS_UINT"), (uint32)((Mode & RL_InUint) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("OUTPUT_IS_UINT"), (uint32)((Mode & RL_OutUint) ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("USE_SURFACE_OFFSET"), (uint32)((Mode & RL_IgnoreOffset) ? 0 : 1));
	}

	FRepairLevelsetCS()
	{
	}

	explicit FRepairLevelsetCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Input
		InLevelset.Bind(Initializer.ParameterMap, TEXT("InLevelset"));
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));

		// Output
		OutLevelset.Bind(Initializer.ParameterMap, TEXT("OutLevelset"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InLevelset;
		Ar << ActiveBricksList;
		Ar << OutLevelset;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutLevelsetUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutLevelset.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLevelset.GetBaseIndex(), OutLevelsetUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef InLevelsetRHI,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (InLevelset.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, InLevelset.GetBaseIndex(), InLevelsetRHI);
		}
		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
		}

	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (ActiveBricksList.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (OutLevelset.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLevelset.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter InLevelset;
	FShaderResourceParameter ActiveBricksList;

	FShaderResourceParameter OutLevelset;
};

class FPosInVoxCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPosInVoxCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPosInVoxCS()
	{
	}

	explicit FPosInVoxCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ParticleVoxels.Bind(Initializer.ParameterMap, TEXT("ParticleVoxels"));
		InCount.Bind(Initializer.ParameterMap, TEXT("InCount"));
		SortedParticleIndices.Bind(Initializer.ParameterMap, TEXT("SortedParticleIndices"));
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));
		OutPosInVox.Bind(Initializer.ParameterMap, TEXT("OutPosInVox"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ParticleVoxels;
		Ar << InCount;
		Ar << SortedParticleIndices;
		Ar << PositionTexture;
		Ar << OutPosInVox;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutPosInVoxUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutPosInVox.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutPosInVox.GetBaseIndex(), OutPosInVoxUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ParticleVoxelsRHI,
		FShaderResourceViewRHIParamRef InCountRHI,
		FShaderResourceViewRHIParamRef SortedParticleIndicesSRV,
		FTexture2DRHIParamRef PositionTextureRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);
		
		if (SortedParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), SortedParticleIndicesSRV);
		}
		if (PositionTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), ParticleVoxelsRHI);
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), InCountRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (SortedParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SortedParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}

		if (OutPosInVox.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutPosInVox.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (ParticleVoxels.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleVoxels.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (InCount.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InCount.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter ParticleVoxels;
	FShaderResourceParameter InCount;
	FShaderResourceParameter SortedParticleIndices;
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter OutPosInVox;
};

class FVelAtPosCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVelAtPosCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FVelAtPosCS()
	{
	}

	explicit FVelAtPosCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InU.Bind(Initializer.ParameterMap, TEXT("InU"));
		InV.Bind(Initializer.ParameterMap, TEXT("InV"));
		InW.Bind(Initializer.ParameterMap, TEXT("InW"));
		LiquidBoundary.Bind(Initializer.ParameterMap, TEXT("LiquidBoundary"));
		InUSampler.Bind(Initializer.ParameterMap, TEXT("InUSampler"));
		InVSampler.Bind(Initializer.ParameterMap, TEXT("InVSampler"));
		InWSampler.Bind(Initializer.ParameterMap, TEXT("InWSampler"));
		LiquidBoundarySampler.Bind(Initializer.ParameterMap, TEXT("LiquidBoundarySampler"));
		ParticleIndices.Bind(Initializer.ParameterMap, TEXT("ParticleIndices"));
		ExtraParticleIndices.Bind(Initializer.ParameterMap, TEXT("ExtraParticleIndices"));
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));
		OutVelAtPos.Bind(Initializer.ParameterMap, TEXT("OutVelAtPos"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InU;
		Ar << InV;
		Ar << InW;
		Ar << LiquidBoundary;
		Ar << InUSampler;
		Ar << InVSampler;
		Ar << InWSampler;
		Ar << LiquidBoundarySampler;
		Ar << ParticleIndices;
		Ar << ExtraParticleIndices;
		Ar << PositionTexture;
		Ar << OutVelAtPos;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutVelAtPosUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutVelAtPos.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVelAtPos.GetBaseIndex(), OutVelAtPosUAV);
		}
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FShaderResourceViewRHIParamRef ParticleIndicesRHI,
		FShaderResourceViewRHIParamRef ExtraParticleIndicesRHI,
		FTexture3DRHIParamRef InURHI,
		FTexture3DRHIParamRef InVRHI,
		FTexture3DRHIParamRef InWRHI,
		FTexture3DRHIParamRef LiquidBoundaryRHI,
		FTexture2DRHIParamRef PositionTextureRHI,
		FUniformBufferRHIParamRef VoxelUniformBuffer,
		FUniformBufferRHIParamRef ParticleCountUniformBuffer
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FDomainVoxelInfoUniformParameters>(), VoxelUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FParticleCountUniformParameters>(), ParticleCountUniformBuffer);
		
		FSamplerStateRHIParamRef SamplerStateTrilinear = TStaticSamplerState<SF_Trilinear>::GetRHI();

		if (InU.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, InU, InUSampler, SamplerStateTrilinear, InURHI);
		if (InV.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, InV, InUSampler, SamplerStateTrilinear, InVRHI);
		if (InW.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, InW, InUSampler, SamplerStateTrilinear, InWRHI);
		if (LiquidBoundary.IsBound()) SetTextureParameter(RHICmdList, ComputeShaderRHI, LiquidBoundary, LiquidBoundarySampler, SamplerStateTrilinear, LiquidBoundaryRHI);

		if (PositionTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
		if (ParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleIndices.GetBaseIndex(), ParticleIndicesRHI);
		}
		if (ExtraParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ExtraParticleIndices.GetBaseIndex(), ExtraParticleIndicesRHI);
		}
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutVelAtPos.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVelAtPos.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if (ParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if (ExtraParticleIndices.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ExtraParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
	}

private:
	FShaderResourceParameter ParticleIndices;
	FShaderResourceParameter ExtraParticleIndices;
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter InU;
	FShaderResourceParameter InV;
	FShaderResourceParameter InW;
	FShaderResourceParameter LiquidBoundary;
	FShaderResourceParameter InUSampler;
	FShaderResourceParameter InVSampler;
	FShaderResourceParameter InWSampler;
	FShaderResourceParameter LiquidBoundarySampler;
	FShaderResourceParameter OutVelAtPos;
};

class FGetLiquidVoxelsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGetLiquidVoxelsCS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FGetLiquidVoxelsCS()
	{
	}

	explicit FGetLiquidVoxelsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		LiquidBoundary.Bind(Initializer.ParameterMap, TEXT("LiquidBoundary"));
		Weights.Bind(Initializer.ParameterMap, TEXT("Weights"));
		ActiveBricksList.Bind(Initializer.ParameterMap, TEXT("ActiveBricksList"));

		OutFloat0.Bind(Initializer.ParameterMap, TEXT("OutFloat0"));
		OutFloat1.Bind(Initializer.ParameterMap, TEXT("OutFloat1"));
		OutLiquidVoxels.Bind(Initializer.ParameterMap, TEXT("OutLiquidVoxels"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LiquidBoundary;
		Ar << Weights;
		Ar << ActiveBricksList;
		Ar << OutFloat0;
		Ar << OutFloat1;
		Ar << OutLiquidVoxels;
		return bShaderHasOutdatedParameters;
	}

	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutLiquidVoxelsUAV,
		FUnorderedAccessViewRHIParamRef OutFloat0UAV,
		FUnorderedAccessViewRHIParamRef OutFloat1UAV
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutFloat0.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFloat0.GetBaseIndex(), OutFloat0UAV);
		if (OutFloat1.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFloat1.GetBaseIndex(), OutFloat1UAV);
		if (OutLiquidVoxels.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidVoxels.GetBaseIndex(), OutLiquidVoxelsUAV, 0);
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture3DRHIParamRef LiquidBoundaryRHI,
		FTexture3DRHIParamRef WeightsRHI,
		FShaderResourceViewRHIParamRef ActiveBricksListRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if (LiquidBoundary.IsBound())RHICmdList.SetShaderTexture(ComputeShaderRHI, LiquidBoundary.GetBaseIndex(), LiquidBoundaryRHI);
		if (Weights.IsBound()) RHICmdList.SetShaderTexture(ComputeShaderRHI, Weights.GetBaseIndex(), WeightsRHI);
		if (ActiveBricksList.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), ActiveBricksListRHI);
	}

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (ActiveBricksList.IsBound()) RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, ActiveBricksList.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		if (OutFloat0.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFloat0.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		if (OutFloat1.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutFloat1.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		if (OutLiquidVoxels.IsBound()) RHICmdList.SetUAVParameter(ComputeShaderRHI, OutLiquidVoxels.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}

private:
	FShaderResourceParameter LiquidBoundary;
	FShaderResourceParameter Weights;
	FShaderResourceParameter ActiveBricksList;

	FShaderResourceParameter OutFloat0;
	FShaderResourceParameter OutFloat1;
	FShaderResourceParameter OutLiquidVoxels;
};
