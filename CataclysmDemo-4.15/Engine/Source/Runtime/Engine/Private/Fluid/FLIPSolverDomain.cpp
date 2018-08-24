// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// CATACLYSM

#include "Fluid/FLIPSolverDomain.h"
#include "Components/BillboardComponent.h"
#include "EnginePrivatePCH.h"
#include "Engine/Texture2D.h"
#include "FluidSceneProxy.h"
#include "FluidSimulation.h"
#include "Fluid/FLIPSolverDomainComponent.h"
#include "FXSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"

AFLIPSolverDomain::AFLIPSolverDomain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Component = CreateDefaultSubobject<UFLIPSolverDomainComponent>(TEXT("FLIPSolverDomainComponent0"));
	RootComponent = Component;
	bHidden = 0;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture;
			FName ID_FLIPSolverDomain;
			FText NAME_FLIPSolverDomain;

			FConstructorStatics()
				: SpriteTexture(TEXT("/Engine/EditorResources/EmptyActor"))
				, ID_FLIPSolverDomain(TEXT("FLIPSolverDomain"))
				, NAME_FLIPSolverDomain(NSLOCTEXT( "SpriteCategory", "Cataclysm", "FLIPSolverDomain" ))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;
		
		if (GetSpriteComponent())
		{
			GetSpriteComponent()->Sprite = ConstructorStatics.SpriteTexture.Get();
			GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_FLIPSolverDomain;
			GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_FLIPSolverDomain;
			GetSpriteComponent()->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
			GetSpriteComponent()->SetupAttachment(RootComponent);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

UFLIPSolverDomainComponent* AFLIPSolverDomain::GetFLIPSolverDomainComponent() const
{
	return Component;
}


UFLIPSolverDomainComponent::UFLIPSolverDomainComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnabled(true)
	, DomainSize(DOMAIN_SIZE_X, DOMAIN_SIZE_Y, DOMAIN_SIZE_Z)
	, BrickSize(BRICK_SIZE_X, BRICK_SIZE_Y, BRICK_SIZE_Z)
	, VoxelWidth(10)
	, PICSmoothing(0.05f)
	, SurfaceAtDensity(6.0f)
	, bUseDensityVelocityStretch(true)
	, bUseVelocityStretch(false)
	, RadiusMultiplier(1)
	, ParticleRadius(0.866f)
	, StretchMax(4.0f)//StretchMax(0.59f)
	, StretchGain(1)
	, StretchInputScale(0.075f)
	, SurfaceOffset(0)
	, IgnoreDensityBelow(2.0f)
	, SmoothKernelRadius(2.0f)
	, WavesAmplitude(1.0f)
	, WavesLengthPeriod(2000.0f)
	, WavesMaxWaveLength(200.0f)
	, WavesWindSpeed(10.0f)
	, TexCoordRadiusMultiplier(2.5f)
	, bEnableWaves(true)
	, FXSystem(nullptr)
{
	bIsActive = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	FluidRenderingMaterial = CreateDefaultSubobject<UFluidRenderingMaterial>(TEXT("FluidRenderingMaterial0"));

	bEnableDiffuseParticles = true;
	TrappedAirMin = 1;
	TrappedAirMax = 5;
	WaveCrestMin = 1;
	WaveCrestMax = 5;
	EnergyMin = 1;
	EnergyMax = 10;
	TrappedAirSamples = 2500;
	WaveCrestSamples = 2500;
	LifetimeMin = 2;
	LifetimeMax = 5;
	BubbleDrag = 0.3;
	BubbleBuoyancy = 0.2;
	FoamSurfaceDistThreshold = 1.0f;
	GenerateSurfaceDistThreshold = 1.0f;
	GenerateRadius = 0.866f;
	FadeinTime = 0.5f;
	FadeoutTime = 1.0f;
	SideVelocityScale = 0.1f;
}


void UFLIPSolverDomainComponent::OnRegister()
{
	Super::OnRegister();	

	if (!GRHISupportsVTR)
		return;

	UWorld* World = GetWorld();
	if (bEnabled && World && World->Scene && World->Scene->GetFXSystem())
	{
		// Store the FX system for the world in which this component is registered.
		check(FXSystem == nullptr);
		FXSystem = World->Scene->GetFXSystem();
		check(FXSystem != nullptr);

		FSurfaceSculptingParameters SurfaceSculptingParameters;

		SurfaceSculptingParameters.UseDensityVelocityStretch = bUseDensityVelocityStretch ? 1 : 0;
		SurfaceSculptingParameters.UseVelocityStretch = bUseVelocityStretch ? 1 : 0;
		SurfaceSculptingParameters.RadiusMultiplier = RadiusMultiplier;
		SurfaceSculptingParameters.ParticleRadius = ParticleRadius;
		SurfaceSculptingParameters.StretchMax = StretchMax;
		SurfaceSculptingParameters.StretchGain = StretchGain;
		SurfaceSculptingParameters.StretchInputScale = StretchInputScale;
		SurfaceSculptingParameters.SurfaceOffset = SurfaceOffset;
		SurfaceSculptingParameters.SmoothKernelRadius = SmoothKernelRadius;
		SurfaceSculptingParameters.IgnoreDensityBelow = IgnoreDensityBelow;

		FWavesParameters WavesParameters;
		WavesParameters.bEnabled = bEnableWaves;
		WavesParameters.WavesAmplitude = WavesAmplitude;
		WavesParameters.WavesLengthPeriod = WavesLengthPeriod;
		WavesParameters.WavesMaxWaveLength = WavesMaxWaveLength;
		WavesParameters.WavesWindSpeed = WavesWindSpeed;

		FFluidDiffuseParticlesParameters DiffuseParticlesParameters;
		DiffuseParticlesParameters.bEnabled = bEnableDiffuseParticles;
		DiffuseParticlesParameters.TrappedAirMin = TrappedAirMin;
		DiffuseParticlesParameters.TrappedAirMax = TrappedAirMax;
		DiffuseParticlesParameters.WaveCrestMin = WaveCrestMin;
		DiffuseParticlesParameters.WaveCrestMax = WaveCrestMax;
		DiffuseParticlesParameters.TrappedAirSamples = TrappedAirSamples;
		DiffuseParticlesParameters.WaveCrestSamples = WaveCrestSamples;
		DiffuseParticlesParameters.EnergyMin = EnergyMin;
		DiffuseParticlesParameters.EnergyMax = EnergyMax;
		DiffuseParticlesParameters.LifetimeMin = LifetimeMin;
		DiffuseParticlesParameters.LifetimeMax = LifetimeMax;
		DiffuseParticlesParameters.BubbleDrag = BubbleDrag;
		DiffuseParticlesParameters.BubbleBuoyancy = BubbleBuoyancy;
		DiffuseParticlesParameters.FoamSurfaceDistThreshold = FoamSurfaceDistThreshold;
		DiffuseParticlesParameters.GenerateSurfaceDistThreshold = GenerateSurfaceDistThreshold;
		DiffuseParticlesParameters.GenerateRadius = GenerateRadius;
		DiffuseParticlesParameters.FadeinTime = FadeinTime;
		DiffuseParticlesParameters.FadeoutTime = FadeoutTime;
		DiffuseParticlesParameters.SideVelocityScale = SideVelocityScale;

		FXSystem->RegisterFluidSimulation(new FFluidSimulation(VoxelWidth, PICSmoothing, SurfaceAtDensity, ComponentToWorld.GetTranslation(), this,
			SurfaceSculptingParameters, WavesParameters, TexCoordRadiusMultiplier, DiffuseParticlesParameters));
	}
}


void UFLIPSolverDomainComponent::OnUnregister()
{
	if (FXSystem != nullptr)
	{
		FXSystem->UnregisterFluidSimulation();
	}
	FXSystem = nullptr;
	Super::OnUnregister();
}

UParticleSystemComponent* UFLIPSolverDomainComponent::GetSprayParticleSystemComponent(int32 EffectIndex)
{
	if ((EffectIndex >= 0) && (EffectIndex < SprayParticleSystemComponents.Num()))
	{
		return SprayParticleSystemComponents[EffectIndex];
	}

	return NULL;
}

void UFLIPSolverDomainComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GRHISupportsVTR)
		return;

	for (int32 Index = 0; Index < SprayEffects.Num(); Index++)
	{
		UParticleSystemComponent* SprayParticleSystemComponent = nullptr;
		UParticleSystem* ParticleSystem = SprayEffects[Index].ParticleSystem;

		if (ParticleSystem)
		{
			SprayParticleSystemComponent = UGameplayStatics::SpawnEmitterAtLocation(this, ParticleSystem, FVector::ZeroVector/*ComponentToWorld.GetLocation()*/);
		}
		
		SprayParticleSystemComponents.Add(SprayParticleSystemComponent);
	}
}

void UFLIPSolverDomainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 Index = 0; Index < SprayParticleSystemComponents.Num(); Index++)
	{
		UParticleSystemComponent* SprayParticleSystemComponent = SprayParticleSystemComponents[Index];
		if (SprayParticleSystemComponent)
		{
			SprayParticleSystemComponent->DeactivateSystem();
			SprayParticleSystemComponent->KillParticlesForced();
			SprayParticleSystemComponent = nullptr;
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UFLIPSolverDomainComponent::PostInitProperties()
{
	Super::PostInitProperties();
	UpdateFluidSimulation();
}

#if WITH_EDITOR
void UFLIPSolverDomainComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateFluidSimulation();
}
#endif

void UFLIPSolverDomainComponent::UpdateFluidSimulation()
{
	if (FXSystem && FXSystem->GetFluidSimulation())
	{
		// Don't allow scaling or rotations.
		FVector translation = ComponentToWorld.GetTranslation();
		FXSystem->GetFluidSimulation()->UpdateSprayParameters();
		FXSystem->GetFluidSimulation()->UpdateSimulationParameters(PICSmoothing, SurfaceAtDensity);
		FXSystem->GetFluidSimulation()->UpdateSurfaceSculptingParameters(bUseDensityVelocityStretch ? 1 : 0, bUseVelocityStretch ? 1 : 0, RadiusMultiplier, ParticleRadius, StretchMax, StretchGain, StretchInputScale, SurfaceOffset, SmoothKernelRadius, IgnoreDensityBelow);
		FXSystem->GetFluidSimulation()->UpdateDomainTransforms(VoxelWidth, translation);
		FXSystem->GetFluidSimulation()->UpdateWaves(WavesAmplitude, WavesLengthPeriod, WavesWindSpeed);
	}
};

void UFLIPSolverDomainComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (SceneProxy && FXSystem && FXSystem->GetFluidSimulation())
	{
		static_cast<FFluidSceneProxy*>(SceneProxy)->SetFluidRenderingMaterial(FluidRenderingMaterial);
	}
	UpdateFluidSimulation();
	UpdateFluidOverlap();
	UpdateTimeBlending(DeltaTime);
}

