// CATACLYSM 
// Solver TODO:
//		Write faster/better pressure solver.

// TODO: Anisotropic particles instead of spheres.

// TODO: Use DispatchIndirect so we can build the LiquidSirface at the end of the frame and use it the next... complications clearing it maybe?

// TODO: ? Do we need a staging buffer to get the out brick list append buffer from the gpu?  Since we only have one buffer (See Stagging resources in ReadbackBuffer).

// TODO: Make The velocity in Fluid Fields affect the pressure solve... did before change to staggared and variational boundary.

#include "FluidSimulation.h"
#include "EnginePrivatePCH.h"

#include "RHIStaticStates.h"
#include "UniformBuffer.h"
#include "ShaderParameterUtils.h"
#include "SparseGrid.h"
#include "CountAppendBuffer.h"
#include "ReadbackBuffer.h"
#include "GlobalShader.h"
#include "SceneUtils.h"
#include "../Particles/ParticleSimulationGPU.h"
#include "GlobalDistanceFieldParameters.h"
#include "VTRDebugger.h"
#include "Fluid/FLIPSolverDomainComponent.h"
#include "FluidGlobalShaders.h"

#include "Components/ShapeComponent.h"

// Logs
DEFINE_LOG_CATEGORY(CataclysmInfo);
DEFINE_LOG_CATEGORY(CataclysmErrors);

