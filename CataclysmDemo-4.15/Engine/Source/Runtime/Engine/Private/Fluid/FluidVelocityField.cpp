// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// CATACLYSM

#include "Fluid/FluidVelocityField.h"
#include "EnginePrivatePCH.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "FXSystem.h"
#include "FluidSimulationCommon.h"

AFluidVelocityField::AFluidVelocityField(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBoxComponent>(TEXT("CollisionComp")))
{
	UBoxComponent* BoxCollisionComponent = CastChecked<UBoxComponent>(GetCollisionComponent());

	BoxCollisionComponent->ShapeColor = FColor(240, 120, 0, 255);
	BoxCollisionComponent->InitBoxExtent(FVector(200.0f, 200.0f, 200.0f));

	static FName CollisionProfileName(TEXT("NoCollision"));
	BoxCollisionComponent->SetCollisionProfileName(CollisionProfileName);

	if (GetSpriteComponent())
	{
		GetSpriteComponent()->SetupAttachment(BoxCollisionComponent);
	}

	Weight = 0.0f;
	bKillParticlesInside = false;
	bKillParticlesOutside = false;
}

// CATACLYSM Begin
void UShapeComponent::OnRegister()
{
	Super::OnRegister();
	AActor * Owner = GetOwner();
	if(bGenerateOverlapEvents && Owner && Owner->IsA(ATriggerBase::StaticClass()))
	{
		UWorld* World = GetWorld();
		if (World && World->Scene && World->Scene->GetFXSystem())
		{
			check(FXSystem == NULL);
			FXSystem = World->Scene->GetFXSystem();
			check(FXSystem != NULL);

			// Add this component to the FX system.
			if (GetOwner()->IsA(AFluidVelocityField::StaticClass()))
			{
				AFluidVelocityField* FluidVelocityField = Cast<AFluidVelocityField>(GetOwner());
				uint32 RegionFlags = 0x00;
				if (FluidVelocityField->bKillParticlesInside) RegionFlags |= EFluidRegionFlags::KillInside;
				if (FluidVelocityField->bKillParticlesOutside) RegionFlags |= EFluidRegionFlags::KillOutside;
				FXSystem->AddFluidRegion(this, EFluidRegionType::Field, FVector4(FluidVelocityField->Velocity, FluidVelocityField->Weight), RegionFlags);
			}
			else if (FApp::IsGame())
			{
				FXSystem->AddFluidRegion(this, EFluidRegionType::Trigger, FVector4(0), 0x00);
			}
		}
	}
}

void UShapeComponent::OnUnregister()
{
	if (FluidRegion)
	{
		check(FXSystem != NULL);
		// Remove the component from the FX system.
		FXSystem->RemoveFluidRegion(this);
	}
	FXSystem = NULL;

	Super::OnUnregister();
}

void UShapeComponent::SendRenderTransform_Concurrent()
{
	Super::SendRenderTransform_Concurrent();
	if (FXSystem)
	{
		FXSystem->UpdateFluidRegion(this);
	}
}
// CATACLYSM End