FBoxSphereBounds UFLIPSolverDomainComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
//	NewBounds.Origin = FVector(0.0f);
	NewBounds.Origin = FVector(0, 0, DomainSize.Z * VoxelWidth * 0.5f);
	NewBounds.BoxExtent = FVector(DomainSize.X, DomainSize.Y, DomainSize.Z) * (VoxelWidth * 0.5f);
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();

	return NewBounds.TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* UFLIPSolverDomainComponent::CreateSceneProxy()
{
	return new FFluidSceneProxy(this);
}

void UFLIPSolverDomainComponent::AppendBeginOverlap(UPrimitiveComponent* Component)
{
	FScopeLock ScopeLock(&OverlapCriticalSection);
	// from rendering thread
	BeginOverlapList.Add(Component);
}

void UFLIPSolverDomainComponent::AppendEndOverlap(UPrimitiveComponent* Component)
{
	FScopeLock ScopeLock(&OverlapCriticalSection);
	// from rendering thread
	EndOverlapList.Add(Component);
}

extern bool IsActorValidToNotify(AActor* Actor);
void UFLIPSolverDomainComponent::UpdateFluidOverlap()
{
	FScopeLock ScopeLock(&OverlapCriticalSection);

	AActor* const MyActor = GetOwner();

	for(UPrimitiveComponent* OtherComp : BeginOverlapList)
	{
		AActor* const OtherActor = OtherComp->GetOwner();

		if (IsActorValidToNotify(MyActor))
		{
			MyActor->NotifyActorBeginOverlap(OtherActor);
			MyActor->OnActorBeginOverlap.Broadcast(MyActor, OtherActor);
		}

		if (IsActorValidToNotify(OtherActor))
		{
			OtherActor->NotifyActorBeginOverlap(MyActor);
			OtherActor->OnActorBeginOverlap.Broadcast(OtherActor, MyActor);
		}
	}
	BeginOverlapList.Reset();

	for(UPrimitiveComponent* OtherComp : EndOverlapList)
	{
		AActor* const OtherActor = OtherComp->GetOwner();

		if (IsActorValidToNotify(MyActor))
		{
			MyActor->NotifyActorEndOverlap(OtherActor);
			MyActor->OnActorEndOverlap.Broadcast(MyActor, OtherActor);
		}

		if (IsActorValidToNotify(OtherActor))
		{
			OtherActor->NotifyActorEndOverlap(MyActor);
			OtherActor->OnActorEndOverlap.Broadcast(OtherActor, MyActor);
		}
	}
	EndOverlapList.Reset();
}