// Timers
DEFINE_STAT(STAT_FluidPreSim);
DEFINE_STAT(STAT_FluidMapNewParticles);
DEFINE_STAT(STAT_FluidZeroBricks);
DEFINE_STAT(STAT_FluidCreateBrickMap);
DEFINE_STAT(STAT_FluidUpdateTiledResources);
DEFINE_STAT(STAT_FluidSplatParticles);
DEFINE_STAT(STAT_FluidBuildLiquidSurface);
DEFINE_STAT(STAT_FluidBuildSolidBoundary);
DEFINE_STAT(STAT_FluidVisualizeDomain);
DEFINE_STAT(STAT_FluidDivergence);
DEFINE_STAT(STAT_FluidPressure);
DEFINE_STAT(STAT_FluidProjectVelocity);
DEFINE_STAT(STAT_FluidTriggers);
DEFINE_STAT(STAT_FluidUpdateFoam);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FSurfaceSculptingUniformParameters, TEXT("SurfaceSculpting"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FDomainVoxelInfoUniformParameters, TEXT("DomainVoxelInfo"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleCountUniformParameters,TEXT("ParticleCount"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FSprayParticleInfoUniformParameters, TEXT("SprayParticleInfo"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FFluidTriggerUniformParameters,TEXT("FluidTriggers"));
typedef TUniformBufferRef<FFluidTriggerUniformParameters> FFluidTriggerUniformBufferRef;
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FFluidFieldUniformParameters,TEXT("FluidFields"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FGenerateTexCoordsUniformParameters, TEXT("GenerateTexCoords"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent1UniformParameters, TEXT("SprayParticleSpawnEvent1"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent2UniformParameters, TEXT("SprayParticleSpawnEvent2"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FSprayParticleSpawnEvent3UniformParameters, TEXT("SprayParticleSpawnEvent3"));

IMPLEMENT_SHADER_TYPE(, FComputeVelocityWeightsCS, TEXT("FluidComputeVelocityWeights"), TEXT("ComputeVelocityWeights"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FParticlesToGridCS<false>, TEXT("FluidParticlesToGrid"), TEXT("ParticlesToGrid"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FParticlesToGridCS<true>, TEXT("FluidParticlesToGrid"), TEXT("ParticlesToGrid"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FExtendLiquidIntoSolidCS, TEXT("FluidExtendLiquidIntoSolid"), TEXT("ExtendLiquidIntoSolid"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FAddSkirtCS, TEXT("FluidAddSkirt"), TEXT("AddSkirt"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FBuildSolidBoundaryCS, TEXT("FluidBuildSolidBoundary"), TEXT("BuildSolidBoundary"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FCalculateBricksCS,TEXT("FluidCalculateBricksShader"),TEXT("CalculateBricks"),SF_Compute);
IMPLEMENT_SHADER_TYPE(, FCreateBrickMapCS,TEXT("FluidCreateBrickMapShader"),TEXT("CreateBrickMap"),SF_Compute);
IMPLEMENT_SHADER_TYPE(, FDivergenceLiquidCS, TEXT("FluidDivergenceShader"), TEXT("DivergenceLiquid"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FLevelsetSplatCS<false>, TEXT("FluidLevelsetSplat"), TEXT("LevelsetSplat"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FLevelsetSplatCS<true>, TEXT("FluidLevelsetSplat"), TEXT("LevelsetSplat"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FMarkLevelsetCrossingsCS, TEXT("FluidMarkLevelsetCrossings"), TEXT("MarkLevelsetCrossings"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPrepLevelsetSplatCS, TEXT("FluidPrepLevelsetSplat"), TEXT("PrepLevelsetSplat"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPressureJacobiCS, TEXT("FluidPressureJacobi"), TEXT("PressureJacobi"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FProjectVelocityCS, TEXT("FluidProjectVelocity"), TEXT("ProjectVelocity"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRedistanceLevelsetCS<RL_Normal>, TEXT("FluidRedistanceLevelset"), TEXT("RedistanceLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRedistanceLevelsetCS<RL_OutUint>, TEXT("FluidRedistanceLevelset"), TEXT("RedistanceLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRedistanceLevelsetCS<RL_InUint>, TEXT("FluidRedistanceLevelset"), TEXT("RedistanceLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRedistanceLevelsetCS<RL_UseCrude | RL_OutUint>, TEXT("FluidRedistanceLevelset"), TEXT("RedistanceLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRedistanceLevelsetCS<RL_UseCrude>, TEXT("FluidRedistanceLevelset"), TEXT("RedistanceLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRedistanceLevelsetCS<RL_InUint | RL_Levelset>, TEXT("FluidRedistanceLevelset"), TEXT("RedistanceLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRedistanceLevelsetCS<RL_InUint | RL_UseCrude | RL_Levelset>, TEXT("FluidRedistanceLevelset"), TEXT("RedistanceLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRedistanceLevelsetCS<RL_OutUint | RL_UseCrude | RL_Levelset>, TEXT("FluidRedistanceLevelset"), TEXT("RedistanceLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRepairLevelsetCS<RL_InUint>, TEXT("FluidRepairLevelset"), TEXT("RepairLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRepairLevelsetCS<RL_InUint | RL_Levelset>, TEXT("FluidRepairLevelset"), TEXT("RepairLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRepairLevelsetCS<RL_OutUint | RL_Levelset>, TEXT("FluidRepairLevelset"), TEXT("RepairLevelset"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FNormalizeVelocityAndSplatFieldsCS, TEXT("FluidNormalizeVelocityAndSplatFields"), TEXT("NormalizeVelocityAndSplatFields"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroBricksCS<ZBD_unmapped | FSparseGrid::ETexDataType::TDT_FLOAT>, TEXT("FluidZeroBricks"), TEXT("ZeroBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroBricksCS<ZBD_unmapped | FSparseGrid::ETexDataType::TDT_FLOAT2>, TEXT("FluidZeroBricks"), TEXT("ZeroBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroBricksCS<ZBD_unmapped | FSparseGrid::ETexDataType::TDT_FLOAT4>, TEXT("FluidZeroBricks"), TEXT("ZeroBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroBricksCS<ZBD_unmapped | FSparseGrid::ETexDataType::TDT_UINT>, TEXT("FluidZeroBricks"), TEXT("ZeroBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroBricksCS<ZBD_mapped | FSparseGrid::ETexDataType::TDT_FLOAT>, TEXT("FluidZeroBricks"), TEXT("ZeroBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroBricksCS<ZBD_mapped | FSparseGrid::ETexDataType::TDT_FLOAT2>, TEXT("FluidZeroBricks"), TEXT("ZeroBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroBricksCS<ZBD_mapped | FSparseGrid::ETexDataType::TDT_FLOAT4>, TEXT("FluidZeroBricks"), TEXT("ZeroBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroBricksCS<ZBD_mapped | FSparseGrid::ETexDataType::TDT_UINT>, TEXT("FluidZeroBricks"), TEXT("ZeroBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroLevelsetBricksCS<ZBD_unmapped>, TEXT("FluidZeroLevelsetBricks"), TEXT("ZeroLevelsetBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FZeroLevelsetBricksCS<ZBD_mapped>, TEXT("FluidZeroLevelsetBricks"), TEXT("ZeroLevelsetBricks"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FTriggerOverlapCS,TEXT("FluidTriggerOverlap"),TEXT("TriggerOverlap"),SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGenerateKeysAndValuesCS, TEXT("FluidParticlesGrid"), TEXT("GenerateKeysAndValues"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGenerateCellRangesCS, TEXT("FluidParticlesGrid"), TEXT("GenerateCellRanges"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FCoverSprayParticleCS, TEXT("FluidCoverSprayParticles"), TEXT("CoverSprayParticles"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGenerateTexCoordsCS, TEXT("FluidGenerateTexCoords"), TEXT("GenerateTexCoords"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<1>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<2>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<4>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<8>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<16>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<32>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<64>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<128>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIndirectDispatchFillParameterCS<256>, TEXT("FluidIndirectDispatchFillParameter"), TEXT("IndirectDispatchFillParameter"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FVelocityToGridCS, TEXT("FluidVelocityToGrid"), TEXT("VelocityToGrid"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FDensityToGridCS<false>, TEXT("FluidDensityToGrid"), TEXT("DensityToGrid"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FDensityToGridCS<true>, TEXT("FluidDensityToGrid"), TEXT("DensityToGrid"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FAndoBoundaryCS, TEXT("FluidAndoBoundary"), TEXT("AndoBoundary"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPosInVoxCS, TEXT("FluidPosInVox"), TEXT("PosInVox"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FExtendScalarVelocityCS<1>, TEXT("FluidExtendScalarVelocity"), TEXT("ExtendScalarVelocity"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FExtendScalarVelocityCS<2>, TEXT("FluidExtendScalarVelocity"), TEXT("ExtendScalarVelocity"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FExtendScalarVelocityCS<4>, TEXT("FluidExtendScalarVelocity"), TEXT("ExtendScalarVelocity"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FConstrainGetFacesCS, TEXT("FluidConstrainScalarVelocityWithSolidBoundary"), TEXT("ConstrainGetFaces"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FConstrainFillVelocityBufferCS, TEXT("FluidConstrainScalarVelocityWithSolidBoundary"), TEXT("ConstrainFillVelocityBuffer"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FConstrainScalarVelocityWithSolidBoundaryCS, TEXT("FluidConstrainScalarVelocityWithSolidBoundary"), TEXT("ConstrainScalarVelocityWithSolidBoundary"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FVelAtPosCS, TEXT("FluidVelAtPos"), TEXT("VelAtPos"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGetLiquidVoxelsCS, TEXT("FluidGetLiquidVoxels"), TEXT("GetLiquidVoxels"), SF_Compute);
static const float TexCoordsRegeneratePeriod = 10.0f; //seconds

FFluidSimulation::FFluidSimulation(float voxelWidth, float InPICSmoothing, float InSurfaceAtDensity, const FVector& translation, UFLIPSolverDomainComponent* InDomainComponent,
	const FSurfaceSculptingParameters& InSurfaceSculptingParameters, const FWavesParameters& InWavesParameters, float InTexCoordRadiusMultiplier, const FFluidDiffuseParticlesParameters& InDiffuseParticlesParameters)
	: ExtraParticleCount(0)
	, SortedParticleCount(0)
	, SparseGrid(nullptr)
	, VelocityWeightsVTR(nullptr)
	, LiquidBoundaryVTR(nullptr)
	, SolidBoundaryVTR(nullptr)
	, FloatVTR(nullptr)
	, Velocity_U_VTR(nullptr)
	, Velocity_V_VTR(nullptr)
	, Velocity_W_VTR(nullptr)
	, SmoothDensityVTR(nullptr)
	, ValidFlagsVTR(nullptr)
	, LiquidSurfaceLS(nullptr)
	, Uint32LS(nullptr)
	, GridVelocityU(nullptr)
	, GridVelocityV(nullptr)
	, GridVelocityW(nullptr)
	, SprayParametersDirty(true)
	, SimulationParametersDirty(true)
	, SurfaceSculptingParametersDirty(true)
	, PICSmoothing(InPICSmoothing)
	, SurfaceAtDensity(InSurfaceAtDensity)
	, ChangedBricksListFromGPU(nullptr)
	, ChangedTriggers(nullptr)
	, FrameIndex(0)
	, BufferCleared(0)
	, FluidRegions(nullptr)
	, DomainComponent(InDomainComponent)
	, FluidTriggerCount(0)
	, FluidFieldCount(0)
	, RadixSort(nullptr)
	, SurfaceSculptingParameters(InSurfaceSculptingParameters)
	, WavesParameters(InWavesParameters)
	, WavesFFT(nullptr)
	, TotalSeconds(0)
	, TexCoordRadiusMultiplier(InTexCoordRadiusMultiplier)
	, LastDeltaSeconds(0.0f)
	, DiffuseParticles(nullptr)
	, DiffuseParticlesParameters(InDiffuseParticlesParameters)
	, frameCount(0)
	, lastProfileTime(0.0f)
{
	FluidStats.Reset();
	PressureVTR[0] = PressureVTR[1] = nullptr;
	TriggerStates[0] = TriggerStates[1] = nullptr;
	UpdateDomainTransforms(voxelWidth, translation);
	ParticlesToSort = nullptr;
	ExtraParticleIndices = nullptr;

	for (int32 i = 0; i < EFluidEventType::Event_Max; i++)
	{
		SprayParticleBirthDataBuffer[i] = nullptr;
		SprayParticleCount[i] = 0;
		SprayEventPercentage[i] = 1.0f;
	}

	LiquidVoxels = nullptr;
	FacesToConstrain = nullptr;
	FirstParticleInVoxel = nullptr;
	BricksWithParticles = nullptr;

	SprayBuffersAreInitializedTo0 = false;
}

FFluidSimulation::~FFluidSimulation()
{
}

void FFluidSimulation::UpdateDomainVoxelInfoUniformBuffer()
{
	//if (SimulationParametersDirty)
	{
		SimulationParametersDirty = false;
		DomainVoxelInfoUniformBuffer.SafeRelease();
		FDomainVoxelInfoUniformParameters DomainVoxelInfoParameters;
		DomainVoxelInfoParameters.WorldToVoxel = FVector4(WorldToVoxel.M[3][0], WorldToVoxel.M[3][1], WorldToVoxel.M[3][2], WorldToVoxel.M[0][0]);
		DomainVoxelInfoParameters.VoxelWidth = VoxelWidth;
		DomainVoxelInfoParameters.PICSmoothing = PICSmoothing;
		DomainVoxelInfoParameters.SurfaceAtDensity = SurfaceAtDensity;
		DomainVoxelInfoUniformBuffer = FDomainVoxelInfoUniformBufferRef::CreateUniformBufferImmediate(DomainVoxelInfoParameters, UniformBuffer_SingleFrame);
	}
}

void FFluidSimulation::UpdateSurfaceSculptingUniformBuffer()
{
//	if (SurfaceSculptingParametersDirty)
	{
		SurfaceSculptingParametersDirty = false;
		SurfaceSculptingUniformBuffer.SafeRelease();
		FSurfaceSculptingUniformParameters SurfaceSculptingUniformParameters;
		SurfaceSculptingUniformParameters.UseDensityVelocityStretch = SurfaceSculptingParameters.UseDensityVelocityStretch;
		SurfaceSculptingUniformParameters.UseVelocityStretch = SurfaceSculptingParameters.UseVelocityStretch;
		SurfaceSculptingUniformParameters.RadiusMultiplier = SurfaceSculptingParameters.RadiusMultiplier;
		SurfaceSculptingUniformParameters.ParticleRadius = SurfaceSculptingParameters.ParticleRadius;
		SurfaceSculptingUniformParameters.StretchMax = SurfaceSculptingParameters.StretchMax;
		SurfaceSculptingUniformParameters.StretchGain = SurfaceSculptingParameters.StretchGain;
		SurfaceSculptingUniformParameters.StretchInputScale = SurfaceSculptingParameters.StretchInputScale;
		SurfaceSculptingUniformParameters.SurfaceOffset = SurfaceSculptingParameters.SurfaceOffset;
		SurfaceSculptingUniformParameters.SmoothKernelRadius = SurfaceSculptingParameters.SmoothKernelRadius;
		SurfaceSculptingUniformParameters.IgnoreDensityBelow = SurfaceSculptingParameters.IgnoreDensityBelow;
		SurfaceSculptingUniformBuffer = FSurfaceSculptingUniformBufferRef::CreateUniformBufferImmediate(SurfaceSculptingUniformParameters, UniformBuffer_SingleFrame);
	}
}

void FFluidSimulation::UpdateSprayParticleSpawnEventUniformBuffers()
{
	//if (SprayParametersDirty)
	{
		SprayParametersDirty = false;
		// Create Spray Events Condition Uniform Buffer
		SprayParticleSpawnEvent1UniformBuffer.SafeRelease();
		FSprayParticleSpawnEvent1UniformParameters SprayParticleSpawnEvent1UniformParameters;
		SprayParticleSpawnEvent1UniformParameters.bEnabled =			DomainComponent->CollisionSprayEventConditions.bEnabled ? 1 : 0;
		SprayParticleSpawnEvent1UniformParameters.SpawnDelay =			DomainComponent->CollisionSprayEventConditions.SpawnDelay;
		SprayParticleSpawnEvent1UniformParameters.MinDistanceToSurface =DomainComponent->CollisionSprayEventConditions.MinDistanceToSurface;
		SprayParticleSpawnEvent1UniformParameters.MinEscapeSpeed =		DomainComponent->CollisionSprayEventConditions.MinEscapeSpeed;
		SprayParticleSpawnEvent1UniformParameters.ZBias =				DomainComponent->CollisionSprayEventConditions.ZBias;
		SprayParticleSpawnEvent1UniformBuffer = FSprayParticleSpawnEvent1UniformBufferRef::CreateUniformBufferImmediate(SprayParticleSpawnEvent1UniformParameters, UniformBuffer_SingleFrame);
		SprayEventPercentage[EFluidEventType::Event1] = DomainComponent->CollisionSprayEventConditions.SpawnPercentage;

		SprayParticleSpawnEvent2UniformBuffer.SafeRelease();
		FSprayParticleSpawnEvent2UniformParameters SprayParticleSpawnEvent2UniformParameters;
		SprayParticleSpawnEvent2UniformParameters.bEnabled =			DomainComponent->EscapeSprayEventCondtions.bEnabled ? 1 : 0;
		SprayParticleSpawnEvent2UniformParameters.SpawnDelay =			DomainComponent->EscapeSprayEventCondtions.SpawnDelay;
		SprayParticleSpawnEvent2UniformParameters.MinNewVelocityLength =DomainComponent->EscapeSprayEventCondtions.MinNewVelocityLength;
		SprayParticleSpawnEvent2UniformBuffer = FSprayParticleSpawnEvent2UniformBufferRef::CreateUniformBufferImmediate(SprayParticleSpawnEvent2UniformParameters, UniformBuffer_SingleFrame);
		SprayEventPercentage[EFluidEventType::Event2] = DomainComponent->EscapeSprayEventCondtions.SpawnPercentage;

		SprayParticleSpawnEvent3UniformBuffer.SafeRelease();
		FSprayParticleSpawnEvent3UniformParameters SprayParticleSpawnEvent3UniformParameters;
		SprayParticleSpawnEvent3UniformParameters.bEnabled =			DomainComponent->SplashSprayEventCondtions.bEnabled ? 1 : 0;
		SprayParticleSpawnEvent3UniformParameters.SpawnDelay =			DomainComponent->SplashSprayEventCondtions.SpawnDelay;
		SprayParticleSpawnEvent3UniformParameters.MinDistanceToSurface =DomainComponent->SplashSprayEventCondtions.MinDistanceToSurface;
		SprayParticleSpawnEvent3UniformParameters.MinEscapeSpeed =		DomainComponent->SplashSprayEventCondtions.MinEscapeSpeed;
		SprayParticleSpawnEvent3UniformParameters.DeltaDepth =			DomainComponent->SplashSprayEventCondtions.DeltaDepth;
		SprayParticleSpawnEvent3UniformParameters.bIgnoreCollided =		DomainComponent->SplashSprayEventCondtions.bIgnoreCollided ? 1 : 0;
		SprayParticleSpawnEvent3UniformParameters.bIgnoreEscaped =		DomainComponent->SplashSprayEventCondtions.bIgnoreEscaped ? 1 : 0;
		SprayParticleSpawnEvent3UniformBuffer = FSprayParticleSpawnEvent3UniformBufferRef::CreateUniformBufferImmediate(SprayParticleSpawnEvent3UniformParameters, UniformBuffer_SingleFrame);
		SprayEventPercentage[EFluidEventType::Event3] = DomainComponent->SplashSprayEventCondtions.SpawnPercentage;
	}
}

void FFluidSimulation::InitResources(TSparseArray<FFluidRegion*>* InFluidRegions)
{
	check(IsInRenderingThread());
	FluidRegions = InFluidRegions;
	SparseGrid = new FSparseGrid();

	VelocityWeightsVTR = new FSparseGrid::TypedVTRStructured(PF_R32_UINT);
	VelocityWeightsVTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);

	{
		LiquidBoundaryVTR = new FSparseGrid::TypedVTRStructured(PF_R32_FLOAT);
		LiquidBoundaryVTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);
	}

	SolidBoundaryVTR = new FSparseGrid::TypedVTRStructured(PF_R32_FLOAT);
	SolidBoundaryVTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);

	FloatVTR = new FSparseGrid::TypedVTRStructured(PF_R32_FLOAT);
	FloatVTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);

	Velocity_U_VTR = new FSparseGrid::TypedVTRStructured(PF_R32_FLOAT);
	Velocity_U_VTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);

	Velocity_V_VTR = new FSparseGrid::TypedVTRStructured(PF_R32_FLOAT);
	Velocity_V_VTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);

	Velocity_W_VTR = new FSparseGrid::TypedVTRStructured(PF_R32_FLOAT);
	Velocity_W_VTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);

	SmoothDensityVTR = new FSparseGrid::TypedVTRStructured(PF_R32_FLOAT);
	SmoothDensityVTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);

	LiquidSurfaceLS = new FSparseGrid::TypedVTRStructured(PF_R32_FLOAT, LS_MULTIPLIER==2);
	LiquidSurfaceLS->Initialize(LS_DOMAIN_SIZE_X, LS_DOMAIN_SIZE_Y, LS_DOMAIN_SIZE_Z, 1);
#if LS_MULTIPLIER == 2
	Uint32LS = new FSparseGrid::TypedVTRStructured(PF_R32_UINT, LS_MULTIPLIER == 2);
	Uint32LS->Initialize(LS_DOMAIN_SIZE_X, LS_DOMAIN_SIZE_Y, LS_DOMAIN_SIZE_Z, 1);
#endif

	/// The Living particles that we simulate are gathered into two groups at the end of each frame.  The ParticlesToSort, which are sorted into SortedParticleIndicesSRV
	/// ExtraParticles.  The extra particles are all the ones that are beyond the first MAX_SPLAT_COUNT in the splatting procedures.
	ParticlesToSort = new FIndirectAppendBuffer(GPU_PARTICLE_SIM_TEXTURE_SIZE_X*GPU_PARTICLE_SIM_TEXTURE_SIZE_Y, sizeof(uint32));
	ExtraParticleIndices = new FIndirectAppendBuffer(GPU_PARTICLE_SIM_TEXTURE_SIZE_X*GPU_PARTICLE_SIM_TEXTURE_SIZE_Y, sizeof(uint32));

	ChangedBricksListFromGPU = new FReadbackBuffer(MAX_NUM_CHANGED_BRICKS, sizeof(FBrickIndex));// max could be NUM_BRICKS, but if we get there we probably can't run the sim anyway.

	FRHIResourceCreateInfo CreateInfo;
	BrickMapTexture[0] = RHICreateTexture3D(NUM_BRICKS_X, NUM_BRICKS_Y, NUM_BRICKS_Z, PF_R32_UINT, 1, TexCreate_UAV | TexCreate_ShaderResource, CreateInfo);
	BrickMapUAV[0] = RHICreateUnorderedAccessView(BrickMapTexture[0]);
	BrickMapTexture[1] = RHICreateTexture3D(NUM_BRICKS_X, NUM_BRICKS_Y, NUM_BRICKS_Z, PF_R32_UINT, 1, TexCreate_UAV | TexCreate_ShaderResource, CreateInfo);
	BrickMapUAV[1] = RHICreateUnorderedAccessView(BrickMapTexture[1]);
	BrickMapCrossingsTexture = RHICreateTexture3D(NUM_BRICKS_X, NUM_BRICKS_Y, NUM_BRICKS_Z, PF_R32_UINT, 1, TexCreate_UAV | TexCreate_ShaderResource, CreateInfo);
	BrickMapCrossingsUAV = RHICreateUnorderedAccessView(BrickMapCrossingsTexture);

	UpdateDomainVoxelInfoUniformBuffer();

	UpdateSurfaceSculptingUniformBuffer();

	UpdateSprayParticleSpawnEventUniformBuffers();

	TriggerStates[0] = new FRWBufferStructured();
	TriggerStates[0]->Initialize(sizeof(uint32), MAX_FLUID_TRIGGERS);
	TriggerStates[1] = new FRWBufferStructured();
	TriggerStates[1]->Initialize(sizeof(uint32), MAX_FLUID_TRIGGERS);

	ChangedTriggers = new FReadbackBuffer(MAX_FLUID_TRIGGERS, sizeof(uint32));

	RadixSort = new FRadixSort(GPU_PARTICLE_SIM_TEXTURE_SIZE_X * GPU_PARTICLE_SIM_TEXTURE_SIZE_Y);

	if (WavesParameters.bEnabled)
	{
		for (int i = 0; i < PARTICLES_TEX_COORDS_COUNT; ++i)
		{
			ParticlesTexCoords[i].Texture = RHICreateTexture2D(GPU_PARTICLE_SIM_TEXTURE_SIZE_X, GPU_PARTICLE_SIM_TEXTURE_SIZE_Y, PF_G32R32F, 1, 1, TexCreate_UAV | TexCreate_ShaderResource, CreateInfo);
			ParticlesTexCoords[i].UAV = RHICreateUnorderedAccessView(ParticlesTexCoords[i].Texture);
			ParticlesTexCoords[i].ElapsedSeconds = TexCoordsRegeneratePeriod * i / PARTICLES_TEX_COORDS_COUNT;
			ParticlesTexCoords[i].bRegenerate = false;
			ParticlesTexCoords[i].Weight = 0;
		}
		WavesFFT = new FWavesFFT(512, VoxelWidth, WavesParameters.WavesLengthPeriod, WavesParameters.WavesMaxWaveLength, WavesParameters.WavesWindSpeed);
	}

	PreSolveParticleVelocityTexture = RHICreateTexture2D(GPU_PARTICLE_SIM_TEXTURE_SIZE_X, GPU_PARTICLE_SIM_TEXTURE_SIZE_Y, PF_A32B32G32R32F, 1, 1, TexCreate_UAV | TexCreate_ShaderResource, CreateInfo);
	PreSolveParticleVelocityUAV = RHICreateUnorderedAccessView(PreSolveParticleVelocityTexture);

	for (int32 i = 0; i < EFluidEventType::Event_Max; i++)
	{
		SprayParticleBirthDataBuffer[i] = new FCountAppendBuffer(GPU_PARTICLE_SIM_TEXTURE_SIZE_X*GPU_PARTICLE_SIM_TEXTURE_SIZE_Y, sizeof(FSprayParticleCreatePointData));
	}

	// Voxels with liquid boundary < 0 and not all the way inside a solid.  Used to iterate over pressure.
	const uint32 MaxLiquidVoxels = 256 * 256 * 64;// go beyond this and we need to put in error checking and control code.
	LiquidVoxels = new FIndirectAppendBuffer(MaxLiquidVoxels, sizeof(uint32));
	PressureSolverParams.Initialize(sizeof(float)*4, MaxLiquidVoxels);
	
	// Faces that are all solid boundary, and need constraining for the slip conditions. (TODO make the size dynamic or smaller or bigger if needed).
	const uint32 MaxFacesToConstrain = 512 * 512 * 3;// go beyond this and we need to put in error checking and control code.
	FacesToConstrain = new FIndirectAppendBuffer(MaxFacesToConstrain, sizeof(uint32));
	ConstrainVelocityParams.Initialize(sizeof(float) * 2, MaxFacesToConstrain);// TODO worth storing the solid boundary normal too?

	// indices into the sorted particle list of the first particle in a voxel, or sub-voxel
	FirstParticleInVoxel = new FIndirectAppendBuffer(GPU_PARTICLE_SIM_TEXTURE_SIZE_X*GPU_PARTICLE_SIM_TEXTURE_SIZE_Y, sizeof(uint32));// could be smaller

	ValidFlagsVTR = new FSparseGrid::TypedVTRStructured(PF_R32_UINT);
	ValidFlagsVTR->Initialize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z, 1);

	BricksWithParticles = new FIndirectAppendBuffer(MAX_NUM_COUNT_BRICKS, sizeof(uint32));

	// Used to make AnisoToParticles fast enough... should make other things faster too... less matrix multiplies.  Stored in Sorted Order
	ParticlePositionInVox.Initialize(sizeof(float) * 4, GPU_PARTICLE_SIM_TEXTURE_SIZE_X*GPU_PARTICLE_SIM_TEXTURE_SIZE_Y);


	if (DiffuseParticlesParameters.bEnabled)
	{
		DiffuseParticles = new FFluidDiffuseParticles(2 * 1024 * 1024);
		DiffuseParticles->SetParameters(DiffuseParticlesParameters);
	}
}

void FFluidSimulation::Destroy()
{
	check(IsInRenderingThread());
	// print the stats

	const FText NumMaxLiquidParticles = FText::AsNumber(FluidStats.NumMaxLiquidParticles, &FNumberFormattingOptions::DefaultWithGrouping());
	const FText NumMaxBricks = FText::AsNumber(FluidStats.NumMaxBricks, &FNumberFormattingOptions::DefaultWithGrouping());
	const FText NumMaxVoxels = FText::AsNumber(FluidStats.NumMaxBricks*BRICK_SIZE_X*BRICK_SIZE_Y*BRICK_SIZE_Z, &FNumberFormattingOptions::DefaultWithGrouping());
	const FText NumMaxLSVoxels = FText::AsNumber(FluidStats.NumMaxBricks*BRICK_SIZE_X*BRICK_SIZE_Y*BRICK_SIZE_Z*LS_MULTIPLIER*LS_MULTIPLIER*LS_MULTIPLIER, &FNumberFormattingOptions::DefaultWithGrouping());
	const FText NumMaxSolvedVoxels = FText::AsNumber(FluidStats.NumMaxVoxels, &FNumberFormattingOptions::DefaultWithGrouping());
	const FText NumSprayParticlesBirthed = FText::AsNumber(FluidStats.NumSprayParticlesBirthed, &FNumberFormattingOptions::DefaultWithGrouping());
	const FText NumFoamParticlesBirthed = FText::AsNumber(FluidStats.NumFoamParticlesBirthed, &FNumberFormattingOptions::DefaultWithGrouping());
	const FText NumLiquidParticlesBirthed = FText::AsNumber(FluidStats.NumLiquidParticlesBirthed, &FNumberFormattingOptions::DefaultWithGrouping());

	UE_LOG(CataclysmInfo, Log, TEXT("Max Liquid Particles Living at One Time: %s"), *NumMaxLiquidParticles.ToString());
	UE_LOG(CataclysmInfo, Log, TEXT("Max bricks active at one time:           %s"), *NumMaxBricks.ToString());
	UE_LOG(CataclysmInfo, Log, TEXT("Max voxels active at one time:           %s"), *NumMaxVoxels.ToString());
	UE_LOG(CataclysmInfo, Log, TEXT("Max levelset voxels active at one time:  %s"), *NumMaxLSVoxels.ToString());
	UE_LOG(CataclysmInfo, Log, TEXT("Max voxels used in pressure solve:       %s"), *NumMaxSolvedVoxels.ToString());
	UE_LOG(CataclysmInfo, Log, TEXT("Spray Particles Birthed:                 %s"), *NumSprayParticlesBirthed.ToString());
	UE_LOG(CataclysmInfo, Log, TEXT("Foam Particles Birthed:                  %s"), *NumFoamParticlesBirthed.ToString());
	UE_LOG(CataclysmInfo, Log, TEXT("Liquid Particles Birthed:                %s"), *NumLiquidParticlesBirthed.ToString());

	if (DiffuseParticles)
	{
		DiffuseParticles->Destroy();
		DiffuseParticles = nullptr;
	}

	if (WavesFFT)
	{
		WavesFFT->Destroy();
		WavesFFT = nullptr;
	}

	for (int i = 0; i < PARTICLES_TEX_COORDS_COUNT; ++i)
	{
		ParticlesTexCoords[i].UAV.SafeRelease();
		ParticlesTexCoords[i].Texture.SafeRelease();
	}

	PreSolveParticleVelocityUAV.SafeRelease();
	PreSolveParticleVelocityTexture.SafeRelease();

	if (RadixSort)
	{
		RadixSort->Destroy();
		RadixSort = nullptr;
	}

	if (SparseGrid)
	{
		SparseGrid->Destroy();
		SparseGrid = nullptr;
	}

	if (VelocityWeightsVTR)
	{
		VelocityWeightsVTR->Release();
		delete VelocityWeightsVTR;
		VelocityWeightsVTR = nullptr;
	}

	if (LiquidBoundaryVTR)
	{
		LiquidBoundaryVTR->Release();
		delete LiquidBoundaryVTR;
		LiquidBoundaryVTR = nullptr;
	}

	if (SolidBoundaryVTR)
	{
		SolidBoundaryVTR->Release();
		delete SolidBoundaryVTR;
		SolidBoundaryVTR = nullptr;
	}

	if (FloatVTR)
	{
		FloatVTR->Release();
		delete FloatVTR;
		FloatVTR = nullptr;
	}

	if (Velocity_U_VTR)
	{
		Velocity_U_VTR->Release();
		delete Velocity_U_VTR;
		Velocity_U_VTR = nullptr;
	}

	if (Velocity_V_VTR)
	{
		Velocity_V_VTR->Release();
		delete Velocity_V_VTR;
		Velocity_V_VTR = nullptr;
	}

	if (Velocity_W_VTR)
	{
		Velocity_W_VTR->Release();
		delete Velocity_W_VTR;
		Velocity_W_VTR = nullptr;
	}

	if (SmoothDensityVTR)
	{
		SmoothDensityVTR->Release();
		delete SmoothDensityVTR;
		SmoothDensityVTR = nullptr;
	}

	if (LiquidSurfaceLS)
	{
		LiquidSurfaceLS->Release();
		delete LiquidSurfaceLS;
		LiquidSurfaceLS = nullptr;
	}

#if LS_MULTIPLIER == 2
	if (Uint32LS)
	{
		Uint32LS->Release();
		delete Uint32LS;
		Uint32LS = nullptr;
	}
#endif


	if (ParticlesToSort)
	{
		ParticlesToSort->Destroy();
		ParticlesToSort = nullptr;
	}
	
	if (ExtraParticleIndices)
	{
		ExtraParticleIndices->Destroy();
		ExtraParticleIndices = nullptr;
	}

	for (int32 i = 0; i < EFluidEventType::Event_Max; i++)
	{
		if (SprayParticleBirthDataBuffer[i])
		{
			SprayParticleBirthDataBuffer[i]->Destroy();
			SprayParticleBirthDataBuffer[i] = nullptr;
		}
	}

	if (LiquidVoxels)
	{
		LiquidVoxels->Destroy();
		LiquidVoxels = nullptr;
	}
	PressureSolverParams.Release();

	if (FacesToConstrain)
	{
		FacesToConstrain->Destroy();
		FacesToConstrain = nullptr;
	}
	ConstrainVelocityParams.Release();

	if (FirstParticleInVoxel) FirstParticleInVoxel->Destroy();
	FirstParticleInVoxel = nullptr;

	if (ValidFlagsVTR)
	{
		ValidFlagsVTR->Release();
		delete ValidFlagsVTR;
		ValidFlagsVTR = nullptr;
	}

	if (BricksWithParticles) BricksWithParticles->Destroy();
	BricksWithParticles = nullptr;

	ParticlePositionInVox.Release();

	if (ChangedBricksListFromGPU)
	{
		ChangedBricksListFromGPU->Destroy();
		ChangedBricksListFromGPU = nullptr;
	}

	TriggerStates[0]->Release();
	delete TriggerStates[0];
	TriggerStates[0] = nullptr;
	TriggerStates[1]->Release();
	delete TriggerStates[1];
	TriggerStates[1] = nullptr;
	if (ChangedTriggers)
	{
		ChangedTriggers->Destroy();
		ChangedTriggers = nullptr;
	}
	BrickMapTexture[0].SafeRelease();
	BrickMapUAV[0].SafeRelease();
	BrickMapTexture[1].SafeRelease();
	BrickMapUAV[1].SafeRelease();
	BrickMapCrossingsTexture.SafeRelease();
	BrickMapCrossingsUAV.SafeRelease();

	SurfaceSculptingUniformBuffer.SafeRelease();

	DomainVoxelInfoUniformBuffer.SafeRelease();

	SprayParticleSpawnEvent1UniformBuffer.SafeRelease();
	SprayParticleSpawnEvent2UniformBuffer.SafeRelease();
	SprayParticleSpawnEvent3UniformBuffer.SafeRelease();

	delete this;
}

void FFluidSimulation::PreSimulate(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef PositionTextureRHI, FTexture2DRHIRef VelocityTextureRHI)
{
	check(IsInRenderingThread());

	SCOPE_CYCLE_COUNTER(STAT_FluidPreSim);
	SCOPED_DRAW_EVENT(RHICmdList, FluidPreSim);
	
	if (FluidConsoleVariables::RunSimProfiler)
	{
		if (TotalSeconds - lastProfileTime > 0.25)
		{
			static IConsoleVariable* CVShowUI = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowUI"));
			static IConsoleVariable* CVScreenshot = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.Screenshot"));
			if (CVShowUI->GetInt() == 0 && CVScreenshot->GetInt() == 0)
			{
				/*
r.ProfileGPU.ShowUI 0
r.ProfileGPU.Screenshot 0
r.ProfileGPU.Root Fluid*
				*/
				GTriggerGPUProfile = true;
			}
			else
			{
				UE_LOG(CataclysmInfo, Log, TEXT("Need , r.ProfileGPU.ShowUI 0, r.ProfileGPU.Screenshot 0, r.ProfileGPU.Root Fluid* to take profiles this way."));
				FluidConsoleVariables::RunSimProfiler = 0;
			}
			lastProfileTime = TotalSeconds;
		}
	}
	else
	{
		lastProfileTime = TotalSeconds;
	}
	UpdateRegionsBuffer();

	UpdateDomainVoxelInfoUniformBuffer();
	UpdateSurfaceSculptingUniformBuffer();
	UpdateSprayParticleSpawnEventUniformBuffers();

	// Make sure the count of particles that we need to simulate is updated.
	// The particle index buffer was filled at the end of last frame with all
	// living particles and all injected and cleared particles.  
	// The sum of the ParticlesToSort->Count (the first MAX_SPLAT particls in each cell) and the ExtraParticleIndices->Count should
	// be the same as the living number of particles we want to process.
	ExtraParticleCount = ExtraParticleIndices->FetchCount();
	SortedParticleCount = ParticlesToSort->FetchCount();

	uint32 ClearValues[4] = { 0 };
	if(!BufferCleared) // only clear at first frame;
	{
		RHICmdList.ClearUAV(BrickMapUAV[FrameIndex ^ 0x1], ClearValues);
		RHICmdList.ClearUAV(TriggerStates[FrameIndex ^ 0x1]->UAV, ClearValues);
		BufferCleared = true;
	}

	// clear brick map texture
	RHICmdList.ClearUAV(BrickMapUAV[FrameIndex], ClearValues);

	RHICmdList.ClearUAV(BrickMapCrossingsUAV, ClearValues);

	// clear the trigger states
	RHICmdList.ClearUAV(TriggerStates[FrameIndex]->UAV, ClearValues);

	TArray <FSparseGrid::TypedVTRStructured*> VTRList;
	if (VelocityWeightsVTR) VTRList.Add(VelocityWeightsVTR);
	if (LiquidBoundaryVTR) VTRList.Add(LiquidBoundaryVTR);
	if (SolidBoundaryVTR) VTRList.Add(SolidBoundaryVTR);
	if (FloatVTR) VTRList.Add(FloatVTR);
 	if (Velocity_U_VTR) VTRList.Add(Velocity_U_VTR);
 	if (Velocity_V_VTR) VTRList.Add(Velocity_V_VTR);
 	if (Velocity_W_VTR) VTRList.Add(Velocity_W_VTR);
	if (SmoothDensityVTR) VTRList.Add(SmoothDensityVTR);
	if (ValidFlagsVTR) VTRList.Add(ValidFlagsVTR);
	if (LiquidSurfaceLS) VTRList.Add(LiquidSurfaceLS);
#if LS_MULTIPLIER == 2
	if (Uint32LS) VTRList.Add(Uint32LS);
#endif

	// fetch tile list from GPU to CPU
	void* BrickListData = nullptr;
	uint32 NumChangedBricks = 0;
	if (ChangedBricksListFromGPU->FetchData(BrickListData, NumChangedBricks))
	{
		// FetchData() return true only when NumChangedBricks > 0
		SCOPED_DRAW_EVENT(RHICmdList, FluidMapTiledResources);
		SparseGrid->ProcessActiveBricksFromGPU((FBrickIndex*)BrickListData, NumChangedBricks);
		if (SparseGrid->UnmappedCount)
		{
			SCOPED_DRAW_EVENT(RHICmdList, FluidZeroUnmappedBricks);
			ZeroBricks<ZBD_unmapped>(RHICmdList, VTRList, NumChangedBricks, ChangedBricksListFromGPU->GetSRV());
		}
		SparseGrid->UpdateTiledResources(RHICmdList, VTRList);
	}
	else if(NumChangedBricks)
	{
		// If the number of changed bricks was too high, then maybe a simulation was removed, or maybe the sim is exploding.
		// If the sim is exploding, we can't do much easily.  But if it was from mass particle removal, we can try to recover
		// by just removing all the bricks.

		// first clear the previous brick map, so we don't run into this again.
		RHICmdList.ClearUAV(BrickMapUAV[FrameIndex ^ 0x1], ClearValues);
		// zero out all the tiles resources.
		for (auto VTR : VTRList)
		{
			RHICmdList.ClearUAV(VTR->UAV, ClearValues);
		}
		// fake the data so we can unmap all previously mapped bricks.
		TArray<FBrickIndex> nullArray;
		for (auto idx : SparseGrid->ActiveBricks)
		{
			nullArray.Add(idx.Index | 0x80000000);
		}
		SparseGrid->ProcessActiveBricksFromGPU(nullArray.GetData(), nullArray.Num());
		SparseGrid->UpdateTiledResources(RHICmdList, VTRList);
	}

	if (FluidRegions && FluidTriggerCount > 0)
	{
		void* ChangedTriggerListData = nullptr;
		uint32 NumChangedTriggers = 0;
		if (ChangedTriggers->FetchData(ChangedTriggerListData, NumChangedTriggers))
		{
			NotifyActors((uint32*)ChangedTriggerListData, NumChangedTriggers);
		}
	}
//	if (FluidConsoleVariables::Pause) RadixSort->Test(RHICmdList);

	uint32 LivingParticleCount = ExtraParticleCount + SortedParticleCount;
	//generate texcoords
	if (WavesFFT) // We only need texture coords when we have WaveFFT on.
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidGenerateTexcoords);
		TShaderMapRef<FGenerateTexCoordsCS> GenerateTexCoordsCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(GenerateTexCoordsCS->GetComputeShader());
		GenerateTexCoordsCS->SetOutput(RHICmdList, ParticlesTexCoords[0].UAV, ParticlesTexCoords[1].UAV);

		FGenerateTexCoordsUniformParameters GenerateTexCoordsParameters;
		GenerateTexCoordsParameters.ParticleCount = LivingParticleCount;
		GenerateTexCoordsParameters.ExtraParticleCount = ExtraParticleCount;
		GenerateTexCoordsParameters.ElapsedSeconds0 = ParticlesTexCoords[0].ElapsedSeconds;
		GenerateTexCoordsParameters.ElapsedSeconds1 = ParticlesTexCoords[1].ElapsedSeconds;
		GenerateTexCoordsParameters.bRegenerate0 = ParticlesTexCoords[0].bRegenerate ? 1 : 0;
		GenerateTexCoordsParameters.bRegenerate1 = ParticlesTexCoords[1].bRegenerate ? 1 : 0;
		FGenerateTexCoordsUniformBufferRef GenerateTexCoordsUniformBuffer = FGenerateTexCoordsUniformBufferRef::CreateUniformBufferImmediate(GenerateTexCoordsParameters, UniformBuffer_SingleFrame);
		// TODO Would this be faster with sorted particle indices
		GenerateTexCoordsCS->SetParameters(RHICmdList, PositionTextureRHI, VelocityTextureRHI, ExtraParticleIndices->GetSRV(), ParticlesToSort->GetSRV(), GenerateTexCoordsUniformBuffer);

		const uint32 GroupCount = FMath::Max<uint32>(1, (LivingParticleCount + CREATE_BRICK_MAP_THREADS_COUNT - 1) / CREATE_BRICK_MAP_THREADS_COUNT);

		DispatchComputeShader(RHICmdList, *GenerateTexCoordsCS, GroupCount, 1, 1);
		GenerateTexCoordsCS->UnbindBuffers(RHICmdList);
	}

	static uint32 MaxNumChangedBricks = 0;
	static bool MaxNumBricksUpdated = false;
	if (NumChangedBricks > MaxNumChangedBricks)
	{
		MaxNumChangedBricks = NumChangedBricks;
		MaxNumBricksUpdated = true;
	}
	// even and odd voxels
	int32 NumSolvedVoxels = LiquidVoxels->FetchCount();
	FluidStats.Update(LivingParticleCount, SparseGrid->NumActiveBricks(), NumSolvedVoxels);
	if (!FluidConsoleVariables::RunSimProfiler && !(frameCount % 60))
	{
		int32 lpcK = LivingParticleCount % 1000;
		int32 lpcM = (LivingParticleCount / 1000) % 1000;
		if (LivingParticleCount >= 1000000)
		{
			UE_LOG(CataclysmInfo, Log, TEXT("LivingParticleCount = %d,%03d,%03d"), LivingParticleCount / 1000000, lpcM, lpcK);
		}
		else if (LivingParticleCount >= 1000)
		{
			UE_LOG(CataclysmInfo, Log, TEXT("LivingParticleCount = %5d,%03d"), LivingParticleCount / 1000, lpcK);
		}
		else
		{
			UE_LOG(CataclysmInfo, Log, TEXT("LivingParticleCount = %9d"), LivingParticleCount);
		}

		if (MaxNumBricksUpdated)
		{
			MaxNumBricksUpdated = false;
			UE_LOG(CataclysmInfo, Warning, TEXT("MaxNumChangedBricks = %d"), MaxNumChangedBricks);
		}
	}
}

void FFluidSimulation::Simulate(
	FRHICommandListImmediate& RHICmdList,
	float DeltaSeconds,
	const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
	FTexture2DRHIRef PrevPositionTextureRHI,
	FTexture2DRHIRef PrevVelocityTextureRHI,
	FTexture2DRHIRef CurFLIPVelocityTextureRHI)
{
	check(IsInRenderingThread());

	// At the end of the solve, make sure these are correct so the particle shaders can use them.
	GridVelocityU = nullptr;
	GridVelocityV = nullptr;
	GridVelocityW = nullptr;


	//generate particles grid
	SortParticles(RHICmdList, PrevPositionTextureRHI);

	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidPosInVox);

		IndirectDispatchFillParameter<32>(RHICmdList, FirstParticleInVoxel);
		TShaderMapRef<FPosInVoxCS> PosInVoxCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(PosInVoxCS->GetComputeShader());
		PosInVoxCS->SetOutput(RHICmdList, ParticlePositionInVox.UAV);
		PosInVoxCS->SetParameters(RHICmdList,
			FirstParticleInVoxel->GetSRV(),
			FirstParticleInVoxel->CountBuffer->SRV,
			SortedParticlesIndicesSRV,
			PrevPositionTextureRHI,
			DomainVoxelInfoUniformBuffer);

		DispatchIndirectComputeShader(RHICmdList, *PosInVoxCS, FirstParticleInVoxel->DispatchParameters->Buffer, 0);
		PosInVoxCS->UnbindBuffers(RHICmdList);
	}


	{
		ZeroSingleVTR(RHICmdList, Velocity_U_VTR);
		ZeroSingleVTR(RHICmdList, Velocity_V_VTR);
		ZeroSingleVTR(RHICmdList, Velocity_W_VTR);
		ZeroSingleVTR(RHICmdList, SmoothDensityVTR);
		ZeroSingleVTR(RHICmdList, SolidBoundaryVTR);
		ZeroSingleVTR(RHICmdList, LiquidBoundaryVTR);
		IndirectDispatchFillParameter<PARTICLES_TO_GRID_THREADS>(RHICmdList, FirstParticleInVoxel);
		SCOPED_DRAW_EVENT(RHICmdList, FluidVelocityToGrid);
		TShaderMapRef<FVelocityToGridCS> VelocityToGridCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(VelocityToGridCS->GetComputeShader());
		VelocityToGridCS->SetOutput(RHICmdList,
			Velocity_U_VTR->UAV,
			SmoothDensityVTR->UAV,
			Velocity_V_VTR->UAV,
			SolidBoundaryVTR->UAV,
			Velocity_W_VTR->UAV,
			LiquidBoundaryVTR->UAV);

		VelocityToGridCS->SetParameters(RHICmdList,
			FirstParticleInVoxel->GetSRV(),
			FirstParticleInVoxel->CountBuffer->SRV,
			SortedParticlesIndicesSRV,
			ParticlePositionInVox.SRV,
			CurFLIPVelocityTextureRHI,
			SurfaceSculptingUniformBuffer,
			DomainVoxelInfoUniformBuffer);

		DispatchIndirectComputeShader(RHICmdList, *VelocityToGridCS, FirstParticleInVoxel->DispatchParameters->Buffer, 0);

		VelocityToGridCS->UnbindBuffers(RHICmdList);
	}

	// Stamp in the fluid field shapes, and blend them into the splat
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidNormalizeVelocityAndSplatFields);
		TShaderMapRef<FNormalizeVelocityAndSplatFieldsCS> NormalizeVelocityAndSplatFieldsCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(NormalizeVelocityAndSplatFieldsCS->GetComputeShader());
		NormalizeVelocityAndSplatFieldsCS->SetOutput(RHICmdList,
			Velocity_U_VTR->UAV,
			Velocity_V_VTR->UAV,
			Velocity_W_VTR->UAV,
			ValidFlagsVTR->UAV);

		NormalizeVelocityAndSplatFieldsCS->SetParameters(RHICmdList,
			SmoothDensityVTR->Texture3D,
			SolidBoundaryVTR->Texture3D,
			LiquidBoundaryVTR->Texture3D,
			DomainVoxelInfoUniformBuffer,
			SparseGrid->ActiveBricksOnGPU->GetSRV());

		DispatchComputeShader(
			RHICmdList,
			*NormalizeVelocityAndSplatFieldsCS,
			SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,
			BRICK_SIZE_Y / CTA_SIZE_Y,
			BRICK_SIZE_Z / CTA_SIZE_Z);
		NormalizeVelocityAndSplatFieldsCS->UnbindBuffers(RHICmdList);
	}

	ExtendScalarVelocity(RHICmdList, FluidConsoleVariables::ExtendVelocityCount, Velocity_U_VTR, Velocity_V_VTR, Velocity_W_VTR, ValidFlagsVTR, SmoothDensityVTR, VelocityWeightsVTR);

	{
		SCOPE_CYCLE_COUNTER(STAT_FluidBuildSolidBoundary);
		SCOPED_DRAW_EVENT(RHICmdList, FluidBuildSolidBoundary);
		TShaderMapRef<FBuildSolidBoundaryCS> BuildSolidBoundaryCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(BuildSolidBoundaryCS->GetComputeShader());
		BuildSolidBoundaryCS->SetOutput(RHICmdList, SolidBoundaryVTR->UAV);
		BuildSolidBoundaryCS->SetParameters(RHICmdList,
			SparseGrid->ActiveBricksOnGPU->GetSRV(), GlobalDistanceFieldParameterData, DomainVoxelInfoUniformBuffer);

		DispatchComputeShader(
			RHICmdList,
			*BuildSolidBoundaryCS,
			SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,
			BRICK_SIZE_Y / CTA_SIZE_Y,
			BRICK_SIZE_Z / CTA_SIZE_Z);
		BuildSolidBoundaryCS->UnbindBuffers(RHICmdList);
	}

	{
		ZeroSingleVTR(RHICmdList, SmoothDensityVTR);
		IndirectDispatchFillParameter<PARTICLES_TO_GRID_THREADS>(RHICmdList, FirstParticleInVoxel);

		SCOPED_DRAW_EVENT(RHICmdList, FluidDensityToGrid);
#define DensityToGrid_TAMPLATE_CALLER  \
		RHICmdList.SetComputeShader(DensityToGridCS->GetComputeShader());\
		DensityToGridCS->SetOutput(RHICmdList, SmoothDensityVTR->UAV);\
		DensityToGridCS->SetParameters(RHICmdList,\
			FirstParticleInVoxel->GetSRV(),\
			FirstParticleInVoxel->CountBuffer->SRV,\
			SurfaceSculptingParameters.UseDensityVelocityStretch ? PrevVelocityTextureRHI : nullptr,\
			SurfaceSculptingParameters.UseDensityVelocityStretch ? SortedParticlesIndicesSRV : nullptr,\
			ParticlePositionInVox.SRV,\
			SurfaceSculptingUniformBuffer,\
			DomainVoxelInfoUniformBuffer);\
		DispatchIndirectComputeShader(RHICmdList, *DensityToGridCS, FirstParticleInVoxel->DispatchParameters->Buffer, 0);\
		DensityToGridCS->UnbindBuffers(RHICmdList)
		if (SurfaceSculptingParameters.UseDensityVelocityStretch)
		{
			TShaderMapRef<FDensityToGridCS<true>> DensityToGridCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			DensityToGrid_TAMPLATE_CALLER;
		}
		else
		{
			TShaderMapRef<FDensityToGridCS<false>> DensityToGridCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			DensityToGrid_TAMPLATE_CALLER;
		}
#undef DensityToGrid_TAMPLATE_CALLER
	}
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidNewSurface);

		// for now, if we have a negative density, consider that a flag to use particle radis splatting.
		if (SurfaceAtDensity <= 0)
		{
			ZeroSingleVTR(RHICmdList, VelocityWeightsVTR);
			IndirectDispatchFillParameter<PARTICLES_TO_GRID_THREADS>(RHICmdList, FirstParticleInVoxel);
			{
				SCOPED_DRAW_EVENT(RHICmdList, FluidParticlesToGrid);
#define ParticlesToGrid_TAMPLATE_CALLER  \
				RHICmdList.SetComputeShader(ParticlesToGridCS->GetComputeShader());\
				ParticlesToGridCS->SetOutput(RHICmdList,\
					VelocityWeightsVTR->UAV);\
				ParticlesToGridCS->SetParameters(RHICmdList,\
					FirstParticleInVoxel->GetSRV(),\
					FirstParticleInVoxel->CountBuffer->SRV,\
					SurfaceSculptingParameters.UseDensityVelocityStretch ? PrevVelocityTextureRHI : nullptr,\
					SurfaceSculptingParameters.UseDensityVelocityStretch ? SortedParticlesIndicesSRV : nullptr,\
					ParticlePositionInVox.SRV,\
					SurfaceSculptingUniformBuffer,\
					DomainVoxelInfoUniformBuffer,\
					SmoothDensityVTR->Texture3D);\
				DispatchIndirectComputeShader(RHICmdList, *ParticlesToGridCS, FirstParticleInVoxel->DispatchParameters->Buffer, 0);\
				ParticlesToGridCS->UnbindBuffers(RHICmdList)
				if (SurfaceSculptingParameters.UseDensityVelocityStretch)
				{
					TShaderMapRef<FParticlesToGridCS<true>> ParticlesToGridCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					ParticlesToGrid_TAMPLATE_CALLER;
				}
				else
				{
					TShaderMapRef<FParticlesToGridCS<false>> ParticlesToGridCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					ParticlesToGrid_TAMPLATE_CALLER;
				}
#undef ParticlesToGrid_TAMPLATE_CALLER
			}

			if (FluidConsoleVariables::DoRepairLevelset)
			{
				SCOPED_DRAW_EVENT(RHICmdList, RepairLevelset);
				RepairLevelset<RL_InUint>(RHICmdList, VelocityWeightsVTR, LiquidBoundaryVTR);
			}
			else
			{
				SCOPED_DRAW_EVENT(RHICmdList, RedistanceLevelset);
				RedistanceLevelset<RL_InUint>(RHICmdList, VelocityWeightsVTR, LiquidBoundaryVTR);
			}
		}
		else
		{
			SCOPED_DRAW_EVENT(RHICmdList, FluidAndoBoundary);
			TShaderMapRef<FAndoBoundaryCS> AndoBoundaryCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			RHICmdList.SetComputeShader(AndoBoundaryCS->GetComputeShader());
			AndoBoundaryCS->SetOutput(RHICmdList,
				LiquidBoundaryVTR->UAV);

			AndoBoundaryCS->SetParameters(RHICmdList,
				SmoothDensityVTR->Texture3D,
				DomainVoxelInfoUniformBuffer,
				SparseGrid->ActiveBricksOnGPU->GetSRV());

			DispatchComputeShader(
				RHICmdList,
				*AndoBoundaryCS,
				SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,
				BRICK_SIZE_Y / CTA_SIZE_Y,
				BRICK_SIZE_Z / CTA_SIZE_Z);
			AndoBoundaryCS->UnbindBuffers(RHICmdList);
		}
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, RedistanceLiquidBoundary);
		RedistanceLevelset<RL_UseCrude|RL_OutUint>(RHICmdList, LiquidBoundaryVTR, VelocityWeightsVTR);
		RedistanceLevelset<RL_Normal|RL_InUint>(RHICmdList, VelocityWeightsVTR, LiquidBoundaryVTR);
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidPrepLevelsetSplat);

		TShaderMapRef<FPrepLevelsetSplatCS> PrepLevelsetSplatCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
#if LS_MULTIPLIER == 1
		// If we don't have a high resolution levelset, we can use a differnt temp uint for the fluid surface splatting.  Because Uint32LS is not allocated.
		Uint32LS = VelocityWeightsVTR;
#endif
		RHICmdList.SetComputeShader(PrepLevelsetSplatCS->GetComputeShader());
		PrepLevelsetSplatCS->SetOutput(RHICmdList,
			Uint32LS->UAV);
		PrepLevelsetSplatCS->SetParameters(RHICmdList,
			LiquidBoundaryVTR->Texture3D,
			SurfaceSculptingUniformBuffer,
			SparseGrid->ActiveBricksOnGPU->GetSRV());

		DispatchComputeShader(
			RHICmdList,
			*PrepLevelsetSplatCS,
			SparseGrid->NumActiveBricks()*LS_BRICK_SIZE_X / CTA_SIZE_X,
			LS_BRICK_SIZE_Y / CTA_SIZE_Y,
			LS_BRICK_SIZE_Z / CTA_SIZE_Z);
		PrepLevelsetSplatCS->UnbindBuffers(RHICmdList);
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidLevelsetSplat);

		IndirectDispatchFillParameter<LEVELSET_SPLAT_THREADS>(RHICmdList, FirstParticleInVoxel);

#define LevelsetSplat_TAMPLATE_CALLER  \
		RHICmdList.SetComputeShader(LevelsetSplatCS->GetComputeShader());\
		LevelsetSplatCS->SetOutput(RHICmdList, Uint32LS->UAV);\
		LevelsetSplatCS->SetParameters(RHICmdList,\
			FirstParticleInVoxel->GetSRV(),\
			FirstParticleInVoxel->CountBuffer->SRV,\
			SmoothDensityVTR->Texture3D,\
			LiquidBoundaryVTR->Texture3D,\
			SurfaceSculptingParameters.UseVelocityStretch ? PrevVelocityTextureRHI : nullptr, \
			SurfaceSculptingParameters.UseVelocityStretch ? SortedParticlesIndicesSRV : nullptr, \
			ParticlePositionInVox.SRV,\
			SurfaceSculptingUniformBuffer,\
			DomainVoxelInfoUniformBuffer);\
		DispatchIndirectComputeShader(RHICmdList, *LevelsetSplatCS, FirstParticleInVoxel->DispatchParameters->Buffer, 0);\
		LevelsetSplatCS->UnbindBuffers(RHICmdList)
		if (SurfaceSculptingParameters.UseVelocityStretch)
		{
			TShaderMapRef<FLevelsetSplatCS<true>> LevelsetSplatCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			LevelsetSplat_TAMPLATE_CALLER;
		}
		else
		{
			TShaderMapRef<FLevelsetSplatCS<false>> LevelsetSplatCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			LevelsetSplat_TAMPLATE_CALLER;
		}
#undef LevelsetSplat_TAMPLATE_CALLER

	}
	if (FluidConsoleVariables::DoRepairLevelset)
	{
		SCOPED_DRAW_EVENT(RHICmdList, RepairLiquidSurface);
		RepairLevelset<RL_InUint | RL_Levelset>(RHICmdList, Uint32LS, LiquidSurfaceLS);
		RepairLevelset<RL_OutUint | RL_Levelset>(RHICmdList, LiquidSurfaceLS, Uint32LS);
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, RedistanceLiquidSurface);
		RedistanceLevelset<RL_InUint | RL_UseCrude | RL_Levelset>(RHICmdList, Uint32LS, LiquidSurfaceLS);
		RedistanceLevelset<RL_OutUint | RL_UseCrude | RL_Levelset>(RHICmdList, LiquidSurfaceLS, Uint32LS);
		RedistanceLevelset<RL_InUint | RL_Levelset>(RHICmdList, Uint32LS, LiquidSurfaceLS);
	}
#if LS_MULTIPLIER == 1
	Uint32LS = nullptr; // careful we don't use this...
#endif
	/// TODO hold this off as long as possible to use VelocityWeights as a Uint Temp.
	ComputeVelocityWeights(RHICmdList, SolidBoundaryVTR, VelocityWeightsVTR);

	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidExtendLiquidIntoSolid);
		TShaderMapRef<FExtendLiquidIntoSolidCS> ExtendLiquidIntoSolidCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(ExtendLiquidIntoSolidCS->GetComputeShader());
		ExtendLiquidIntoSolidCS->SetOutput(RHICmdList, LiquidBoundaryVTR->UAV);
		ExtendLiquidIntoSolidCS->SetParameters(RHICmdList, SparseGrid->ActiveBricksOnGPU->GetSRV(), SolidBoundaryVTR->Texture3D);

		DispatchComputeShader(
			RHICmdList,
			*ExtendLiquidIntoSolidCS,
			SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,
			BRICK_SIZE_Y / CTA_SIZE_Y,
			BRICK_SIZE_Z / CTA_SIZE_Z);
		ExtendLiquidIntoSolidCS->UnbindBuffers(RHICmdList);
	}

	// Needs the splatted velocity, and the solid boundary and velocity weights, all of which are used a lost for temp work earlier.
	ConstrainScalarVelocity(RHICmdList, true, SolidBoundaryVTR, VelocityWeightsVTR, ValidFlagsVTR, Velocity_U_VTR, Velocity_V_VTR, Velocity_W_VTR);

	// have to do VelAt Pos for both Sorted and extra.
 	{
		///TODO: Use the count buffer here instead of fetching to save GPU -> CPU copy?
		SCOPED_DRAW_EVENT(RHICmdList, FluidVelAtPos);
		int32 LivingParticleCount = SortedParticleCount + ExtraParticleCount;
		// Create the uniform buffer.
		FParticleCountUniformParameters ParticleCountParameters;
		FParticleCountUniformBufferRef ParticleCountUniformBuffer;
		ParticleCountParameters.ParticleCount = LivingParticleCount;
		ParticleCountParameters.ExtraParticleCount = ExtraParticleCount;
		ParticleCountUniformBuffer = FParticleCountUniformBufferRef::CreateUniformBufferImmediate(ParticleCountParameters, UniformBuffer_SingleDraw);

		TShaderMapRef<FVelAtPosCS> VelAtPosCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(VelAtPosCS->GetComputeShader());
		VelAtPosCS->SetOutput(RHICmdList, PreSolveParticleVelocityUAV);
		VelAtPosCS->SetParameters(RHICmdList,
			ParticlesToSort->GetSRV(),
			ExtraParticleIndices->GetSRV(),
			Velocity_U_VTR->Texture3D,
			Velocity_V_VTR->Texture3D,
			Velocity_W_VTR->Texture3D,
			LiquidBoundaryVTR->Texture3D,
			PrevPositionTextureRHI,
			DomainVoxelInfoUniformBuffer,
			ParticleCountUniformBuffer);

		const uint32 GroupCount = FMath::Max<uint32>(1, (LivingParticleCount + GENERATE_PARTICLES_GRID_THREAD_COUNT - 1) / GENERATE_PARTICLES_GRID_THREAD_COUNT);
		DispatchComputeShader(RHICmdList, *VelAtPosCS, GroupCount, 1, 1);
		VelAtPosCS->UnbindBuffers(RHICmdList);
	}

	// Solve the pressure.
	{
		SCOPE_CYCLE_COUNTER(STAT_FluidPressure);
		SCOPED_DRAW_EVENT(RHICmdList, FluidPressure);
		PressureVTR[0] = SmoothDensityVTR;
		PressureVTR[1] = FloatVTR;

		{
			// Fill the voxels that we will solve over.
			SCOPED_DRAW_EVENT(RHICmdList, FluidGetLiquidVoxels);
			TShaderMapRef<FGetLiquidVoxelsCS> GetLiquidVoxelsCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			RHICmdList.SetComputeShader(GetLiquidVoxelsCS->GetComputeShader());
			GetLiquidVoxelsCS->SetOutput(RHICmdList,
				LiquidVoxels->GetUAV(),
				PressureVTR[0]->UAV,
				PressureVTR[1]->UAV);

			GetLiquidVoxelsCS->SetParameters(RHICmdList,
				LiquidBoundaryVTR->Texture3D,
				VelocityWeightsVTR->Texture3D,
				SparseGrid->ActiveBricksOnGPU->GetSRV());

			DispatchComputeShader(
				RHICmdList,
				*GetLiquidVoxelsCS,
				SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,
				BRICK_SIZE_Y / CTA_SIZE_Y,
				BRICK_SIZE_Z / CTA_SIZE_Z);
			GetLiquidVoxelsCS->UnbindBuffers(RHICmdList);
		}
		
		IndirectDispatchFillParameter<PRESSURE_SOLVE_THREADS>(RHICmdList, LiquidVoxels);
		{
			SCOPED_DRAW_EVENT(RHICmdList, FluidDivergence);
			TShaderMapRef<FDivergenceLiquidCS> DivergenceLiquidCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			RHICmdList.SetComputeShader(DivergenceLiquidCS->GetComputeShader());
			DivergenceLiquidCS->SetOutput(RHICmdList,
				PressureSolverParams.UAV); // Fill this up for use
			DivergenceLiquidCS->SetParameters(RHICmdList,
				LiquidVoxels->GetSRV(),
				LiquidVoxels->CountBuffer->SRV,
				Velocity_U_VTR->Texture3D,
				Velocity_V_VTR->Texture3D,
				Velocity_W_VTR->Texture3D,
				LiquidBoundaryVTR->Texture3D,
				VelocityWeightsVTR->Texture3D,
				DomainVoxelInfoUniformBuffer);

			DispatchIndirectComputeShader(RHICmdList, *DivergenceLiquidCS, LiquidVoxels->DispatchParameters->Buffer, 0);
			DivergenceLiquidCS->UnbindBuffers(RHICmdList);
		}
		{
			SCOPED_DRAW_EVENT(RHICmdList, FluidJacobi);

			TShaderMapRef<FPressureJacobiCS> PressureJacobiCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			RHICmdList.SetComputeShader(PressureJacobiCS->GetComputeShader());

			PressureJacobiCS->SetParameters(RHICmdList,
				PressureSolverParams.SRV,
				LiquidVoxels->GetSRV(),
				LiquidVoxels->CountBuffer->SRV);

			int32 NumIterations = FluidConsoleVariables::NumPressureIter & 0x000ffffe;// needs to be even and reasonable
			uint8 src = 0;
			uint8 dst = 1;
			for (int32 i = 0; i < NumIterations; ++i)
			{
				PressureJacobiCS->SetInput(RHICmdList, PressureVTR[src]->Texture3D);
				PressureJacobiCS->SetOutput(RHICmdList, PressureVTR[dst]->UAV);
				DispatchIndirectComputeShader(RHICmdList, *PressureJacobiCS, LiquidVoxels->DispatchParameters->Buffer, 0);
				PressureJacobiCS->UnbindBuffers(RHICmdList);
				src ^= 1;
				dst ^= 1;
			}
			PressureJacobiCS->UnbindSRV(RHICmdList);
		}
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_FluidProjectVelocity);
		SCOPED_DRAW_EVENT(RHICmdList, FluidProjectVelocity);
		TShaderMapRef<FProjectVelocityCS> ProjectVelocityCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(ProjectVelocityCS->GetComputeShader());
		ProjectVelocityCS->SetOutput(RHICmdList,
			Velocity_U_VTR->UAV,
			Velocity_V_VTR->UAV,
			Velocity_W_VTR->UAV,
			ValidFlagsVTR->UAV);
		ProjectVelocityCS->SetParameters(RHICmdList,
			LiquidBoundaryVTR->Texture3D,
			PressureVTR[0]->Texture3D,
			VelocityWeightsVTR->Texture3D,
			SparseGrid->ActiveBricksOnGPU->GetSRV(), DomainVoxelInfoUniformBuffer);

		DispatchComputeShader(
			RHICmdList,
			*ProjectVelocityCS,
			SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,
			BRICK_SIZE_Y / CTA_SIZE_Y,
			BRICK_SIZE_Z / CTA_SIZE_Z);
		ProjectVelocityCS->UnbindBuffers(RHICmdList);
	}
	PressureVTR[0] = nullptr;
	PressureVTR[1] = nullptr;

	ExtendScalarVelocity(RHICmdList, FluidConsoleVariables::ExtendVelocityCount, Velocity_U_VTR, Velocity_V_VTR, Velocity_W_VTR, ValidFlagsVTR, SmoothDensityVTR, VelocityWeightsVTR);

	ConstrainScalarVelocity(RHICmdList, false, SolidBoundaryVTR, nullptr, ValidFlagsVTR, Velocity_U_VTR, Velocity_V_VTR, Velocity_W_VTR);

	GridVelocityU = Velocity_U_VTR;
	GridVelocityV = Velocity_V_VTR;
	GridVelocityW = Velocity_W_VTR;
	ParticlesPositionTexture = PrevPositionTextureRHI;

	if (DiffuseParticles != nullptr)
	{
		////TODO update this to add work for the extra particles as well, and not just the sorted ones?
		SCOPED_DRAW_EVENT(RHICmdList, FluidDiffuseParticles);
		DiffuseParticles->Simulate(RHICmdList, DeltaSeconds,
			SortedParticleCount,
			LiquidSurfaceLS->Texture3D,
			SolidBoundaryVTR->Texture3D,
			GridVelocityU->Texture3D,
			GridVelocityV->Texture3D,
			GridVelocityW->Texture3D,
			SortedParticlesIndicesSRV,
			PrevPositionTextureRHI,
			PrevVelocityTextureRHI,
			DomainVoxelInfoUniformBuffer
			);
	}
}

template <uint32 GroupCount>
void FFluidSimulation::IndirectDispatchFillParameter(FRHICommandListImmediate& RHICmdList, FIndirectAppendBuffer *IndirectAppendBuffer)
{
//	SCOPED_DRAW_EVENT(RHICmdList, FluidIndirectDispatchFillParameter);
	if (IndirectAppendBuffer->DataIsReady && GroupCount == IndirectAppendBuffer->CurrentDispatchGroupCount) return;
	IndirectAppendBuffer->CopyCount();
	TShaderMapRef<FIndirectDispatchFillParameterCS<GroupCount>> IndirectDispatchFillParameterCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(IndirectDispatchFillParameterCS->GetComputeShader());
	IndirectDispatchFillParameterCS->SetParameters(RHICmdList, 1.0f, IndirectAppendBuffer->CountBuffer->SRV);
	IndirectDispatchFillParameterCS->SetOutput(RHICmdList, IndirectAppendBuffer->DispatchParameters->UAV);

	DispatchComputeShader(RHICmdList, *IndirectDispatchFillParameterCS, 1, 1, 1);
	IndirectDispatchFillParameterCS->UnbindBuffers(RHICmdList);
	IndirectAppendBuffer->CurrentDispatchGroupCount = GroupCount;
}

int32 FFluidSimulation::SortParticles(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef ParticlePositionTextureRHI)
{
	SCOPED_DRAW_EVENT(RHICmdList, FluidSortParticles);

	// Set up the count uniform buffer.
	FParticleCountUniformParameters ParticleCountParameters;
	FParticleCountUniformBufferRef ParticleCountUniformBuffer;
	///TODO: Use the count buffer here instead of fetching to save GPU -> CPU copy?
	int32 NumToSort = SortedParticleCount;
	// Create the uniform buffer.
	ParticleCountParameters.ParticleCount = NumToSort;
	ParticleCountUniformBuffer = FParticleCountUniformBufferRef::CreateUniformBufferImmediate(ParticleCountParameters, UniformBuffer_SingleFrame);

	const FRWBufferStructured* InputKeys;
	const FRWBufferStructured* InputValues;
	RadixSort->GetInputKeysAndValues(InputKeys, InputValues);
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidGenerateKeysAndValues);
		TShaderMapRef<FGenerateKeysAndValuesCS> GenerateKeysAndValuesCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(GenerateKeysAndValuesCS->GetComputeShader());
		GenerateKeysAndValuesCS->SetOutput(RHICmdList, InputKeys->UAV, InputValues->UAV);
		GenerateKeysAndValuesCS->SetParameters(RHICmdList, ParticlePositionTextureRHI, ParticlesToSort->GetSRV(), DomainVoxelInfoUniformBuffer, ParticleCountUniformBuffer);

		const uint32 GroupCount = FMath::Max<uint32>(1, (NumToSort + GENERATE_PARTICLES_GRID_THREAD_COUNT - 1) / GENERATE_PARTICLES_GRID_THREAD_COUNT);
		DispatchComputeShader(RHICmdList, *GenerateKeysAndValuesCS, GroupCount, 1, 1);
		GenerateKeysAndValuesCS->UnbindBuffers(RHICmdList);
	}

	const FRWBufferStructured* SortedKeys;
	const FRWBufferStructured* SortedValues;
	RadixSort->Sort(RHICmdList, NumToSort, 0, 32, SortedKeys, SortedValues);
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidGenerateCellStarts);
		TShaderMapRef<FGenerateCellRangesCS> GenerateCellRangesCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(GenerateCellRangesCS->GetComputeShader());
		GenerateCellRangesCS->SetOutput(RHICmdList, FirstParticleInVoxel->GetUAV());
		GenerateCellRangesCS->SetParameters(RHICmdList, SortedKeys->SRV, ParticleCountUniformBuffer);
		const uint32 GroupCount = FMath::Max<uint32>(1, (NumToSort + GENERATE_PARTICLES_GRID_THREAD_COUNT - 1) / GENERATE_PARTICLES_GRID_THREAD_COUNT);
		DispatchComputeShader(RHICmdList, *GenerateCellRangesCS, GroupCount, 1, 1);
		GenerateCellRangesCS->UnbindBuffers(RHICmdList);
	}
	SortedParticlesIndicesSRV = SortedValues->SRV;
	return NumToSort;
}

void FFluidSimulation::PostSimulate(FRHICommandListImmediate& RHICmdList,
	const TArray<FGPUParticleVertexBuffer*>& GPUParticleVertexBuffers,
	FTexture2DRHIRef PositionTextureRHI, float DeltaSeconds)
{
	SCOPED_DRAW_EVENT(RHICmdList, FluidPostSimulate);
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidMarkLevelsetCrossings);
		TShaderMapRef<FMarkLevelsetCrossingsCS> MarkLevelsetCrossingsCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(MarkLevelsetCrossingsCS->GetComputeShader());

		MarkLevelsetCrossingsCS->SetOutput(RHICmdList, BrickMapCrossingsUAV);
		MarkLevelsetCrossingsCS->SetParameters(RHICmdList,
			LiquidSurfaceLS->Texture3D,
			SparseGrid->ActiveBricksOnGPU->GetSRV());

		DispatchComputeShader(
			RHICmdList,
			*MarkLevelsetCrossingsCS,
			SparseGrid->NumActiveBricks()*LS_BRICK_SIZE_X / CTA_SIZE_X,
			LS_BRICK_SIZE_Y / CTA_SIZE_Y,
			LS_BRICK_SIZE_Z / CTA_SIZE_Z);
		MarkLevelsetCrossingsCS->UnbindBuffers(RHICmdList);
	}

	if (FluidRegions && FluidTriggerCount > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_FluidTriggers);
		SCOPED_DRAW_EVENT(RHICmdList, FluidTriggers);

		TShaderMapRef<FTriggerOverlapCS> TriggerOverlapCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(TriggerOverlapCS->GetComputeShader());

		TriggerOverlapCS->SetParameters(RHICmdList, LiquidSurfaceLS->Texture3D, TriggerStates[FrameIndex ^ 0x1]->SRV, DomainVoxelInfoUniformBuffer, FluidTriggerUniformBuffer);
		TriggerOverlapCS->SetOutput(RHICmdList, TriggerStates[FrameIndex]->UAV, ChangedTriggers->GetUAV());

		DispatchComputeShader(
			RHICmdList,
			*TriggerOverlapCS,
			1,
			1,
			1);
		TriggerOverlapCS->UnbindBuffers(RHICmdList);
		ChangedTriggers->CopyData();
	}

	CreateBrickMap(RHICmdList, GPUParticleVertexBuffers, PositionTextureRHI);

	for (int i = 0; i < PARTICLES_TEX_COORDS_COUNT; ++i)
	{
		//we need prev time weight since we use prev particles positions
		ParticlesTexCoords[i].Weight = 0.5*(1 - FMath::Cos(2 * PI * ParticlesTexCoords[i].ElapsedSeconds / TexCoordsRegeneratePeriod));

		ParticlesTexCoords[i].ElapsedSeconds += DeltaSeconds;
		ParticlesTexCoords[i].bRegenerate = false;
		if (ParticlesTexCoords[i].ElapsedSeconds >= TexCoordsRegeneratePeriod)
		{
			ParticlesTexCoords[i].ElapsedSeconds -= TexCoordsRegeneratePeriod;
			ParticlesTexCoords[i].bRegenerate = true;
		}
	}

	if (WavesFFT)
	{
		WavesTexture = WavesFFT->Update(RHICmdList, TotalSeconds);
	}
	FluidConsoleVariables::OutputDebugData = 0;
	FrameIndex ^= 0x1;
	TotalSeconds += DeltaSeconds;
	LastDeltaSeconds = DeltaSeconds;
	frameCount++;
}

void FFluidSimulation::ComputeVelocityWeights(FRHICommandListImmediate& RHICmdList,
	FSparseGrid::TypedVTRStructured* InNodalLevelset,
	FSparseGrid::TypedVTRStructured* OutWeights)
{
	SCOPED_DRAW_EVENT(RHICmdList, FluidComputeVelocityWeights);
	TShaderMapRef<FComputeVelocityWeightsCS> ComputeVelocityWeightsCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(ComputeVelocityWeightsCS->GetComputeShader());
	ComputeVelocityWeightsCS->SetOutput(RHICmdList,
		OutWeights->UAV);
	ComputeVelocityWeightsCS->SetParameters(RHICmdList,
		InNodalLevelset->Texture3D,
		SparseGrid->ActiveBricksOnGPU->GetSRV());

	DispatchComputeShader(
		RHICmdList,
		*ComputeVelocityWeightsCS,
		SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,
		BRICK_SIZE_Y / CTA_SIZE_Y,
		BRICK_SIZE_Z / CTA_SIZE_Z);
	ComputeVelocityWeightsCS->UnbindBuffers(RHICmdList);
}

void FFluidSimulation::ConstrainScalarVelocity(FRHICommandListImmediate& RHICmdList,
	bool GetFaces,
	FSparseGrid::TypedVTRStructured* InNodalLevelset,
	FSparseGrid::TypedVTRStructured* InWeights,
	FSparseGrid::TypedVTRStructured* InValidFlags,
	FSparseGrid::TypedVTRStructured* InOutU,
	FSparseGrid::TypedVTRStructured* InOutV,
	FSparseGrid::TypedVTRStructured* InOutW)
{
	SCOPED_DRAW_EVENT(RHICmdList, FluidConstrainScalarVelocityWithSolidBoundary);
	if (GetFaces)
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidConstrainGetFaces);
		TShaderMapRef<FConstrainGetFacesCS> ConstrainGetFacesCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(ConstrainGetFacesCS->GetComputeShader());
		ConstrainGetFacesCS->SetOutput(RHICmdList, FacesToConstrain->GetUAV());
		ConstrainGetFacesCS->SetParameters(RHICmdList,
			InValidFlags->Texture3D,
			InWeights->Texture3D,
			SparseGrid->ActiveBricksOnGPU->GetSRV());
		DispatchComputeShader(
			RHICmdList,
			*ConstrainGetFacesCS,
			SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,
			BRICK_SIZE_Y / CTA_SIZE_Y,
			BRICK_SIZE_Z / CTA_SIZE_Z);
		ConstrainGetFacesCS->UnbindBuffers(RHICmdList);

		IndirectDispatchFillParameter<128>(RHICmdList, FacesToConstrain);
	}
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidConstrainFillVelocityBuffer);
		TShaderMapRef<FConstrainFillVelocityBufferCS> ConstrainFillVelocityBufferCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(ConstrainFillVelocityBufferCS->GetComputeShader());
		ConstrainFillVelocityBufferCS->SetOutput(RHICmdList,
			ConstrainVelocityParams.UAV);
		ConstrainFillVelocityBufferCS->SetParameters(RHICmdList,
			FacesToConstrain->GetSRV(),
			FacesToConstrain->CountBuffer->SRV,
			InOutU->Texture3D,
			InOutV->Texture3D,
			InOutW->Texture3D,
			InValidFlags->Texture3D);

		DispatchIndirectComputeShader(RHICmdList, *ConstrainFillVelocityBufferCS, FacesToConstrain->DispatchParameters->Buffer, 0);
		ConstrainFillVelocityBufferCS->UnbindBuffers(RHICmdList);
	}
	TShaderMapRef<FConstrainScalarVelocityWithSolidBoundaryCS> ConstrainScalarVelocityWithSolidBoundaryCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(ConstrainScalarVelocityWithSolidBoundaryCS->GetComputeShader());
	ConstrainScalarVelocityWithSolidBoundaryCS->SetOutput(RHICmdList,
		InOutU->UAV,
		InOutV->UAV,
		InOutW->UAV);
	ConstrainScalarVelocityWithSolidBoundaryCS->SetParameters(RHICmdList,
		ConstrainVelocityParams.SRV,
		FacesToConstrain->GetSRV(),
		FacesToConstrain->CountBuffer->SRV,
		InNodalLevelset->Texture3D);

	DispatchIndirectComputeShader(RHICmdList, *ConstrainScalarVelocityWithSolidBoundaryCS, FacesToConstrain->DispatchParameters->Buffer, 0);
	ConstrainScalarVelocityWithSolidBoundaryCS->UnbindBuffers(RHICmdList);
}

