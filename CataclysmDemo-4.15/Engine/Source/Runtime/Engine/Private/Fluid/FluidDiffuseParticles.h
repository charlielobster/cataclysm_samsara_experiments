#pragma once
// CATACLYSM 

DECLARE_CYCLE_STAT_EXTERN(TEXT("FluidDP Scan"), STAT_FluidDP_Scan, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("FluidDP Compact"), STAT_FluidDP_Compact, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("FluidDP Simulate"), STAT_FluidDP_Simulate, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("FluidDP Append"), STAT_FluidDP_Append, STATGROUP_Fluid, );

struct FFluidDiffuseParticlesParameters
{
	bool  bEnabled;
	float TrappedAirMin;
	float TrappedAirMax;
	float WaveCrestMin;
	float WaveCrestMax;
	float TrappedAirSamples;
	float WaveCrestSamples;
	float EnergyMin;
	float EnergyMax;
	float LifetimeMin;
	float LifetimeMax;
	float BubbleDrag;
	float BubbleBuoyancy;
	float FoamSurfaceDistThreshold;
	float GenerateSurfaceDistThreshold;
	float GenerateRadius;
	float FadeinTime;
	float FadeoutTime;
	float SideVelocityScale;
};


class FFluidDiffuseParticles
{
public:
	FFluidDiffuseParticles(uint32 InMaxParticlesCount);
	~FFluidDiffuseParticles();

	void SetParameters(const FFluidDiffuseParticlesParameters& InParameters)
	{
		Parameters = InParameters;
	}

	void Destroy();


	void Simulate(FRHICommandListImmediate& RHICmdList, float DeltaSeconds,
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
		);

public:
	uint32 MaxParticlesCount;
	uint32 DispatchGridDim;

	int32 FrameIndex;

	FRWBuffer Indices[2];
	FRWBuffer ScanTemp;
	FRWBuffer LastCount;
	FRWBuffer TargetCount;

	FRWBuffer PositionBuffer;
	FRWBuffer VelocityBuffer;
	FRWBuffer RenderAttrBuffer;

	FRWBuffer DrawArgs;

	FFluidDiffuseParticlesParameters Parameters;
};
