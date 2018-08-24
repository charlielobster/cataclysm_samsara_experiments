// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// CATACLYSM

#pragma once
#include "FLIPSolverDomainComponent.generated.h"

/** Struct used to hold effects for spray events */
USTRUCT()
struct FSprayEffect
{
	GENERATED_USTRUCT_BODY()

	/** Particle system effect to play at spray location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SprayEffect)
	class UParticleSystem* ParticleSystem;

	// TODO: Add other spray info here

	FSprayEffect()
		: ParticleSystem(nullptr)
	{ }
};

/** Struct used to hold conditions for collision spray events */
USTRUCT()
struct FCollisionSprayEventConditions
{
	GENERATED_USTRUCT_BODY()
	
	// Enable Spray when particle collide with distance fields and exit the surface at a given rate from a given depth.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CollisionSprayEvent)
	uint32 bEnabled : 1;

	// Frame Delay before a particle can spawn the same event type again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CollisionSprayEvent, meta = (ClampMin = "0", ClampMax = "31"))
	int32 SpawnDelay;

	// Velocity of particle compared to surface normal.  Positive values mean the particle must be moving away from the surface (Escaping).  If you even want particles that are going back into the liquid to span spray, make this a negative number.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CollisionSprayEvent)
	float MinEscapeSpeed;

	// Particles Deeper than this will not spawn.  0 is right at surface, -1 is one voxel under the surface, and 1 is one voxel above.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CollisionSprayEvent)
	float MinDistanceToSurface;

	// Only particles with a Z normal above this value will spawn.  -1 for all particles, 0 for only up normals, 1 for none.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CollisionSprayEvent, meta = (ClampMin = "-1", ClampMax = "1"))
	float ZBias;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CollisionSprayEvent, meta = (ClampMin = "0", ClampMax = "1"))
	float SpawnPercentage;

	FCollisionSprayEventConditions()
		: bEnabled(1)
		, SpawnDelay(0)
		, MinEscapeSpeed(0)
		, MinDistanceToSurface(-2.0)
		, ZBias(-0.05)
		, SpawnPercentage(1.0f)
	{ }
};

/** Struct used to hold conditions for escape spray events */
USTRUCT()
struct FEscapeSprayEventConditions
{
	GENERATED_USTRUCT_BODY()

	/** Enable spray the first time a particle that was inside the bulk of the liquid leaves it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EscapeSprayEventConditions)
	uint32 bEnabled : 1;

	// Frame Delay before a particle can spawn the same event type again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EscapeSprayEventConditions, meta = (ClampMin = "0", ClampMax = "31"))
	int32 SpawnDelay;

	// Speed at which a particle must be traveling to spawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EscapeSprayEventConditions)
	float MinNewVelocityLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EscapeSprayEventConditions, meta = (ClampMin = "0", ClampMax = "1"))
	float SpawnPercentage;

	FEscapeSprayEventConditions()
		: bEnabled(1)
		, SpawnDelay(0)
		, MinNewVelocityLength(200)
		, SpawnPercentage(1.0f)
	{ }
};

/** Struct used to hold conditions for splash spray events */
USTRUCT()
struct FSplashSprayEventConditions
{
	GENERATED_USTRUCT_BODY()

	// Enable spray when particles are rushing toward the surface from a minimum depth with a particular change in depth.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplashSprayEventConditions)
	uint32 bEnabled : 1;

	// Frame Delay before a particle can spawn the same event type again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplashSprayEventConditions, meta = (ClampMin = "0", ClampMax = "31"))
	int32 SpawnDelay;

	// Velocity of particle compared to surface normal.  Positive values mean the particle must be moving away from the surface (Escaping).  If you even want particles that are going back into the liquid to span spray, make this a negative number.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplashSprayEventConditions)
	float MinEscapeSpeed;

	// Particles Deeper than this will not spawn.  0 is right at surface, -1 is one voxel under the surface, and 1 is one voxel above.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplashSprayEventConditions)
	float MinDistanceToSurface;

	// Particles that move less than this in a frame will not make spray. Negative values mean the particle is traveling deeper into the liquid. Slow Motion will affect this!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplashSprayEventConditions)
	float DeltaDepth;

	// Don't create spray events for particles that have collided with solids... collision spray events take care of that.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplashSprayEventConditions)
	uint32 bIgnoreCollided : 1;

	// Don't create spray events for particles that have just exited the liquid... escape spray events take care of that.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplashSprayEventConditions)
	uint32 bIgnoreEscaped : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplashSprayEventConditions, meta = (ClampMin = "0", ClampMax = "1"))
	float SpawnPercentage;

	FSplashSprayEventConditions()
		: bEnabled(1)
		, SpawnDelay(0)
		, MinEscapeSpeed(0)
		, MinDistanceToSurface(-.5)
		, DeltaDepth(0.5)
		, bIgnoreCollided(1)
		, bIgnoreEscaped(0)
		, SpawnPercentage(1.0f)
	{ }
};

UENUM()
namespace EFluidFloatProperty
{
	enum Type
	{
		// Solver
		Property_WavesAmplitude,
		Property_WavesLengthPeriod,

		// Material
		Property_WaterPlanktonConcentration,
		Property_WaterDistanceScale,
		Property_WaterDistanceOffset,
		Property_DensityFalloffStart,
		Property_DensityFalloffLength,
		Property_BlobCullDistance,
		Property_BlobCullDensity,

		Property_WaterDiffuseColorMod_R,
		Property_WaterDiffuseColorMod_G,
		Property_WaterDiffuseColorMod_B,
		Property_WaterAttenuationColorMod_R,
		Property_WaterAttenuationColorMod_G,
		Property_WaterAttenuationColorMod_B,
		// To add new float property here
	};
}