void FFluidSimulation::ExtendScalarVelocity(FRHICommandListImmediate& RHICmdList,
	int32 Count,
	FSparseGrid::TypedVTRStructured* U,
	FSparseGrid::TypedVTRStructured* V,
	FSparseGrid::TypedVTRStructured* W,
	FSparseGrid::TypedVTRStructured* Valid,
	FSparseGrid::TypedVTRStructured* TempFlt,
	FSparseGrid::TypedVTRStructured* TempValid)
{
	SCOPED_DRAW_EVENT(RHICmdList, FluidExtendScalarVelocity);

#define ExtendScalarVelocity_TAMPLATE_CALLER(FLAG, InFlt, InValid, OutFlt, OutValid)  {\
	TShaderMapRef<FExtendScalarVelocityCS<FLAG>> ExtendScalarVelocityCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));\
	RHICmdList.SetComputeShader(ExtendScalarVelocityCS->GetComputeShader());\
	ExtendScalarVelocityCS->SetOutput(RHICmdList,\
		OutFlt->UAV,\
		OutValid->UAV);\
	ExtendScalarVelocityCS->SetParameters(RHICmdList,\
		InFlt->Texture3D,\
		InValid->Texture3D,\
		SparseGrid->ActiveBricksOnGPU->GetSRV());\
	DispatchComputeShader(\
		RHICmdList,\
		*ExtendScalarVelocityCS,\
		SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,\
		BRICK_SIZE_Y / CTA_SIZE_Y,\
		BRICK_SIZE_Z / CTA_SIZE_Z);\
	ExtendScalarVelocityCS->UnbindBuffers(RHICmdList);\
	}
	for (int32 i = 0; i < Count; ++i)
	{
		ExtendScalarVelocity_TAMPLATE_CALLER(4, U, Valid, TempFlt, TempValid);
		ExtendScalarVelocity_TAMPLATE_CALLER(4, TempFlt, TempValid, U, Valid);
		ExtendScalarVelocity_TAMPLATE_CALLER(2, V, Valid, TempFlt, TempValid);
		ExtendScalarVelocity_TAMPLATE_CALLER(2, TempFlt, TempValid, V, Valid);
		ExtendScalarVelocity_TAMPLATE_CALLER(1, W, Valid, TempFlt, TempValid);
		ExtendScalarVelocity_TAMPLATE_CALLER(1, TempFlt, TempValid, W, Valid);
	}
