// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// CATACLYSM

#pragma once
#include "GameFramework/Info.h"
#include "FLIPSolverDomain.generated.h"

UCLASS(ClassGroup=Physics, hidecategories=(Input, Physics, Collision, LOD, Object, SceneComponent, Activation, Actor, Tags), Blueprintable)
class ENGINE_API AFLIPSolverDomain : public AInfo
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** @todo document */
	DEPRECATED_FORGAME(4.6, "FLIPSolverDomainComponent should not be accessed directly, please use GetFLIPSolverDomainComponent() function instead. FLIPSolverDomainComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = "Fluid Simulator", VisibleAnywhere, BlueprintReadOnly)
	class UFLIPSolverDomainComponent* Component;

public:
	class UFLIPSolverDomainComponent* GetFLIPSolverDomainComponent() const;
};