void UFLIPSolverDomainComponent::AddFloatTimeBlending(const FFloatTimeBlending& FloatTimeBlending)
{
	FloatsToBlend.Add(FloatTimeBlending);
}

void UFLIPSolverDomainComponent::UpdateTimeBlending(float DeltaTime)
{
	if (FloatsToBlend.Num())
	{
		for(int32 Index = 0; Index < FloatsToBlend.Num();)
		{
			FFloatTimeBlending& FloatTimeBlending = FloatsToBlend[Index];

			if (FloatTimeBlending.PropertyToChange == nullptr)
			{
				switch(FloatTimeBlending.FloatTarget)
				{
				// Solver
				case EFluidFloatProperty::Type::Property_WavesAmplitude:
					FloatTimeBlending.PropertyToChange = &WavesAmplitude;
					break;
				case EFluidFloatProperty::Type::Property_WavesLengthPeriod:
					FloatTimeBlending.PropertyToChange = &WavesLengthPeriod;
					break;

				// Material
				case EFluidFloatProperty::Type::Property_WaterPlanktonConcentration:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterPlanktonConcentration;
					break;
				case EFluidFloatProperty::Type::Property_WaterDistanceScale:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterDistanceScale;
					break;
				case EFluidFloatProperty::Type::Property_WaterDistanceOffset:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterDistanceOffset;
					break;
				case EFluidFloatProperty::Type::Property_DensityFalloffStart:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->DensityFalloffStart;
					break;
				case EFluidFloatProperty::Type::Property_DensityFalloffLength:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->DensityFalloffLength;
					break;
				case EFluidFloatProperty::Type::Property_BlobCullDistance:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->BlobCullDistance;
					break;
				case EFluidFloatProperty::Type::Property_BlobCullDensity:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->BlobCullDensity;
					break;

				case EFluidFloatProperty::Type::Property_WaterDiffuseColorMod_R:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterDiffuseColorMod.R;
					break;
				case EFluidFloatProperty::Type::Property_WaterDiffuseColorMod_G:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterDiffuseColorMod.G;
					break;
				case EFluidFloatProperty::Type::Property_WaterDiffuseColorMod_B:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterDiffuseColorMod.B;
					break;
				case EFluidFloatProperty::Type::Property_WaterAttenuationColorMod_R:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterAttenuationColorMod.R;
					break;
				case EFluidFloatProperty::Type::Property_WaterAttenuationColorMod_G:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterAttenuationColorMod.G;
					break;
				case EFluidFloatProperty::Type::Property_WaterAttenuationColorMod_B:
					FloatTimeBlending.PropertyToChange = &FluidRenderingMaterial->WaterAttenuationColorMod.B;
					break;

				// To add new property here
				}

				FloatTimeBlending.AdditionOnTime = FloatTimeBlending.Time > 0 ? (FloatTimeBlending.To - FloatTimeBlending.From) / FloatTimeBlending.Time : 0;
			}

			if (FloatTimeBlending.TimeSinceCreation >= FloatTimeBlending.Time)
			{
				if (FloatTimeBlending.PropertyToChange)
				{
					*FloatTimeBlending.PropertyToChange = FloatTimeBlending.To;
				}

				FloatsToBlend.RemoveAtSwap(Index);
			}
			else
			{
				if (FloatTimeBlending.PropertyToChange)
				{
					*FloatTimeBlending.PropertyToChange = FloatTimeBlending.From + FloatTimeBlending.AdditionOnTime * FloatTimeBlending.TimeSinceCreation;
				}

				FloatTimeBlending.TimeSinceCreation += DeltaTime;
				++Index;
			}
		}
	}
}