#undef ExtendScalarVelocity_TAMPLATE_CALLER
}

void FFluidSimulation::CreateBrickMap(
	FRHICommandListImmediate& RHICmdList,
	const TArray<FGPUParticleVertexBuffer*>& GPUParticleVertexBuffers,
	FTexture2DRHIRef PositionTextureRHI)
{
	check(IsInRenderingThread());
	SCOPE_CYCLE_COUNTER(STAT_FluidCreateBrickMap);
	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidCreateBrickMap);
		ZeroSingleVTR(RHICmdList, ValidFlagsVTR);
		// splat into a work buffer, then add the skirt after that.
		TShaderMapRef<FCreateBrickMapCS> CreateBrickMapCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(CreateBrickMapCS->GetComputeShader());
		CreateBrickMapCS->SetOutput(RHICmdList, BrickMapUAV[FrameIndex], BricksWithParticles->GetUAV(), ExtraParticleIndices->GetUAV(), ParticlesToSort->GetUAV(), ValidFlagsVTR->UAV);

		for (int VBIndex = 0; VBIndex < GPUParticleVertexBuffers.Num(); ++VBIndex)
		{
			FParticleCountUniformParameters ParticleCountParameters;
			FParticleCountUniformBufferRef ParticleCountUniformBuffer;
			int32 ParticleCount = GPUParticleVertexBuffers[VBIndex]->ParticleCount;
			const uint32 GroupCount = FMath::Max<uint32>(1, (ParticleCount + CREATE_BRICK_MAP_THREADS_COUNT - 1) / CREATE_BRICK_MAP_THREADS_COUNT);

			// Create the uniform buffer.// TODO can we just use this directly if it is already on the GPU in GPUParticleVertexBuffers[VBIndex]->ParticleCount?
			ParticleCountParameters.ParticleCount = ParticleCount;
			ParticleCountUniformBuffer = FParticleCountUniformBufferRef::CreateUniformBufferImmediate(ParticleCountParameters, UniformBuffer_SingleDraw);

			CreateBrickMapCS->SetParameters(RHICmdList,
				PositionTextureRHI,
				GPUParticleVertexBuffers[VBIndex]->VertexBufferSRV,
				DomainVoxelInfoUniformBuffer,
				ParticleCountUniformBuffer,
				SurfaceSculptingUniformBuffer);

			DispatchComputeShader(
				RHICmdList,
				*CreateBrickMapCS,
				GroupCount,
				1,
				1);
		}
		CreateBrickMapCS->UnbindBuffers(RHICmdList);
		ExtraParticleIndices->CopyCount();// TODO same as next comment.
		ParticlesToSort->CopyCount();// TODO use this like an indirect append buffer instead of a count buffer.
	}
	// Add a skirt to the current brick map, and find the indices for the BricksWithParticles
	{
		SCOPED_DRAW_EVENT(RHICmdList, GetBrickIndexAndAddSkirt);
		IndirectDispatchFillParameter<32>(RHICmdList, BricksWithParticles);
		TShaderMapRef<FAddSkirtCS> IndexAndAddSkirtCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(IndexAndAddSkirtCS->GetComputeShader());
		IndexAndAddSkirtCS->SetOutput(RHICmdList, BrickMapUAV[FrameIndex]);
		IndexAndAddSkirtCS->SetParameters(RHICmdList,
			BricksWithParticles->GetSRV(),
			BricksWithParticles->CountBuffer->SRV);
		DispatchIndirectComputeShader(RHICmdList, *IndexAndAddSkirtCS, BricksWithParticles->DispatchParameters->Buffer, 0);
		IndexAndAddSkirtCS->UnbindBuffers(RHICmdList);
	}

	uint32 TileDispatchX = FMath::Max<uint32>(1, (NUM_BRICKS_X + CTA_SIZE_X - 1) / CTA_SIZE_X);
	uint32 TileDispatchY = FMath::Max<uint32>(1, (NUM_BRICKS_Y + CTA_SIZE_Y - 1) / CTA_SIZE_Y);
	uint32 TileDispatchZ = FMath::Max<uint32>(1, (NUM_BRICKS_Z + CTA_SIZE_Z - 1) / CTA_SIZE_Z);

	{
		SCOPED_DRAW_EVENT(RHICmdList, FluidComputeListFromBrickMap);
		// Copy to staging resource and prepare for readback
		TShaderMapRef<FCalculateBricksCS> CalculateBricksCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(CalculateBricksCS->GetComputeShader());
		CalculateBricksCS->SetParameters(RHICmdList, BrickMapTexture[FrameIndex], BrickMapTexture[FrameIndex ^ 0x1]);
		CalculateBricksCS->SetOutput(RHICmdList, ChangedBricksListFromGPU->GetUAV(), SparseGrid->ActiveBricksOnGPU->GetUAV());
		DispatchComputeShader(
			RHICmdList,
			*CalculateBricksCS,
			TileDispatchX,
			TileDispatchY,
			TileDispatchZ);
		CalculateBricksCS->UnbindBuffers(RHICmdList);
		{
			// TODO Make this copies of the actual tiles that need mapped?
			SCOPED_DRAW_EVENT(RHICmdList, FluidComputeListFromBrickMap_CopyData);
			ChangedBricksListFromGPU->CopyData();
		}
	}
}

