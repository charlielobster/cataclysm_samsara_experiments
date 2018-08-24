// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// CATACLYSM

#include "Fluid/FluidRenderingMaterial.h"
#include "EnginePrivatePCH.h"


UFluidRenderingMaterial::UFluidRenderingMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaterPlanktonConcentration = 0.1f;
	WaterDistanceScale = 10.0f;
	WaterDistanceOffset = 0.0f;

	WaterDiffuseColorMod = FLinearColor::White;
	WaterAttenuationColorMod = FLinearColor::White;

	WaterReflectivity = 0.02f;

	WaterAttenuationColorMap = nullptr;
	WaterDiffuseColorMap = nullptr;

	ScreenDepthOffset = 0.0f;

	DensityFalloffStart = 0.0f;
	DensityFalloffLength = 0.1f;

	DistanceFalloffStart = 0.0f;
	DistanceFalloffLength = 0.1f;

	bWriteSurfaceDepth = true;

	BlobCullDistance = 0.0f;
	BlobCullDensity = 0.0f;

	BubbleDiffuseColor = FLinearColor(0.0f, 1.0f, 1.0f);
	BubbleAttenuationColor = FLinearColor::Black;

	DParticleMinRadius = 5.0f;
	DParticleMaxRadius = 15.0f;

	BubbleIntensityMod = FoamIntensityMod = SprayIntensityMod = 3.0f;
	BubbleIntensityExp = FoamIntensityExp = SprayIntensityExp = 1.25f;

	FoamColor = SprayColor = FLinearColor::White;
}
