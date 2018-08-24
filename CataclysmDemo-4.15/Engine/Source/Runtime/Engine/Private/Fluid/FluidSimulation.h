#pragma once
// CATACLYSM 
#include "FluidSimulationCommon.h"
#include "SparseGrid.h"
#include "RadixSort.h"
#include "WavesFFT.h"
#include "IndirectAppendBuffer.h"
#include "FluidDiffuseParticles.h"

// Timers
DECLARE_CYCLE_STAT_EXTERN(TEXT("Fluid Tick"), STAT_FluidTickTime, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Particle Velocity Update"), STAT_FluidParticleVelocityUpdate, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Particle Position Update"), STAT_FluidParticlePositionUpdate, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Pre-Simulate"), STAT_FluidPreSim, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Map New Particles"), STAT_FluidMapNewParticles, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Tiled Resources"), STAT_FluidUpdateTiledResources, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Splat Particles"), STAT_FluidSplatParticles, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("BuildLiquidSurface"), STAT_FluidBuildLiquidSurface, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("BuildSolidBoundary"), STAT_FluidBuildSolidBoundary, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Zero Bricks"), STAT_FluidZeroBricks, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Create BrickMap"), STAT_FluidCreateBrickMap, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Visualize Domain"), STAT_FluidVisualizeDomain, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Divergence"), STAT_FluidDivergence, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Pressure"), STAT_FluidPressure, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Project Velocity"), STAT_FluidProjectVelocity, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Trigger Overlap"), STAT_FluidTriggers, STATGROUP_Fluid, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Foam Update"), STAT_FluidUpdateFoam, STATGROUP_Fluid, );

/**
* Uniform buffer to hold parameters for domain voxel info.
*/
BEGIN_UNIFORM_BUFFER_STRUCT(FDomainVoxelInfoUniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, WorldToVoxel)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, VoxelWidth)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, PICSmoothing)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, SurfaceAtDensity)
END_UNIFORM_BUFFER_STRUCT(FDomainVoxelInfoUniformParameters)

typedef TUniformBufferRef<FDomainVoxelInfoUniformParameters> FDomainVoxelInfoUniformBufferRef;

BEGIN_UNIFORM_BUFFER_STRUCT(FSurfaceSculptingUniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, UseDensityVelocityStretch)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, UseVelocityStretch)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, RadiusMultiplier)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, ParticleRadius)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, StretchMax)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, StretchGain)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, StretchInputScale)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, SurfaceOffset)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, SmoothKernelRadius)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, IgnoreDensityBelow)
END_UNIFORM_BUFFER_STRUCT(FSurfaceSculptingUniformParameters)

typedef TUniformBufferRef<FSurfaceSculptingUniformParameters> FSurfaceSculptingUniformBufferRef;

BEGIN_UNIFORM_BUFFER_STRUCT(FParticleCountUniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, ParticleCount)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, ExtraParticleCount)
END_UNIFORM_BUFFER_STRUCT(FParticleCountUniformParameters)

typedef TUniformBufferRef<FParticleCountUniformParameters> FParticleCountUniformBufferRef;

BEGIN_UNIFORM_BUFFER_STRUCT(FSprayParticleInfoUniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, ParticlesThisDrawCall)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, SprayParticleCount)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, FirstParticle)
END_UNIFORM_BUFFER_STRUCT(FSprayParticleInfoUniformParameters)

typedef TUniformBufferRef<FSprayParticleInfoUniformParameters> FSprayParticleInfoUniformBufferRef;

BEGIN_UNIFORM_BUFFER_STRUCT(FGenerateTexCoordsUniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, ParticleCount)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, ExtraParticleCount)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, ElapsedSeconds0)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, ElapsedSeconds1)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, bRegenerate0)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, bRegenerate1)
END_UNIFORM_BUFFER_STRUCT(FGenerateTexCoordsUniformParameters)
typedef TUniformBufferRef<FGenerateTexCoordsUniformParameters> FGenerateTexCoordsUniformBufferRef;

BEGIN_UNIFORM_BUFFER_STRUCT(FFluidFieldUniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FMatrix, VolumeToWorld, [MAX_FLUID_FIELDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FMatrix, WorldToVolume, [MAX_FLUID_FIELDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector, VolumeSize, [MAX_FLUID_FIELDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, VelocityAndWeight, [MAX_FLUID_FIELDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(uint32, RegionFlags, [MAX_FLUID_FIELDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Count)
END_UNIFORM_BUFFER_STRUCT(FFluidFieldUniformParameters)

typedef TUniformBufferRef<FFluidFieldUniformParameters> FFluidFieldUniformBufferRef;

/**
* Uniform buffer to hold parameters for spray events condition
*/
BEGIN_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent1UniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, bEnabled)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, SpawnDelay)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, MinEscapeSpeed)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, MinDistanceToSurface)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, ZBias)
END_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent1UniformParameters)