template <int32 Mode, class InType, class OutType>
void FFluidSimulation::RedistanceLevelset(FRHICommandListImmediate& RHICmdList, InType* InLevelset, OutType* OutLevelset)
{
	TShaderMapRef<FRedistanceLevelsetCS<Mode>> RedistanceLevelsetCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(RedistanceLevelsetCS->GetComputeShader());
	RedistanceLevelsetCS->SetOutput(RHICmdList,
		OutLevelset->UAV);
	RedistanceLevelsetCS->SetParameters(RHICmdList,
		InLevelset->Texture3D,
		SparseGrid->ActiveBricksOnGPU->GetSRV());
	uint32 brick_size_x = (Mode & RL_Levelset) ? LS_BRICK_SIZE_X : BRICK_SIZE_X;
	uint32 brick_size_y = (Mode & RL_Levelset) ? LS_BRICK_SIZE_Y : BRICK_SIZE_Y;
	uint32 brick_size_z = (Mode & RL_Levelset) ? LS_BRICK_SIZE_Z : BRICK_SIZE_Z;
	
	DispatchComputeShader(
		RHICmdList,
		*RedistanceLevelsetCS,
		SparseGrid->NumActiveBricks()*brick_size_x / CTA_SIZE_X,
		brick_size_y / CTA_SIZE_Y,
		brick_size_z / CTA_SIZE_Z);
	RedistanceLevelsetCS->UnbindBuffers(RHICmdList);
}

