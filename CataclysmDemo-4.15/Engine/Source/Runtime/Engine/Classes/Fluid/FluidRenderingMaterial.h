// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// CATACLYSM

#pragma once
#include "FluidRenderingMaterial.generated.h"

UCLASS(MinimalAPI)
class UFluidRenderingMaterial : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Water plankton concentration (mg/m^3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering, meta = (ClampMin = "0.01", ClampMax = "10"))
	float WaterPlanktonConcentration;

	/** Water distance scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering, meta = (ClampMin = "0.1", ClampMax = "100"))
	float WaterDistanceScale;

	/** Water distance scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering, meta = (ClampMin = "0.0"))
	float WaterDistanceOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	FLinearColor WaterDiffuseColorMod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	FLinearColor WaterAttenuationColorMod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WaterReflectivity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	class UTexture2D* WaterAttenuationColorMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	class UTexture2D* WaterDiffuseColorMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	float ScreenDepthOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	float DensityFalloffStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	float DensityFalloffLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	float DistanceFalloffStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	float DistanceFalloffLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	bool bWriteSurfaceDepth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	float BlobCullDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FluidRendering)
	float BlobCullDensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	FLinearColor BubbleDiffuseColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	FLinearColor BubbleAttenuationColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	float DParticleMinRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	float DParticleMaxRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	float BubbleIntensityMod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	float BubbleIntensityExp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	float FoamIntensityMod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	float FoamIntensityExp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	float SprayIntensityMod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	float SprayIntensityExp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	FLinearColor FoamColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DiffuseParticlesRendering)
	FLinearColor SprayColor;
};