USTRUCT()
struct FFloatTimeBlending
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fluid|Utilities")
	TEnumAsByte<EFluidFloatProperty::Type>	FloatTarget;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fluid|Utilities")
	float	From;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fluid|Utilities")
	float	To;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fluid|Utilities")
	float	Time;

	float*	PropertyToChange;
	float	TimeSinceCreation;
	float	AdditionOnTime;

	FFloatTimeBlending()
		: PropertyToChange(nullptr)
		, TimeSinceCreation(0)
	{
	}
};

UCLASS(ClassGroup=Physics, config=Engine, hidecategories=(Input, Physics, Collision, LOD, Object, Activation, Actor, Tags, Lighting), editinlinenew, meta=(BlueprintSpawnableComponent),MinimalAPI)
class UFLIPSolverDomainComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fluid Simulator")
	uint32 bEnabled:1;
	
	/** The number of voxels maximum in the simulation. max 2048 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fluid Simulator")
	FIntVector DomainSize;

	/** The number of voxels in a brick in the simulation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fluid Simulator")
	FIntVector BrickSize;

	/** The width of a voxel in world space, same for all 3 dimensions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fluid Simulator")
	float VoxelWidth;

	/** PIC smoothing to calm down simulation.  1 is PIC (calm) and 0 is FLIP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulator")
	float PICSmoothing;

	/** The Liquid which the Simulation sees is different from the surface we render.  This is the particle density at which the simulation starts simulating liquid. Too low, and we have a very fat blobby sim.  Too High and there is not enough liquid to simulate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulator")
	float SurfaceAtDensity;

	/** The Sim sees a Liquid Domain that is made with a smooth kernel of this radius.  The resulting surface is also the starting surface for the rendered liquid surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	float SmoothKernelRadius;

	/** Particle Density below which to ignore particles when creating the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	float IgnoreDensityBelow;

	/** The radius of a particle when stamping it into the liquid sufrace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	float ParticleRadius;

	/** A multiplier on the radius, which is only used in liquid surface creation, not liquid domain creation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	float RadiusMultiplier;

	/** Use Velocity stretching to make ellipses of the splatting particles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	uint32 bUseDensityVelocityStretch : 1;

	/** Use Velocity stretching to make ellipses of the splatting particles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	uint32 bUseVelocityStretch : 1;

	/** The maximum amount that a particle can stretch in size by if stech is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	float StretchMax;

	/** When using stretch, this is a multiplier on the amount of stretch applied, upto the maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	float StretchGain;

	/** When using stretch, this is a multiplier on the input velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	float StretchInputScale;

	/** The render surface starts out the same as the simulation surface shrunk by this amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Surface Sculpting")
	float SurfaceOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Rendering")
	class UFluidRenderingMaterial* FluidRenderingMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Rendering")
	float TexCoordRadiusMultiplier;

	/** Spray Effect.  Use Collision Module on emitter to choose which event type to emit from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fluid Spray Creation")
	TArray<FSprayEffect> SprayEffects;

	/** Collision Spray Spawn Parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Spray Creation")
	FCollisionSprayEventConditions CollisionSprayEventConditions;

	/** Escape Spray Spawn Parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Spray Creation")
	FEscapeSprayEventConditions EscapeSprayEventCondtions;

	/** Splash Spray Spawn Parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Spray Creation")
	FSplashSprayEventConditions SplashSprayEventCondtions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Waves")
	float WavesAmplitude;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Waves")
	float WavesLengthPeriod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Waves")
	float WavesMaxWaveLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Waves")
	float WavesWindSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Rendering")
	bool  bEnableWaves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Rendering")
	bool  bEnableDiffuseParticles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float TrappedAirMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float TrappedAirMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float WaveCrestMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float WaveCrestMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float TrappedAirSamples;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float WaveCrestSamples;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float EnergyMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float EnergyMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float LifetimeMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float LifetimeMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float BubbleDrag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float BubbleBuoyancy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float FoamSurfaceDistThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float GenerateSurfaceDistThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float GenerateRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float FadeinTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float FadeoutTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Diffuse Particles")
	float SideVelocityScale;

	/**
	 *	Get spray particle system component.
	 *
	 *	@param	EffectIndex		The index of the spray effect to set it on
	 */
	UFUNCTION(BlueprintCallable, Category="Fluid|Spray")
	UParticleSystemComponent* GetSprayParticleSystemComponent(int32 EffectIndex);

	UFUNCTION(BlueprintCallable, Category="Fluid|Utilities")
	void AddFloatTimeBlending(const FFloatTimeBlending& FloatTimeBlending);


	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** The FX system. */
	class FFXSystemInterface* FXSystem;

	// We want the properties from the blueprint to init the simulation with.
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	/** The solver domain will control the grid layout of the simulation.  If a parameter changed on the grid that requires a recreation of the fluid sim, we can handle that here */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	void UpdateFluidOverlap();
	void AppendBeginOverlap(UPrimitiveComponent* Component);
	void AppendEndOverlap(UPrimitiveComponent* Component);
	void UpdateTimeBlending(float DeltaTime);

private:

	// Update Fluid Parameters based on our parameters.
	void UpdateFluidSimulation();

	mutable FCriticalSection	OverlapCriticalSection;

	TArray<UPrimitiveComponent*> BeginOverlapList;
	TArray<UPrimitiveComponent*> EndOverlapList;

	TArray<UParticleSystemComponent*> SprayParticleSystemComponents;

	TArray<FFloatTimeBlending>	FloatsToBlend;
};