typedef TUniformBufferRef<FSprayParticleSpawnEvent1UniformParameters> FSprayParticleSpawnEvent1UniformBufferRef;

BEGIN_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent2UniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, bEnabled)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, SpawnDelay)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, MinNewVelocityLength)
END_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent2UniformParameters)

typedef TUniformBufferRef<FSprayParticleSpawnEvent2UniformParameters> FSprayParticleSpawnEvent2UniformBufferRef;

BEGIN_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent3UniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, bEnabled)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, SpawnDelay)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, MinEscapeSpeed)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, MinDistanceToSurface)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, DeltaDepth)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, bIgnoreCollided)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, bIgnoreEscaped)
END_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent3UniformParameters)

typedef TUniformBufferRef<FSprayParticleSpawnEvent3UniformParameters> FSprayParticleSpawnEvent3UniformBufferRef;

class FReadbackBuffer;
class FCountAppendBuffer;
class FGPUParticleVertexBuffer;
class FParticleTileVertexBuffer;

namespace FluidConsoleVariables
{
	extern int32 X;
	extern int32 RunSimProfiler;
	extern int32 ExtendVelocityCount;
	extern int32 ShowBricks;
	extern int32 Pause;
	extern float TimestepMultiplier;
	extern float MaxTimestep;
	extern int32 ShowLiquidSprites;
	extern int32 ShowFoamSprites;
	extern int32 DebugRenderMode;
	extern int32 LimitSpraySpawn;
	extern int32 RunOnPause;
	extern int32 NumPressureIter;
	extern int32 DoRepairLevelset;
}

// EZeroBricksDataMode and ETexDataType must combine for ZeroBricks
enum EZeroBricksDataMode
{
	ZBD_mapped = 0x00,
	ZBD_unmapped = 0x01,
};

enum ERedistanceLevelsetMode
{
	RL_Normal	= 0x00,
	RL_Levelset	= 0x01,
	RL_InUint	= 0x02,
	RL_IgnoreOffset= 0x04,
	RL_OutUint = 0x08,
	RL_UseCrude = 0x10,
};

class FFluidRegion
{
public:
	int32 Index;
	int32 Type;

	FBox Bounds;	// for clip with domain

	FMatrix VolumeToWorld;
	FVector VolumeSize;		// full box size
	FVector4 VelocityAndWeight;		// in cm
	uint32 RegionFlags;

	class UShapeComponent* ShapeComponent; // don't do anything on this at renderer thread unless you could keep thread safe.
	FFluidRegion()
		: Index(INDEX_NONE)
		, ShapeComponent(nullptr)
		, Type(0)
		, RegionFlags(0x00)
	{
	}

	~FFluidRegion()
	{
	}
};

struct FSprayParticleCreatePointData
{
	FVector4 position;
	FVector4 velocity;
};

struct FSurfaceSculptingParameters
{
	uint32 UseDensityVelocityStretch;
	uint32 UseVelocityStretch;
	float RadiusMultiplier;
	float ParticleRadius;
	float StretchMax;
	float StretchGain;
	float StretchInputScale;
	float SurfaceOffset;
	float SmoothKernelRadius;
	float IgnoreDensityBelow;
};

struct FWavesParameters
{
	bool bEnabled;
	float WavesAmplitude;
	float WavesLengthPeriod;
	float WavesMaxWaveLength;
	float WavesWindSpeed;
};


struct FFluidStats
{
	int32 NumLiquidParticlesBirthed;
	int32 NumFoamParticlesBirthed;
	int32 NumSprayParticlesBirthed;
	int32 NumMaxLiquidParticles;
	int32 NumMaxBricks;
	int32 NumMaxVoxels;
	void Reset() { NumLiquidParticlesBirthed = NumFoamParticlesBirthed= NumSprayParticlesBirthed = NumMaxLiquidParticles = NumMaxBricks = NumMaxVoxels = 0; }
	void Update(int32 LivingParticleCount, int32 NumActiveBricks, int32 NumSolvedVoxels)
	{
		NumMaxLiquidParticles = FMath::Max(NumMaxLiquidParticles, LivingParticleCount);
		NumMaxBricks = FMath::Max(NumMaxBricks, NumActiveBricks);
		NumMaxVoxels = FMath::Max(NumMaxVoxels, NumSolvedVoxels);
	}
	void AddParticles(int32 ParticleCount, int32 ParticleType)
	{
		if (ParticleType == ELiquidParticleType::Spray) NumSprayParticlesBirthed += ParticleCount;
		else if (ParticleType == ELiquidParticleType::Foam) NumFoamParticlesBirthed += ParticleCount;
		else if (ParticleType == ELiquidParticleType::Liquid) NumLiquidParticlesBirthed += ParticleCount;
	}
};


