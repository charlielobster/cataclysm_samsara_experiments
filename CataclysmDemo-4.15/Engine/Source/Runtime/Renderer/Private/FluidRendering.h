// CATACLYSM
#pragma once


#include "RHIResources.h"
#include "FluidSceneProxy.h"


struct FFluidRenderingShaderParameters;

class FFluidRendering : public FRenderResource
{
public:
	FFluidRendering()
	{
	}

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;


	void RenderFluid(FDeferredShadingSceneRenderer& SceneRenderer, FRHICommandListImmediate& RHICmdList);

private:
	struct FFluidVertex
	{
		FFluidVertex() {}
		FFluidVertex(const FVector& InPosition) :
			Position(InPosition)
		{
		}

		FVector Position;
	};

	void BuildGeometry(FFluidVertex* OutVerts, int32* OutIndices);

	void UpdateOceanTextures(const UFluidRenderingMaterial* FluidRenderingMaterial);

	void RenderFluidSurface(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, FFluidRenderingShaderParameters& ShaderParams, TRefCountPtr<IPooledRenderTarget> ZBufferTarget);

	FVertexBufferRHIRef VertexBufferRHI;
	FIndexBufferRHIRef IndexBufferRHI;
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	FGlobalBoundShaderState BoundShaderState4BasePass0;
	FGlobalBoundShaderState BoundShaderState4BasePass1;
	FGlobalBoundShaderState BoundShaderState4ShadowPass;

	float PlanktonConcentration;
	float DistanceExpCoeff;
	FTexture2DRHIRef OceanAttenuationTextureRHI;
	FTexture2DRHIRef OceanDiffuseTextureRHI;

};

/** The global fluid rendering object. */
extern TGlobalResource<FFluidRendering> GFluidRendering;