template <int32 Mode, class InType, class OutType>
void FFluidSimulation::RepairLevelset(FRHICommandListImmediate& RHICmdList, InType* InLevelset, OutType* OutLevelset)
{
	TShaderMapRef<FRepairLevelsetCS<Mode>> RepairLevelsetCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(RepairLevelsetCS->GetComputeShader());
	RepairLevelsetCS->SetOutput(RHICmdList,
		OutLevelset->UAV);
	RepairLevelsetCS->SetParameters(RHICmdList,
		InLevelset->Texture3D,
		SparseGrid->ActiveBricksOnGPU->GetSRV());
	uint32 brick_size_x = (Mode & RL_Levelset) ? LS_BRICK_SIZE_X : BRICK_SIZE_X;
	uint32 brick_size_y = (Mode & RL_Levelset) ? LS_BRICK_SIZE_Y : BRICK_SIZE_Y;
	uint32 brick_size_z = (Mode & RL_Levelset) ? LS_BRICK_SIZE_Z : BRICK_SIZE_Z;
	
	DispatchComputeShader(
		RHICmdList,
		*RepairLevelsetCS,
		SparseGrid->NumActiveBricks()*brick_size_x / CTA_SIZE_X,
		brick_size_y / CTA_SIZE_Y,
		brick_size_z / CTA_SIZE_Z);
	RepairLevelsetCS->UnbindBuffers(RHICmdList);
}