class FFluidSimulation
{
public:
	FFluidSimulation(float voxelWidth, float pICSmoothing, float SurfaceAtDensity, const FVector& translation, class UFLIPSolverDomainComponent* DomainComponent,
		const FSurfaceSculptingParameters& InSurfaceSculptingParameters, const FWavesParameters& InWavesParameters, float InTexCoordRadiusMultiplier, const FFluidDiffuseParticlesParameters& InDiffuseParticlesParameters);

	~FFluidSimulation();

	void InitResources(TSparseArray<FFluidRegion*>* FluidRegions);

	void Destroy();

	// Clear Grid
	// Get Tile list from GPU	
	// Fill CPU tile list with new particles
	void PreSimulate(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef PositionTextureRHI, FTexture2DRHIRef VelocityTextureRHI);
	
	// Update tile mapping for Grid velocity
	// Update Grid velocity	
	void Simulate(FRHICommandListImmediate& RHICmdList, 
		float DeltaSeconds,
		const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
		FTexture2DRHIRef PrevPositionTextureRHI, FTexture2DRHIRef PrevVelocityTextureRHI, FTexture2DRHIRef CurFLIPVelocityTextureRHI);

	void PostSimulate(FRHICommandListImmediate& RHICmdList,
		const TArray<FGPUParticleVertexBuffer*>& GPUParticleVertexBuffers,
		FTexture2DRHIRef PositionTextureRHI, float DeltaSeconds);

	// Use the new positions to create a brick map on the GPU for use next frame.
	// Fill GPU tile list with GPU particles
	// Copy to staging resource and prepare for readback
	void CreateBrickMap(FRHICommandListImmediate& RHICmdList,
		const TArray<FGPUParticleVertexBuffer*>& GPUParticleVertexBuffers,
		FTexture2DRHIRef PositionTextureRHI);

	void PreSpawnSpray(int32 EventType);
	void SpawnSpray(FRHICommandList& RHICmdList,
		const FShaderResourceViewRHIRef& InVertexBufferSRV,
		const FUnorderedAccessViewRHIRef& VertexBufferUAV,
		const uint32 FirstParticle,
		const uint32 ParticlesThisDrawCall,
		const int32 EventType);

	void Visualize(FPrimitiveDrawInterface* PDI);

	void UpdateSprayParameters();
	void UpdateSimulationParameters(float InPICSmoothing, float InSurfaceAtDensity);
	void UpdateSurfaceSculptingParameters(uint32 InUseDensityVelocityStretch, uint32 InUseVelocityStretch, float InRadiusMultiplier, float InParticleRadius,
		float InStretchMax, float InStretchGain, float InStretchInputScale, float InSurfaceOffset, float InSmoothKernelRadius, float InIgnoreDensityBelow);

	void UpdateDomainTransforms(float newVoxelWidth, const FVector& translation);

	void GetFluidRenderingParams(struct FFluidRenderingParams& OutParams);

	FUnorderedAccessViewRHIRef GetSprayParticleBirthDataUAV(int32 EventType) const;
	FShaderResourceViewRHIRef GetSprayParticleBirthDataSRV(int32 EventType) const;

	int32 GetSprayParticleCount(int32 EventType) { return SprayParticleCount[EventType]; }

	void UpdateRegionsBuffer();
	void NotifyActors(uint32* ChangedTriggerList, uint32 NumberOfTriggers);

	void UpdateWaves(float Amplitude, float LengthPeriod, float WindSpeed);

	FFluidStats FluidStats;
	// The living particle count is the number of active particles and injected particles from the end of last frame.
	//  VertexBuffer->ParticleCount will be large, it has dead particles, and non initialized particles in it.
	uint32 ExtraParticleCount;
	uint32 SortedParticleCount;

