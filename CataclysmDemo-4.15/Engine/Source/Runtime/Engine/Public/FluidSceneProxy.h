// CATACLYSM
#pragma once

#include "PrimitiveSceneProxy.h"
#include "Fluid/FluidRenderingMaterial.h"

struct FFluidRenderingParams
{
	FTexture3DRHIRef BrickMapTexture;
	FTexture3DRHIRef LiquidSurfaceTexture;
	bool DebugMode;
	float LevelValue;


	uint32 ParticleCount;
	float ParticleRadius;

	FShaderResourceViewRHIParamRef ParticleIndicesSRV;
	FTexture2DRHIRef ParticlesPositionTexture;
	FTexture2DRHIRef ParticlesTexCoordsTexture[2];

	float ParticlesTexCoordsWeight[2];

	bool bWavesEnabled;
	FTexture2DRHIRef WavesTexture;
	float WavesAmplitude;
	float WavesLengthPeriod;

	float TexCoordRadiusMultiplier;

//	FTexture3DRHIRef ParticlesDensityTexture;

	bool bDiffuseParticlesEnabled;
	FShaderResourceViewRHIParamRef DiffuseParticlesPositionBufferSRV;
	FShaderResourceViewRHIParamRef DiffuseParticlesRenderAttrBufferSRV;
	FVertexBufferRHIRef DiffuseParticlesDrawArgsBuffer;

	FFluidRenderingParams()
	{
	}
};

class FFluidSceneProxy : public FPrimitiveSceneProxy
{
	static const FName& GetResourceName()
	{
		static FName sName(TEXT("$CATACLYSM_FLUID"));
		return sName;
	}
public:
	static bool IsEqualType(FPrimitiveSceneProxy* PrimitiveSceneProxy)
	{
		return PrimitiveSceneProxy->GetResourceName().IsEqual(GetResourceName(), ENameCase::CaseSensitive);
	}

	/** Initialization constructor. */
	explicit FFluidSceneProxy(UPrimitiveComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent, GetResourceName())
		, FluidRenderingMaterial(nullptr)
	{
		bCastDynamicShadow = true;
		bCastVolumetricTranslucentShadow = true;
	}

	/**
	* Computes view relevance for this scene proxy.
	*/
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucencyRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);
		return Result;
	}

	/**
	* Computes the memory footprint of this scene proxy.
	*/
	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	void SetFluidRenderingMaterial(UFluidRenderingMaterial* InFluidRenderingMaterial)
	{
		if (IsInRenderingThread())
		{
			FluidRenderingMaterial = InFluidRenderingMaterial;
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				FluidSceneProxy_SettFluidRenderingMaterial,
				FFluidSceneProxy*, Proxy, this,
				UFluidRenderingMaterial*, FluidRenderingMaterial, InFluidRenderingMaterial,
			{
					Proxy->FluidRenderingMaterial = FluidRenderingMaterial;
			});
		}
	}

	UFluidRenderingMaterial* GetFluidRenderingMaterial_RenderingThread() const
	{
		check(IsInRenderingThread());
		return FluidRenderingMaterial;
	}

protected:
	UFluidRenderingMaterial* FluidRenderingMaterial;
};