template <EZeroBricksDataMode dataMode>
void FFluidSimulation::ZeroBricks(FRHICommandListImmediate& RHICmdList, const TArray<FSparseGrid::TypedVTRStructured*>& VTRs, int32 NumBricks, FShaderResourceViewRHIParamRef BrickListRHI)
{
	// if unmappedOnly is true, then the brick list holds a list of changed bricks, the unmapped ones have 0x80000000 on the brick address.
	SCOPE_CYCLE_COUNTER(STAT_FluidZeroBricks);
	FUnorderedAccessViewRHIParamRef LSFloat = nullptr;
	FUnorderedAccessViewRHIParamRef LSUint = nullptr;
	for (auto VTR : VTRs)
	{
		if (VTR->BPP != FSparseGrid::Bits_LS32)
		{
#define ZERO_BRICKS(TDT) {\
	TShaderMapRef<FZeroBricksCS<dataMode | TDT> > ZeroBricksCS(GetGlobalShaderMap(GMaxRHIFeatureLevel)); \
	RHICmdList.SetComputeShader(ZeroBricksCS->GetComputeShader());\
	ZeroBricksCS->SetOutput(RHICmdList, VTR->UAV);\
	ZeroBricksCS->SetParameters(RHICmdList, BrickListRHI);\
	\
	DispatchComputeShader(\
		RHICmdList,\
		*ZeroBricksCS,\
		NumBricks*BRICK_SIZE_X / CTA_SIZE_X,\
		BRICK_SIZE_Y / CTA_SIZE_Y,\
		BRICK_SIZE_Z / CTA_SIZE_Z);\
	ZeroBricksCS->UnbindBuffers(RHICmdList);}

			if (VTR->TDT == FSparseGrid::TDT_FLOAT) ZERO_BRICKS(FSparseGrid::TDT_FLOAT)
			else if (VTR->TDT == FSparseGrid::TDT_FLOAT2) ZERO_BRICKS(FSparseGrid::TDT_FLOAT2)
			else if (VTR->TDT == FSparseGrid::TDT_FLOAT4) ZERO_BRICKS(FSparseGrid::TDT_FLOAT4)
			else if (VTR->TDT == FSparseGrid::TDT_UINT) ZERO_BRICKS(FSparseGrid::TDT_UINT)
			else check(0 && "NEED TO ADD A TDT HERE!");
#undef ZERO_BRICKS
		}
		else
		{

			if (VTR->TDT == FSparseGrid::TDT_FLOAT) LSFloat = VTR->UAV;
			else if (VTR->TDT == FSparseGrid::TDT_UINT) LSUint = VTR->UAV;
			else check(0 && "NEED TO ADD A TDT HERE!");
		}
	}
	if (LSFloat || LSUint)
	{
		TShaderMapRef<FZeroLevelsetBricksCS<dataMode> > ZeroLevelsetBricksCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(ZeroLevelsetBricksCS->GetComputeShader());
		ZeroLevelsetBricksCS->SetOutput(RHICmdList, LSFloat, LSUint);
		ZeroLevelsetBricksCS->SetParameters(RHICmdList, BrickListRHI);
		DispatchComputeShader(
			RHICmdList,
			*ZeroLevelsetBricksCS,
			NumBricks*LS_BRICK_SIZE_X / CTA_SIZE_X,
			LS_BRICK_SIZE_Y / CTA_SIZE_Y,
			LS_BRICK_SIZE_Z / CTA_SIZE_Z);
		ZeroLevelsetBricksCS->UnbindBuffers(RHICmdList);
	}

}

void FFluidSimulation::ZeroSingleVTR(FRHICommandListImmediate& RHICmdList, FSparseGrid::TypedVTRStructured* VTR)
{
	// TODO make this work with levelsets?
	check(VTR->BPP != FSparseGrid::Bits_LS32);
//	SCOPED_DRAW_EVENT(RHICmdList, FluidZeroSingleVTR);
#define ZERO_BRICKS(TDT) {\
	TShaderMapRef<FZeroBricksCS<ZBD_mapped | TDT> > ZeroBricksCS(GetGlobalShaderMap(GMaxRHIFeatureLevel)); \
	RHICmdList.SetComputeShader(ZeroBricksCS->GetComputeShader());\
	ZeroBricksCS->SetOutput(RHICmdList, VTR->UAV);\
	ZeroBricksCS->SetParameters(RHICmdList, SparseGrid->ActiveBricksOnGPU->GetSRV());\
	\
	DispatchComputeShader(\
		RHICmdList,\
		*ZeroBricksCS,\
		SparseGrid->NumActiveBricks()*BRICK_SIZE_X / CTA_SIZE_X,\
		BRICK_SIZE_Y / CTA_SIZE_Y,\
		BRICK_SIZE_Z / CTA_SIZE_Z);\
	ZeroBricksCS->UnbindBuffers(RHICmdList);}

	if (VTR->TDT == FSparseGrid::TDT_FLOAT) ZERO_BRICKS(FSparseGrid::TDT_FLOAT)
	else if (VTR->TDT == FSparseGrid::TDT_FLOAT2) ZERO_BRICKS(FSparseGrid::TDT_FLOAT2)
	else if (VTR->TDT == FSparseGrid::TDT_FLOAT4) ZERO_BRICKS(FSparseGrid::TDT_FLOAT4)
	else if (VTR->TDT == FSparseGrid::TDT_UINT) ZERO_BRICKS(FSparseGrid::TDT_UINT)
	else check(0 && "NEED TO ADD A TDT HERE!");
#undef ZERO_BRICKS
}

void FFluidSimulation::Visualize(FPrimitiveDrawInterface* PDI)
{
	SCOPE_CYCLE_COUNTER(STAT_FluidVisualizeDomain);
	if (SparseGrid)
	{
		SparseGrid->VisualizeDomain(PDI, BrickToWorld, VoxelToWorld, LevelsetToWorld, FluidConsoleVariables::ShowBricks);
	}
}

void FFluidSimulation::UpdateSprayParameters()
{
	// Spray parameters are already updated because the copy of them we have lives on the DomainComponent.
	SprayParametersDirty = true;
}

void FFluidSimulation::UpdateSimulationParameters(float InPICSmoothing, float InSurfaceAtDensity)
{
	// TODO Move these to their own buffer?
	if (PICSmoothing != InPICSmoothing)
	{
		PICSmoothing = InPICSmoothing;
		SimulationParametersDirty = true;
	}
	if (SurfaceAtDensity != InSurfaceAtDensity)
	{
		SurfaceAtDensity = InSurfaceAtDensity;
		SimulationParametersDirty = true;
	}
}

void FFluidSimulation::UpdateSurfaceSculptingParameters(uint32 InUseDensityVelocityStretch, uint32 InUseVelocityStretch, float InRadiusMultiplier,
	float InParticleRadius, float InStretchMax, float InStretchGain, float InStretchInputScale, float InSurfaceOffset, float InSmoothKernelRadius, float InIgnoreDensityBelow)
{
	SurfaceSculptingParametersDirty = true;
	SurfaceSculptingParameters.UseDensityVelocityStretch = InUseDensityVelocityStretch;
	SurfaceSculptingParameters.UseVelocityStretch = InUseVelocityStretch;
	SurfaceSculptingParameters.RadiusMultiplier = InRadiusMultiplier;
	SurfaceSculptingParameters.ParticleRadius = InParticleRadius;
	SurfaceSculptingParameters.StretchMax = InStretchMax;
	SurfaceSculptingParameters.StretchGain = InStretchGain;
	SurfaceSculptingParameters.StretchInputScale = InStretchInputScale;
	SurfaceSculptingParameters.SurfaceOffset = InSurfaceOffset;

	SurfaceSculptingParameters.SmoothKernelRadius = InSmoothKernelRadius;
	SurfaceSculptingParameters.IgnoreDensityBelow = InIgnoreDensityBelow;
}

void FFluidSimulation::UpdateDomainTransforms(float newVoxelWidth, const FVector& translation)
{
	SimulationParametersDirty = true;
	VoxelWidth = newVoxelWidth;
	WorldToVoxel = 
		FTranslationMatrix(-translation) *
		FScaleMatrix(1.0f/VoxelWidth) * // Scale by the inverse voxel width.
		FTranslationMatrix(FVector(DOMAIN_SIZE_X * 0.5f, DOMAIN_SIZE_Y * 0.5f, 0)); // Center The Domain in x and y, and keep at bottom of z
	VoxelToWorld = WorldToVoxel.Inverse();
// 	 	UE_LOG(CataclysmInfo, Log, TEXT("WorldToVoxel = \n%f,\t%f,\t%f,\t%f\n%f,\t%f,\t%f,\t%f\n%f,\t%f,\t%f,\t%f\n%f,\t%f,\t%f,\t%f"),
// 	 		WorldToVoxel.M[0][0], WorldToVoxel.M[0][1], WorldToVoxel.M[0][2], WorldToVoxel.M[0][3],
// 	 		WorldToVoxel.M[1][0], WorldToVoxel.M[1][1], WorldToVoxel.M[1][2], WorldToVoxel.M[1][3],
// 	 		WorldToVoxel.M[2][0], WorldToVoxel.M[2][1], WorldToVoxel.M[2][2], WorldToVoxel.M[2][3],
// 	 		WorldToVoxel.M[3][0], WorldToVoxel.M[3][1], WorldToVoxel.M[3][2], WorldToVoxel.M[3][3]);
	
	WorldToBrick = WorldToVoxel * FScaleMatrix(FVector(1.0f / BRICK_SIZE_X, 1.0f / BRICK_SIZE_Y, 1.0f / BRICK_SIZE_Z)); // Scale by the brick size
	BrickToWorld = WorldToBrick.Inverse();

	WorldToLevelset = WorldToVoxel * FScaleMatrix(FVector(LS_MULTIPLIER, LS_MULTIPLIER, LS_MULTIPLIER)); // Scale by the levelset brick size
	LevelsetToWorld = WorldToLevelset.Inverse();

	Bounds = FBox(
		translation - FVector(newVoxelWidth * DOMAIN_SIZE_X * 0.5f, newVoxelWidth * DOMAIN_SIZE_Y * 0.5f, 0),
		translation + FVector(newVoxelWidth * DOMAIN_SIZE_X * 0.5f, newVoxelWidth * DOMAIN_SIZE_Y * 0.5f, newVoxelWidth * DOMAIN_SIZE_Z));
}