	// In a staggered velocity grid, the components of velocity are held on the faces of the voxel.  The voxel center velocity would be the average of the face velocity.
	FSparseGrid::TypedVTRStructured* VelocityWeightsVTR;// uint
	FSparseGrid::TypedVTRStructured* ValidFlagsVTR;// uint
	FSparseGrid::TypedVTRStructured* SmoothDensityVTR;
	FSparseGrid::TypedVTRStructured* LiquidBoundaryVTR;
	FSparseGrid::TypedVTRStructured* SolidBoundaryVTR;// if we stop using SolidBoundaryVTR after final velocity we can get rid of FloatVTR
	FSparseGrid::TypedVTRStructured* FloatVTR;
 	FSparseGrid::TypedVTRStructured* Velocity_U_VTR;
 	FSparseGrid::TypedVTRStructured* Velocity_V_VTR;
 	FSparseGrid::TypedVTRStructured* Velocity_W_VTR;
	// Levelsets can be twice the resolution of the sim VTRs, and are all 32bpp
	FSparseGrid::TypedVTRStructured* LiquidSurfaceLS;

	FSparseGrid::TypedVTRStructured* Uint32LS; // pointer only, if LS_MULTIPLIER == 1

	// Just pointers for use, not allocated.
	FSparseGrid::TypedVTRStructured* PressureVTR[2];
	FSparseGrid::TypedVTRStructured* GridVelocityU;
	FSparseGrid::TypedVTRStructured* GridVelocityV;
	FSparseGrid::TypedVTRStructured* GridVelocityW;

	TUniformBufferRef<class FFluidFieldUniformParameters> FluidFieldUniformBuffer;


	bool SurfaceSculptingParametersDirty;
	TUniformBufferRef<class FSurfaceSculptingUniformParameters> SurfaceSculptingUniformBuffer;
	bool SimulationParametersDirty;
	TUniformBufferRef<class FDomainVoxelInfoUniformParameters> DomainVoxelInfoUniformBuffer;
	bool SprayParametersDirty;
	TUniformBufferRef<class FSprayParticleSpawnEvent1UniformParameters> SprayParticleSpawnEvent1UniformBuffer;
	TUniformBufferRef<class FSprayParticleSpawnEvent2UniformParameters> SprayParticleSpawnEvent2UniformBuffer;
	TUniformBufferRef<class FSprayParticleSpawnEvent3UniformParameters> SprayParticleSpawnEvent3UniformBuffer;
	

	float VoxelWidth;
	float PICSmoothing;
	float SurfaceAtDensity;

	FMatrix WorldToVoxel;
	FMatrix VoxelToWorld;
	FMatrix WorldToBrick;
	FMatrix BrickToWorld;
	FMatrix WorldToLevelset;
	FMatrix LevelsetToWorld;

	FTexture2DRHIRef ParticlesPositionTexture;

	struct FParticlesTexCoords
	{
		FTexture2DRHIRef Texture;
		FUnorderedAccessViewRHIRef UAV;
		float ElapsedSeconds;
		bool bRegenerate;
		float Weight;
	};
	FParticlesTexCoords ParticlesTexCoords[PARTICLES_TEX_COORDS_COUNT];


	FTexture2DRHIRef PreSolveParticleVelocityTexture;
	FUnorderedAccessViewRHIRef PreSolveParticleVelocityUAV;

	// For some reason, spray was birthing from buffers before they were set to 0, so it was trying to birth a fill buffer worht of particles.
	bool SprayBuffersAreInitializedTo0;

private:
	template <uint32 GroupCount>
	void IndirectDispatchFillParameter(FRHICommandListImmediate& RHICmdList, FIndirectAppendBuffer *IndirectAppendBuffer);

	int32 SortParticles(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef ParticlePositionTextureRHI);

	void ComputeVelocityWeights(FRHICommandListImmediate& RHICmdList,
		FSparseGrid::TypedVTRStructured* InNodalLevelset,
		FSparseGrid::TypedVTRStructured* OutWeights);
	void ConstrainScalarVelocity(FRHICommandListImmediate& RHICmdList,
		bool GetFaces,
		FSparseGrid::TypedVTRStructured* InNodalLevelset,
		FSparseGrid::TypedVTRStructured* InWeights,
		FSparseGrid::TypedVTRStructured* InValidFlags,
		FSparseGrid::TypedVTRStructured* InOutU,
		FSparseGrid::TypedVTRStructured* InOutV,
		FSparseGrid::TypedVTRStructured* InOutW);
	void ExtendScalarVelocity(FRHICommandListImmediate& RHICmdList,
		int32 Count,
		FSparseGrid::TypedVTRStructured* U,
		FSparseGrid::TypedVTRStructured* V,
		FSparseGrid::TypedVTRStructured* W,
		FSparseGrid::TypedVTRStructured* Valid,
		FSparseGrid::TypedVTRStructured* TempFloat,
		FSparseGrid::TypedVTRStructured* TempValid);

