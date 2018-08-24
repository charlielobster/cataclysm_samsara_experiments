// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// CATACLYSM

#pragma once
#include "Engine/TriggerBase.h"
#include "FluidVelocityField.generated.h"

UCLASS(MinimalAPI)
class AFluidVelocityField : public ATriggerBase
{
	GENERATED_UCLASS_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fluid Simulator")
	FVector Velocity;

	// Weight effects how the velocity blends with existing particle velocity.  The velocity ignores weight in cells where particles do not exist.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fluid Simulator", meta=(ClampMin = "0.0", ClampMax = "100.0"))
	float Weight;

	// Kill any liquid particle that pass inside this box.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulator")
	uint32 bKillParticlesInside : 1;

	// Kill any liquid particle that go outside this box.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulator")
	uint32 bKillParticlesOutside : 1;
};