void FFluidSimulation::UpdateRegionsBuffer()
{
	check(IsInRenderingThread());

	FFluidFieldUniformParameters FluidFieldParameters;
	FluidFieldParameters.Count = 0;
	FFluidTriggerUniformParameters FluidTriggerParameters;
	FluidTriggerParameters.Count = 0;
	if (FluidRegions)
	{
		for (TSparseArray<FFluidRegion*>::TIterator It(*FluidRegions); It && (FluidTriggerParameters.Count + FluidFieldParameters.Count) < MAX_FLUID_REGIONS; ++It)
		{
			FFluidRegion* FluidRegion = *It;

			if(FluidRegion->Bounds.Intersect(Bounds))
			{
				if (FluidRegion->Type == EFluidRegionType::Trigger && FluidTriggerParameters.Count < MAX_FLUID_TRIGGERS)
				{
					FluidTriggerParameters.CenterPos[FluidTriggerParameters.Count] = FluidRegion->VolumeToWorld.GetOrigin();
					FluidTriggerParameters.VolumeToWorld[FluidTriggerParameters.Count] = FluidRegion->VolumeToWorld;
					FluidTriggerParameters.WorldToVolume[FluidTriggerParameters.Count] = FluidRegion->VolumeToWorld.InverseFast();
					FluidTriggerParameters.VolumeSize[FluidTriggerParameters.Count] = FluidRegion->VolumeSize;

					FluidTriggerParameters.Count++;				
				}
				else if (FluidRegion->Type == EFluidRegionType::Field && FluidFieldParameters.Count < MAX_FLUID_FIELDS)
				{
					FluidFieldParameters.VolumeToWorld[FluidFieldParameters.Count] = FluidRegion->VolumeToWorld;
					FluidFieldParameters.WorldToVolume[FluidFieldParameters.Count] = FluidRegion->VolumeToWorld.InverseFast();
					FluidFieldParameters.VolumeSize[FluidFieldParameters.Count] = FluidRegion->VolumeSize;
					FluidFieldParameters.VelocityAndWeight[FluidFieldParameters.Count] = FluidRegion->VelocityAndWeight;
					FluidFieldParameters.RegionFlags[FluidFieldParameters.Count] = FluidRegion->RegionFlags;
					FluidFieldParameters.Count++;
				}
			}
		}
	}

	FluidTriggerCount = FluidTriggerParameters.Count;
	FluidFieldCount = FluidFieldParameters.Count;

	FluidTriggerUniformBuffer = FFluidTriggerUniformBufferRef::CreateUniformBufferImmediate(FluidTriggerParameters, UniformBuffer_SingleFrame);
	FluidFieldUniformBuffer = FFluidFieldUniformBufferRef::CreateUniformBufferImmediate(FluidFieldParameters, UniformBuffer_SingleFrame);
}

FUnorderedAccessViewRHIRef FFluidSimulation::GetSprayParticleBirthDataUAV(int32 EventType) const
{
	FUnorderedAccessViewRHIRef pOut = nullptr;
	if (SprayParticleBirthDataBuffer[EventType])
	{
		pOut = SprayParticleBirthDataBuffer[EventType]->GetUAV();
	}
	return pOut;
}

FShaderResourceViewRHIRef FFluidSimulation::GetSprayParticleBirthDataSRV(int32 EventType) const
{
	FShaderResourceViewRHIRef pOut = nullptr;
	if (SprayParticleBirthDataBuffer[EventType])
	{
		pOut = SprayParticleBirthDataBuffer[EventType]->GetSRV();
	}
	return pOut;
}

void FFluidSimulation::NotifyActors(uint32* ChangedTriggerList, uint32 NumberOfTriggers)
{
	// NOTE: Can't use FluidRegions[Index] to get region info, because changed regions were get delay one frame, and the list might be changed.

	for (TSparseArray<FFluidRegion*>::TIterator TriggerIt(*FluidRegions); TriggerIt; ++TriggerIt)
	{
		FFluidRegion* FluidRegion = *TriggerIt;

		if (FluidRegion->Type != EFluidRegionType::Trigger)
		{
			continue;
		}

		for(uint32 i = 0; i < NumberOfTriggers; i++)
		{
			uint32 Index = ChangedTriggerList[i] & 0x7FFFFFFF;
			
			if (Index == FluidRegion->Index && FluidRegion->ShapeComponent)
			{
				if(ChangedTriggerList[i] & 0x80000000)
				{
					DomainComponent->AppendEndOverlap(FluidRegion->ShapeComponent);
				}
				else
				{
					DomainComponent->AppendBeginOverlap(FluidRegion->ShapeComponent);
				}

				// swap current with the last
				ChangedTriggerList[i] = ChangedTriggerList[NumberOfTriggers - 1];
				NumberOfTriggers --;
				
				// each trigger won't exist more than once in one frame
				break;
			}
		}

		if (NumberOfTriggers == 0)
		{
			break;
		}
	}
}

void FFluidSimulation::PreSpawnSpray(int32 EventType)
{
	SprayParticleCount[EventType] = SprayParticleBirthDataBuffer[EventType]->FetchCount() * SprayEventPercentage[EventType];
	SprayParticleBirthDataBuffer[EventType]->CopyCount();	
}

void FFluidSimulation::SpawnSpray(FRHICommandList& RHICmdList, 
	const FShaderResourceViewRHIRef& InVertexBufferSRV, 
	const FUnorderedAccessViewRHIRef& VertexBufferUAV, 
	const uint32 FirstParticle,
	const uint32 ParticlesThisDrawCall,
	const int32 EventType)
{
	TShaderMapRef<FCoverSprayParticleCS> CoverSprayParticleCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(CoverSprayParticleCS->GetComputeShader());
	CoverSprayParticleCS->SetOutput(RHICmdList, VertexBufferUAV);

	const uint32 GroupCount = (ParticlesThisDrawCall + 256 - 1) / 256;
		
	FSprayParticleInfoUniformParameters SprayParticleInfoParameters;		
	if (EFluidEventType::Event_Max == EventType)
	{
		SprayParticleInfoParameters.SprayParticleCount = 0;
	}
	else
	{
		SprayParticleInfoParameters.SprayParticleCount = SprayParticleCount[EventType];		
	}
	SprayParticleInfoParameters.ParticlesThisDrawCall = ParticlesThisDrawCall;
	SprayParticleInfoParameters.FirstParticle = FirstParticle;
	// Create the uniform buffer.
	FSprayParticleInfoUniformBufferRef SprayParticleInfoUniformBuffer;
	SprayParticleInfoUniformBuffer = FSprayParticleInfoUniformBufferRef::CreateUniformBufferImmediate(SprayParticleInfoParameters, UniformBuffer_SingleDraw);

	if (EFluidEventType::Event_Max == EventType)
	{
		CoverSprayParticleCS->SetParameters(RHICmdList, InVertexBufferSRV, nullptr, SprayParticleInfoUniformBuffer);
	}
	else
	{
		CoverSprayParticleCS->SetParameters(RHICmdList, InVertexBufferSRV, SprayParticleBirthDataBuffer[EventType]->GetSRV(), SprayParticleInfoUniformBuffer);
	}

	DispatchComputeShader(
		RHICmdList,
		*CoverSprayParticleCS,
		GroupCount,
		1,
		1);

	CoverSprayParticleCS->UnbindBuffers(RHICmdList);
}

namespace FluidConsoleVariables
{
	int32 X = 0;
	int32 RunSimProfiler = 0;
	int32 ExtendVelocityCount = 1;
	int32 ShowBricks = false;
	int32 Pause = false;
	float TimestepMultiplier = 1.0f;
	float MaxTimestep = 1.0f / 16.0f;
	int32 ShowLiquidSprites = false;
	int32 ShowFoamSprites = true;
	int32 DebugRenderMode = 0;
	int32 LimitSpraySpawn = false;
	int32 RunOnPause = false;
	int32 NumPressureIter = 100;
	int32 DoRepairLevelset = false;

	FAutoConsoleVariableRef CVarX(
		TEXT("Fluid.X"),
		X,
		TEXT("Developer Crap.\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarRunSimProfiler(
		TEXT("Fluid.RunSimProfiler"),
		RunSimProfiler,
		TEXT("Output profile gpu every so many frames in an easy to graph format for the fluid simulation.\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarExtendVelocityCount(
		TEXT("Fluid.ExtendVelocityCount"),
		ExtendVelocityCount,
		TEXT("Number of rounds of extension velocity after initial splatting.\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarShowBricks(
		TEXT("Fluid.ShowBricks"),
		ShowBricks,
		TEXT("Visualize the bricks of the sparse grid.\n"),
		ECVF_Cheat
		);

	// Pause Simulation Command
	FAutoConsoleVariableRef CVarPause(
		TEXT("Fluid.Pause"),
		Pause,
		TEXT("Pause The Fluid Simulation.\n"),
		ECVF_Cheat
		);

	// Slow Simulation Speed
	FAutoConsoleVariableRef CVarTimestepMultiplier(
		TEXT("Fluid.TimestepMultiplier"),
		TimestepMultiplier,
		TEXT("Simulate at half speed with TimestepMultiplier = 0.5.\n"),
		ECVF_Cheat
		);

	// Max Allowed time step, if 0 no limit.
	FAutoConsoleVariableRef CVarMaxTimestep(
		TEXT("Fluid.MaxTimestep"),
		MaxTimestep,
		TEXT("If non 0, is the maximum size of allowed timestep."),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarShowLiquidSprites(
		TEXT("Fluid.ShowLiquidSprites"),
		ShowLiquidSprites,
		TEXT("Visualize the liquid typed GPU particles.\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarShowFoamSprites(
		TEXT("Fluid.ShowFoamSprites"),
		ShowFoamSprites,
		TEXT("Visualize the Foam typed GPU particles.\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarDebugRenderMode(
		TEXT("Fluid.DebugRenderMode"),
		DebugRenderMode,
		TEXT("Debug render off (0), LiquidBoundary (1), SolidBoundary (2).\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarLimitSpraySpawn(
		TEXT("Fluid.LimitSpraySpawn"),
		LimitSpraySpawn,
		TEXT("Spawn Spray Events only up to the spawn and burst rate of the emitter.  If 0, spawn all events up to MaxGPUParticlesSpawnedPerFrame even if emitter rate is 0.\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarRunOnPause(
		TEXT("Fluid.RunOnPause"),
		RunOnPause,
		TEXT("Run the simulation steps even if dt==0\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarNumPressureIter(
		TEXT("Fluid.NumPressureIter"),
		NumPressureIter,
		TEXT("Number of iterations to use when solving Pressure Jacobi\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarDoRepairLevelset(
		TEXT("Fluid.DoRepairLevelset"),
		DoRepairLevelset,
		TEXT("After splatting, repair the levelset in case the splat ring was too small.\n"),
		ECVF_Cheat
		);
}

void FFluidSimulation::GetFluidRenderingParams(FFluidRenderingParams& RenderingParams)
{
	//RenderingParams.FluidRenderingMaterial = FluidRenderingMaterial;
	RenderingParams.BrickMapTexture = BrickMapCrossingsTexture;
	RenderingParams.LiquidSurfaceTexture = (LiquidSurfaceLS != nullptr) ? LiquidSurfaceLS->Texture3D : nullptr;
	RenderingParams.DebugMode = false;
	RenderingParams.LevelValue = 0;
	switch (FluidConsoleVariables::DebugRenderMode)
	{
	case 1:
		RenderingParams.LiquidSurfaceTexture = (LiquidBoundaryVTR != nullptr) ? LiquidBoundaryVTR->Texture3D : nullptr;
		RenderingParams.DebugMode = true;
		RenderingParams.LevelValue = 0;
		break;
	case 2:
		RenderingParams.LiquidSurfaceTexture = (SolidBoundaryVTR != nullptr) ? SolidBoundaryVTR->Texture3D : nullptr;
		RenderingParams.DebugMode = true;
		RenderingParams.LevelValue = 0;
		break;
	};

	RenderingParams.ParticleCount = SortedParticleCount;
	RenderingParams.ParticleRadius = VoxelWidth;
	RenderingParams.ParticleIndicesSRV = SortedParticlesIndicesSRV;/// Even though ParticlesToSort is updated with a new size by this time, the count and the actual sorted particle indices are not updated yet.
	RenderingParams.ParticlesPositionTexture = ParticlesPositionTexture;
	RenderingParams.ParticlesTexCoordsTexture[0] = ParticlesTexCoords[0].Texture;
	RenderingParams.ParticlesTexCoordsTexture[1] = ParticlesTexCoords[1].Texture;
	RenderingParams.ParticlesTexCoordsWeight[0] = ParticlesTexCoords[0].Weight;
	RenderingParams.ParticlesTexCoordsWeight[1] = ParticlesTexCoords[1].Weight;

	RenderingParams.WavesTexture = WavesTexture;
	RenderingParams.WavesAmplitude = WavesParameters.WavesAmplitude;
	RenderingParams.WavesLengthPeriod = WavesParameters.WavesLengthPeriod;

	RenderingParams.TexCoordRadiusMultiplier = TexCoordRadiusMultiplier;

//	RenderingParams.ParticlesDensityTexture = (SmoothDensityVTR != nullptr) ? SmoothDensityVTR->Texture3D : nullptr;

	RenderingParams.bWavesEnabled = WavesParameters.bEnabled;

	RenderingParams.bDiffuseParticlesEnabled = DiffuseParticlesParameters.bEnabled;
	if (DiffuseParticles)
	{
		RenderingParams.DiffuseParticlesPositionBufferSRV = DiffuseParticles->PositionBuffer.SRV;
		RenderingParams.DiffuseParticlesRenderAttrBufferSRV = DiffuseParticles->RenderAttrBuffer.SRV;
		RenderingParams.DiffuseParticlesDrawArgsBuffer = DiffuseParticles->DrawArgs.Buffer;
	}
}

void FFluidSimulation::UpdateWaves(float InAmplitude, float InLengthPeriod, float InWindSpeed)
{
	WavesParameters.WavesAmplitude = InAmplitude;
}