	void UpdateDomainVoxelInfoUniformBuffer();
	void UpdateSurfaceSculptingUniformBuffer();
	void UpdateSprayParticleSpawnEventUniformBuffers();

	// run one pass of levelset redistancing, to make the levelset smooth and length(grad(levelset)) = 1;
	template <int32 Mode, class InType, class OutType>
	void RedistanceLevelset(FRHICommandListImmediate& RHICmdList, InType* InLevelset, OutType* OutLevelset);

	// run one pass to repair the levelset that was splatted with a small search radius compared to the particle radius
	template <int32 Mode, class InType, class OutType>
	void RepairLevelset(FRHICommandListImmediate& RHICmdList, InType* InLevelset, OutType* OutLevelset);

	template <EZeroBricksDataMode dataMode>
	void ZeroBricks(FRHICommandListImmediate& RHICmdList, const TArray<FSparseGrid::TypedVTRStructured*>& VTRs, int32 NumBricks, FShaderResourceViewRHIParamRef BrickListRHI);
	void ZeroSingleVTR(FRHICommandListImmediate& RHICmdList, FSparseGrid::TypedVTRStructured* VTR);

	int32 FrameIndex;
	bool BufferCleared;

	FSparseGrid* SparseGrid;

	FTexture3DRHIRef BrickMapTexture[2];// cur and prev frame brick map texture.
	FUnorderedAccessViewRHIRef BrickMapUAV[2];
	FTexture3DRHIRef BrickMapCrossingsTexture;// Bricks with a level set crossing in them, for rendering speed up.
	FUnorderedAccessViewRHIRef BrickMapCrossingsUAV;

	// GPU side only append buffer to hold the particle indices we should go over each frame.  filed at end of frame.
	// The ParticlesToSort are the first MAX_SPLAT in each cell, and after that they go to ExtraParticleIndies... because not all those are needed by the simulation.
	FIndirectAppendBuffer* ParticlesToSort;
	FIndirectAppendBuffer* ExtraParticleIndices;
	FReadbackBuffer* ChangedBricksListFromGPU;

	TUniformBufferRef<class FFluidTriggerUniformParameters> FluidTriggerUniformBuffer;

	FReadbackBuffer* ChangedTriggers;
	FRWBufferStructured*	TriggerStates[2];
	TSparseArray<FFluidRegion*>* FluidRegions;	// pointer to FXSystem fluid triggers
	class UFLIPSolverDomainComponent* DomainComponent;
	uint32 FluidTriggerCount;
	uint32 FluidFieldCount;

	FBox Bounds;

	FRadixSort* RadixSort;

	FShaderResourceViewRHIParamRef SortedParticlesIndicesSRV;

	// Spray
	FCountAppendBuffer* SprayParticleBirthDataBuffer[EFluidEventType::Event_Max];
	uint32 SprayParticleCount[EFluidEventType::Event_Max];
	float SprayEventPercentage[EFluidEventType::Event_Max];

	// Voxels with liquid boundary < 0 and not all the way inside a solid.  Used to iterate over pressure.
	FIndirectAppendBuffer* LiquidVoxels;// Use for even and odd liquid voxels.
	FRWBufferStructured PressureSolverParams;

	FIndirectAppendBuffer* FacesToConstrain;
	FRWBufferStructured ConstrainVelocityParams;

	// indices into the sorted particle list of the first particle in a voxel, or sub-voxel
	FIndirectAppendBuffer* FirstParticleInVoxel;

	FIndirectAppendBuffer* BricksWithParticles;

	FRWBufferStructured ParticlePositionInVox; // sorted particle position in voxel space.

	FSurfaceSculptingParameters SurfaceSculptingParameters;
	FWavesParameters WavesParameters;

	FWavesFFT* WavesFFT;
	float TotalSeconds;

	FTexture2DRHIRef WavesTexture;

	float TexCoordRadiusMultiplier;

	float LastDeltaSeconds;

	FFluidDiffuseParticlesParameters DiffuseParticlesParameters;
	FFluidDiffuseParticles* DiffuseParticles;

	uint32 frameCount;
	float lastProfileTime;
};