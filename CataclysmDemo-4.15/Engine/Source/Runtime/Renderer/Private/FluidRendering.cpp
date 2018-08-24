// CATACLYSM
#include "FluidRendering.h"
#include "RendererPrivate.h"
#include "SceneFilterRendering.h"
#include "FluidColor/OceanSpectrum.h"
#include "FXSystem.h"


#define USE_2D_DIFFUSE_TEXTURE 0

//atatarinov_begin

#define USE_HALF_RES_FOR_WAVE_NOISE_SPRITES 1
#define SPRITE_SCALE_FACTOR 4

#define FLUID_STENCIL_VALUE 2

//atatarinov_end

/** The global fluid rendering object. */
TGlobalResource<FFluidRendering> GFluidRendering;

namespace FluidConsoleVariables
{
	int32 ShowTexCoords = false;
	int32 RenderMode = 0;

	FAutoConsoleVariableRef CVarShowTexCoords(
		TEXT("Fluid.ShowTexCoords"),
		ShowTexCoords,
		TEXT("Show texcoords on the fluid surface\n"),
		ECVF_Cheat
		);

	FAutoConsoleVariableRef CVarRenderMode(
		TEXT("Fluid.RenderMode"),
		RenderMode,
		TEXT("Fluid surface render mode (0 = closest depth trace, 1 = all depth trace, 2 = no surface render.)\n"),
		ECVF_Cheat
		);
}

/** Batched element parameters for FluidRendering shaders */
struct FFluidRenderingShaderParameters
{
	UFluidRenderingMaterial* FluidRenderingMaterial;

	FMatrix  VolumeToWorld;
	FMatrix  WorldToVolume;
	FVector  ViewOriginInVolumeCS;
	FVector4 VolumeToShadowDepth;

	float DistanceExpCoeff;
	FTexture2DRHIRef AttenuationTexture;
	FTexture2DRHIRef DiffuseTexture;

	FLinearColor SkyInscatteredColor;
	FLinearColor SkyDiffuseColor;

	FTextureRHIRef FluidDepthTexture;
	FTextureRHIRef FluidDepthDistTexture;

	FTextureRHIRef FluidTexCoordTexture;
	FTextureRHIRef FluidTexCoordWTexture;
	float ParticleRadius;

	FVector DirectionalLightDirection;
	FLinearColor DirectionalLightColor;

	float SurfaceDepthOffset;

	FTextureRHIRef SceneDepthCopyTexture;

	FTextureRHIRef FluidFoamTexture;
};


enum FluidRenderingShaderPass
{
	FR_ShaderPass0,
	FR_ShaderPass1,
	FR_ShaderPass2,
};

template <FluidRenderingShaderPass Pass>
class FFluidRenderingParticlesShader : public FGlobalShader
{
public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	/** Default constructor. */
	FFluidRenderingParticlesShader() {}

	/** Initialization constructor. */
	FFluidRenderingParticlesShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ParticleRadiusParam.Bind(Initializer.ParameterMap, TEXT("ParticleRadius"));
		SurfaceDepthOffsetParam.Bind(Initializer.ParameterMap, TEXT("SurfaceDepthOffset"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ParticleRadiusParam;
		Ar << SurfaceDepthOffsetParam;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_PASS"), uint32(Pass));

		//atatarinov_begin

#if USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
		OutEnvironment.SetDefine(TEXT("USE_HALF_RES_FOR_WAVE_NOISE_SPRITES"), 1);
#else
		OutEnvironment.SetDefine(TEXT("USE_HALF_RES_FOR_WAVE_NOISE_SPRITES"), 0);
#endif

		//atatarinov_end
	}

	template<typename ShaderRHIParamRef>
	void SetParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, ParticleRadiusParam, ShaderParams.ParticleRadius);
		if (SurfaceDepthOffsetParam.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, SurfaceDepthOffsetParam, ShaderParams.SurfaceDepthOffset);
		}
	}

private:
	FShaderParameter ParticleRadiusParam;
	FShaderParameter SurfaceDepthOffsetParam;
};

template <FluidRenderingShaderPass Pass>
class FFluidRenderingParticlesVS : public FFluidRenderingParticlesShader<Pass>
{
	DECLARE_SHADER_TYPE(FFluidRenderingParticlesVS, Global);

public:
	/** Default constructor. */
	FFluidRenderingParticlesVS() {}

	/** Initialization constructor. */
	FFluidRenderingParticlesVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FFluidRenderingParticlesShader<Pass>(Initializer)
	{
		ParticleIndicesParam.Bind(Initializer.ParameterMap, TEXT("ParticleIndices"));
		ParticlesPositionTextureParam.Bind(Initializer.ParameterMap, TEXT("ParticlesPositionTexture"));
		ParticlesTexCoord0TextureParam.Bind(Initializer.ParameterMap, TEXT("ParticlesTexCoord0Texture"));
		ParticlesTexCoord1TextureParam.Bind(Initializer.ParameterMap, TEXT("ParticlesTexCoord1Texture"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FFluidRenderingParticlesShader<Pass>::Serialize(Ar);
		Ar << ParticleIndicesParam;
		Ar << ParticlesPositionTextureParam;
		Ar << ParticlesTexCoord0TextureParam;
		Ar << ParticlesTexCoord1TextureParam;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FFluidRenderingParticlesShader<Pass>::SetParameters(RHICmdList, ShaderRHI, View, RenderingParams, ShaderParams);

		SetSRVParameter(RHICmdList, ShaderRHI, ParticleIndicesParam, RenderingParams.ParticleIndicesSRV);
		SetTextureParameter(RHICmdList, ShaderRHI, ParticlesPositionTextureParam, RenderingParams.ParticlesPositionTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, ParticlesTexCoord0TextureParam, RenderingParams.ParticlesTexCoordsTexture[0]);
		SetTextureParameter(RHICmdList, ShaderRHI, ParticlesTexCoord1TextureParam, RenderingParams.ParticlesTexCoordsTexture[1]);
	}

private:
	FShaderResourceParameter ParticleIndicesParam;
	FShaderResourceParameter ParticlesPositionTextureParam;
	FShaderResourceParameter ParticlesTexCoord0TextureParam;
	FShaderResourceParameter ParticlesTexCoord1TextureParam;
};

template <FluidRenderingShaderPass Pass>
class FFluidRenderingParticlesGS : public FFluidRenderingParticlesShader<Pass>
{
	DECLARE_SHADER_TYPE(FFluidRenderingParticlesGS, Global);

public:
	/** Default constructor. */
	FFluidRenderingParticlesGS() {}

	/** Initialization constructor. */
	FFluidRenderingParticlesGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FFluidRenderingParticlesShader<Pass>(Initializer)
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FGeometryShaderRHIParamRef ShaderRHI = GetGeometryShader();
		FFluidRenderingParticlesShader<Pass>::SetParameters(RHICmdList, ShaderRHI, View, RenderingParams, ShaderParams);
	}
};

template <FluidRenderingShaderPass Pass>
class FFluidRenderingParticlesPS : public FFluidRenderingParticlesShader<Pass>
{
	DECLARE_SHADER_TYPE(FFluidRenderingParticlesPS, Global);

public:
	/** Default constructor. */
	FFluidRenderingParticlesPS() {}

	/** Initialization constructor. */
	FFluidRenderingParticlesPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FFluidRenderingParticlesShader<Pass>(Initializer)
	{
		FluidDepthTextureParam.Bind(Initializer.ParameterMap, _TEXT("FluidDepthTexture"));
		FluidDepthTextureSamplerParam.Bind(Initializer.ParameterMap, _TEXT("FluidDepthTextureSampler"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FFluidRenderingParticlesShader<Pass>::Serialize(Ar);
		Ar << FluidDepthTextureParam;
		Ar << FluidDepthTextureSamplerParam;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FFluidRenderingParticlesShader<Pass>::SetParameters(RHICmdList, ShaderRHI, View, RenderingParams, ShaderParams);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			FluidDepthTextureParam,
			FluidDepthTextureSamplerParam,
			TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp >::GetRHI(),
			ShaderParams.FluidDepthTexture);
	}

private:
	FShaderResourceParameter FluidDepthTextureParam;
	FShaderResourceParameter FluidDepthTextureSamplerParam;
};

IMPLEMENT_SHADER_TYPE(template<>, FFluidRenderingParticlesVS<FR_ShaderPass0>, TEXT("FluidRenderingV2"), TEXT("VSMain"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, FFluidRenderingParticlesGS<FR_ShaderPass0>, TEXT("FluidRenderingV2"), TEXT("GSMain"), SF_Geometry);
IMPLEMENT_SHADER_TYPE(template<>, FFluidRenderingParticlesPS<FR_ShaderPass0>, TEXT("FluidRenderingV2"), TEXT("PSMain"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(template<>, FFluidRenderingParticlesVS<FR_ShaderPass1>, TEXT("FluidRenderingV2"), TEXT("VSMain"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, FFluidRenderingParticlesGS<FR_ShaderPass1>, TEXT("FluidRenderingV2"), TEXT("GSMain"), SF_Geometry);
IMPLEMENT_SHADER_TYPE(template<>, FFluidRenderingParticlesPS<FR_ShaderPass1>, TEXT("FluidRenderingV2"), TEXT("PSMain"), SF_Pixel);

//------------------------------------------------------------------------------

class FFluidRenderingDiffuseParticlesShader : public FGlobalShader
{
public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	/** Default constructor. */
	FFluidRenderingDiffuseParticlesShader() {}

	/** Initialization constructor. */
	FFluidRenderingDiffuseParticlesShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DParticleRadiusMinMaxParam.Bind(Initializer.ParameterMap, TEXT("DParticleRadiusMinMax"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DParticleRadiusMinMaxParam;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	template<typename ShaderRHIParamRef>
	void SetParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		if (DParticleRadiusMinMaxParam.IsBound())
		{
			float RadiusMinMax[2] = {
				ShaderParams.FluidRenderingMaterial->DParticleMinRadius,
				ShaderParams.FluidRenderingMaterial->DParticleMaxRadius
			};
			SetShaderValueArray(RHICmdList, ShaderRHI, DParticleRadiusMinMaxParam, RadiusMinMax, 2);
		}
	}

private:
	FShaderParameter DParticleRadiusMinMaxParam;
};

class FFluidRenderingDiffuseParticlesVS : public FFluidRenderingDiffuseParticlesShader
{
	DECLARE_SHADER_TYPE(FFluidRenderingDiffuseParticlesVS, Global);

public:
	/** Default constructor. */
	FFluidRenderingDiffuseParticlesVS() {}

	/** Initialization constructor. */
	FFluidRenderingDiffuseParticlesVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FFluidRenderingDiffuseParticlesShader(Initializer)
	{
		ParticlesPositionBufferParam.Bind(Initializer.ParameterMap, TEXT("ParticlesPositionBuffer"));
		ParticlesRenderAttrBufferParam.Bind(Initializer.ParameterMap, TEXT("ParticlesRenderAttrBuffer"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FFluidRenderingDiffuseParticlesShader::Serialize(Ar);
		Ar << ParticlesPositionBufferParam;
		Ar << ParticlesRenderAttrBufferParam;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FFluidRenderingDiffuseParticlesShader::SetParameters(RHICmdList, ShaderRHI, View, RenderingParams, ShaderParams);

		SetSRVParameter(RHICmdList, ShaderRHI, ParticlesPositionBufferParam, RenderingParams.DiffuseParticlesPositionBufferSRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ParticlesRenderAttrBufferParam, RenderingParams.DiffuseParticlesRenderAttrBufferSRV);
	}

private:
	FShaderResourceParameter ParticlesPositionBufferParam;
	FShaderResourceParameter ParticlesRenderAttrBufferParam;
};

class FFluidRenderingDiffuseParticlesGS : public FFluidRenderingDiffuseParticlesShader
{
	DECLARE_SHADER_TYPE(FFluidRenderingDiffuseParticlesGS, Global);

public:
	/** Default constructor. */
	FFluidRenderingDiffuseParticlesGS() {}

	/** Initialization constructor. */
	FFluidRenderingDiffuseParticlesGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FFluidRenderingDiffuseParticlesShader(Initializer)
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FGeometryShaderRHIParamRef ShaderRHI = GetGeometryShader();
		FFluidRenderingDiffuseParticlesShader::SetParameters(RHICmdList, ShaderRHI, View, RenderingParams, ShaderParams);
	}
};

class FFluidRenderingDiffuseParticlesPS : public FFluidRenderingDiffuseParticlesShader
{
	DECLARE_SHADER_TYPE(FFluidRenderingDiffuseParticlesPS, Global);

public:
	/** Default constructor. */
	FFluidRenderingDiffuseParticlesPS() {}

	/** Initialization constructor. */
	FFluidRenderingDiffuseParticlesPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FFluidRenderingDiffuseParticlesShader(Initializer)
	{
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FFluidRenderingDiffuseParticlesShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FFluidRenderingDiffuseParticlesShader::SetParameters(RHICmdList, ShaderRHI, View, RenderingParams, ShaderParams);
	}

private:
};

IMPLEMENT_SHADER_TYPE(, FFluidRenderingDiffuseParticlesVS, TEXT("FluidRenderingDP"), TEXT("VSMain"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FFluidRenderingDiffuseParticlesGS, TEXT("FluidRenderingDP"), TEXT("GSMain"), SF_Geometry);
IMPLEMENT_SHADER_TYPE(, FFluidRenderingDiffuseParticlesPS, TEXT("FluidRenderingDP"), TEXT("PSMain"), SF_Pixel);

//------------------------------------------------------------------------------

class FFluidRenderingFinalPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFluidRenderingFinalPS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	/** Default constructor. */
	FFluidRenderingFinalPS() {}

	/** Initialization constructor. */
	FFluidRenderingFinalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		TranslucentLightingParameters.Bind(Initializer.ParameterMap);
		SkyLightReflectionParameters.Bind(Initializer.ParameterMap);

		FluidDepthTextureParam.Bind(Initializer.ParameterMap, _TEXT("FluidDepthTexture"));
		FluidDepthTextureSamplerParam.Bind(Initializer.ParameterMap, _TEXT("FluidDepthTextureSampler"));
		FluidDepthDistTextureParam.Bind(Initializer.ParameterMap, _TEXT("FluidDepthDistTexture"));
		FluidDepthDistTextureSamplerParam.Bind(Initializer.ParameterMap, _TEXT("FluidDepthDistTextureSampler"));

		FluidTexCoordTextureParam.Bind(Initializer.ParameterMap, _TEXT("FluidTexCoordTexture"));
		FluidTexCoordTextureSamplerParam.Bind(Initializer.ParameterMap, _TEXT("FluidTexCoordTextureSampler"));
		FluidTexCoordWTextureParam.Bind(Initializer.ParameterMap, _TEXT("FluidTexCoordWTexture"));
		FluidTexCoordWTextureSamplerParam.Bind(Initializer.ParameterMap, _TEXT("FluidTexCoordWTextureSampler"));
		FluidTexCoordsWeightsParam.Bind(Initializer.ParameterMap, _TEXT("FluidTexCoordsWeights"));

		WavesTextureParameter.Bind(Initializer.ParameterMap, TEXT("WavesTexture"));
		WavesTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("WavesTextureSampler"));
		WavesAmplitudeParameter.Bind(Initializer.ParameterMap, TEXT("WavesAmplitude"));
		WavesLengthPeriodParameter.Bind(Initializer.ParameterMap, TEXT("WavesLengthPeriod"));

		WorldToVolumeParameter.Bind(Initializer.ParameterMap, TEXT("WorldToVolume"));
		VolumeToWorldParameter.Bind(Initializer.ParameterMap, TEXT("VolumeToWorld"));
		ViewOriginInVolumeCSParameter.Bind(Initializer.ParameterMap, TEXT("ViewOriginInVolumeCS"));
		BrickMapTextureParameter.Bind(Initializer.ParameterMap, TEXT("BrickMapTexture"));
		LiquidSurfaceTextureParameter.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTexture"));
		LiquidSurfaceTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTextureSampler"));

		DistanceExpCoeffParameter.Bind(Initializer.ParameterMap, TEXT("DistanceExpCoeff"));
		DistanceOffsetParameter.Bind(Initializer.ParameterMap, TEXT("DistanceOffset"));
		AttenuationTextureParameter.Bind(Initializer.ParameterMap, TEXT("AttenuationTexture"));
		AttenuationTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("AttenuationTextureSampler"));
		DiffuseTextureParameter.Bind(Initializer.ParameterMap, TEXT("DiffuseTexture"));
		DiffuseTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("DiffuseTextureSampler"));
		AttenuationColorModParameter.Bind(Initializer.ParameterMap, TEXT("AttenuationColorMod"));
		DiffuseColorModParameter.Bind(Initializer.ParameterMap, TEXT("DiffuseColorMod"));

		SceneColorCopyTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorCopyTexture"));
		SceneColorCopyTextureSampler.Bind(Initializer.ParameterMap, TEXT("SceneColorCopyTextureSampler"));

		SkyInscatteredColorParameter.Bind(Initializer.ParameterMap, TEXT("SkyInscatteredColor"));
		SkyDiffuseColorParameter.Bind(Initializer.ParameterMap, TEXT("SkyDiffuseColor"));

		DirectionalLightDirectionParameter.Bind(Initializer.ParameterMap, TEXT("DirectionalLightDirection"));
		DirectionalLightColorParameter.Bind(Initializer.ParameterMap, TEXT("DirectionalLightColor"));

		ReflectivityParameter.Bind(Initializer.ParameterMap, TEXT("Reflectivity"));

		ScreenDepthOffsetParam.Bind(Initializer.ParameterMap, TEXT("ScreenDepthOffset"));

		WavesEnabledParam.Bind(Initializer.ParameterMap, TEXT("WavesEnabled"));
		DiffuseParticlesEnabledParam.Bind(Initializer.ParameterMap, TEXT("DiffuseParticlesEnabled"));

// 		ParticlesDensityTextureParameter.Bind(Initializer.ParameterMap, TEXT("ParticlesDensityTexture"));
// 		ParticlesDensityTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("ParticlesDensityTextureSampler"));

		FalloffInfoParameter.Bind(Initializer.ParameterMap, TEXT("FalloffInfo"));

		SceneDepthCopyTextureParameter.Bind(Initializer.ParameterMap, TEXT("SceneDepthCopyTexture"));

		ShowTexCoordsParameter.Bind(Initializer.ParameterMap, TEXT("ShowTexCoords"));

		BlobCullInfoParameter.Bind(Initializer.ParameterMap, TEXT("BlobCullInfo"));

		FluidFoamTextureParam.Bind(Initializer.ParameterMap, TEXT("FluidFoamTexture"));
		FluidFoamTextureSamplerParam.Bind(Initializer.ParameterMap, TEXT("FluidFoamTextureSampler"));

		BubbleDiffuseColorParam.Bind(Initializer.ParameterMap, TEXT("BubbleDiffuseColor"));
		BubbleAttenuationColorParam.Bind(Initializer.ParameterMap, TEXT("BubbleAttenuationColor"));

		DParticleIntensityModParam.Bind(Initializer.ParameterMap, TEXT("DParticleIntensityMod"));
		DParticleIntensityExpParam.Bind(Initializer.ParameterMap, TEXT("DParticleIntensityExp"));

		FoamColorParam.Bind(Initializer.ParameterMap, TEXT("FoamColor"));
		SprayColorParam.Bind(Initializer.ParameterMap, TEXT("SprayColor"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << TranslucentLightingParameters;
		Ar << SkyLightReflectionParameters;
		Ar << FluidDepthTextureParam;
		Ar << FluidDepthTextureSamplerParam;
		Ar << FluidDepthDistTextureParam;
		Ar << FluidDepthDistTextureSamplerParam;
		Ar << FluidTexCoordTextureParam;
		Ar << FluidTexCoordTextureSamplerParam;
		Ar << FluidTexCoordWTextureParam;
		Ar << FluidTexCoordWTextureSamplerParam;
		Ar << FluidTexCoordsWeightsParam;
		Ar << WavesTextureParameter;
		Ar << WavesTextureSamplerParameter;
		Ar << WavesAmplitudeParameter;
		Ar << WavesLengthPeriodParameter;
		Ar << WorldToVolumeParameter;
		Ar << VolumeToWorldParameter;
		Ar << ViewOriginInVolumeCSParameter;
		Ar << BrickMapTextureParameter;
		Ar << LiquidSurfaceTextureParameter;
		Ar << LiquidSurfaceTextureSamplerParameter;
		Ar << DistanceExpCoeffParameter;
		Ar << DistanceOffsetParameter;
		Ar << AttenuationTextureParameter;
		Ar << AttenuationTextureSamplerParameter;
		Ar << DiffuseTextureParameter;
		Ar << DiffuseTextureSamplerParameter;
		Ar << AttenuationColorModParameter;
		Ar << DiffuseColorModParameter;
		Ar << SceneColorCopyTexture;
		Ar << SceneColorCopyTextureSampler;
		Ar << SkyInscatteredColorParameter;
		Ar << SkyDiffuseColorParameter;
		Ar << DirectionalLightDirectionParameter;
		Ar << DirectionalLightColorParameter;
		Ar << ReflectivityParameter;
		Ar << ScreenDepthOffsetParam;
		Ar << WavesEnabledParam;
		Ar << DiffuseParticlesEnabledParam;
// 		Ar << ParticlesDensityTextureParameter;
// 		Ar << ParticlesDensityTextureSamplerParameter;
		Ar << FalloffInfoParameter;
		Ar << SceneDepthCopyTextureParameter;
		Ar << ShowTexCoordsParameter;
		Ar << BlobCullInfoParameter;
		Ar << FluidFoamTextureParam;
		Ar << FluidFoamTextureSamplerParam;
		Ar << BubbleDiffuseColorParam;
		Ar << BubbleAttenuationColorParam;
		Ar << DParticleIntensityModParam;
		Ar << DParticleIntensityExpParam;
		Ar << FoamColorParam;
		Ar << SprayColorParam;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View);
		TranslucentLightingParameters.Set(RHICmdList, ShaderRHI, &View);
		SkyLightReflectionParameters.SetParameters(RHICmdList, ShaderRHI, (const FScene*)(View.Family->Scene), true);

		if (FluidDepthTextureParam.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				FluidDepthTextureParam,
				FluidDepthTextureSamplerParam,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp >::GetRHI(),
				ShaderParams.FluidDepthTexture ? ShaderParams.FluidDepthTexture : GBlackTexture->TextureRHI);
		}
		if (FluidDepthDistTextureParam.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				FluidDepthDistTextureParam,
				FluidDepthDistTextureSamplerParam,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp >::GetRHI(),
				ShaderParams.FluidDepthDistTexture ? ShaderParams.FluidDepthDistTexture : GBlackTexture->TextureRHI);
		}
		if (FluidTexCoordTextureParam.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				FluidTexCoordTextureParam,
				FluidTexCoordTextureSamplerParam,
				//atatarinov_begin
				TStaticSamplerState< SF_Bilinear, AM_Clamp, AM_Clamp >::GetRHI(),
				//atatarinov_end
				ShaderParams.FluidTexCoordTexture ? ShaderParams.FluidTexCoordTexture : GBlackTexture->TextureRHI);
		}
		if (FluidTexCoordWTextureParam.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				FluidTexCoordWTextureParam,
				FluidTexCoordWTextureSamplerParam,
				//atatarinov_begin
				TStaticSamplerState< SF_Bilinear, AM_Clamp, AM_Clamp >::GetRHI(),
				//atatarinov_end
				ShaderParams.FluidTexCoordWTexture ? ShaderParams.FluidTexCoordWTexture : GBlackTexture->TextureRHI);
		}
		if (FluidTexCoordsWeightsParam.IsBound())
		{
			const FVector4 TexCoordsWeight(RenderingParams.ParticlesTexCoordsWeight[0], RenderingParams.ParticlesTexCoordsWeight[1], 0, 0);
			SetShaderValue(
				RHICmdList,
				ShaderRHI,
				FluidTexCoordsWeightsParam,
				TexCoordsWeight);
		}
		if (WavesTextureParameter.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				WavesTextureParameter,
				WavesTextureSamplerParameter,
				TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap>::GetRHI(),
				RenderingParams.WavesTexture);
		}
		if (WavesAmplitudeParameter.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, WavesAmplitudeParameter, RenderingParams.WavesAmplitude);
		}
		if (WavesLengthPeriodParameter.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, WavesLengthPeriodParameter, RenderingParams.WavesLengthPeriod);
		}

		SetShaderValue(RHICmdList, ShaderRHI, WorldToVolumeParameter, ShaderParams.WorldToVolume);

		if (ViewOriginInVolumeCSParameter.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, ViewOriginInVolumeCSParameter, ShaderParams.ViewOriginInVolumeCS);
		}
		if (VolumeToWorldParameter.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, VolumeToWorldParameter, ShaderParams.VolumeToWorld);
		}
		if (BrickMapTextureParameter.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				BrickMapTextureParameter,
				RenderingParams.BrickMapTexture);
		}
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			LiquidSurfaceTextureParameter,
			LiquidSurfaceTextureSamplerParameter,
			TStaticSamplerState< SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
			RenderingParams.LiquidSurfaceTexture);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			DistanceExpCoeffParameter,
			ShaderParams.DistanceExpCoeff);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			DistanceOffsetParameter,
			ShaderParams.FluidRenderingMaterial->WaterDistanceOffset);

		FTextureRHIParamRef WaterAttenuationTexture = ShaderParams.AttenuationTexture;
		if (ShaderParams.FluidRenderingMaterial->WaterAttenuationColorMap &&
			ShaderParams.FluidRenderingMaterial->WaterAttenuationColorMap->Resource)
		{
			WaterAttenuationTexture = ShaderParams.FluidRenderingMaterial->WaterAttenuationColorMap->Resource->TextureRHI;
		}
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			AttenuationTextureParameter,
			AttenuationTextureSamplerParameter,
			TStaticSamplerState< SF_Bilinear, AM_Clamp, AM_Clamp >::GetRHI(),
			WaterAttenuationTexture);

		FTextureRHIParamRef WaterDiffuseTexture = ShaderParams.DiffuseTexture;
		if (ShaderParams.FluidRenderingMaterial->WaterDiffuseColorMap &&
			ShaderParams.FluidRenderingMaterial->WaterDiffuseColorMap->Resource)
		{
			WaterDiffuseTexture = ShaderParams.FluidRenderingMaterial->WaterDiffuseColorMap->Resource->TextureRHI;
		}
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DiffuseTextureParameter,
			DiffuseTextureSamplerParameter,
			TStaticSamplerState< SF_Bilinear, AM_Clamp, AM_Clamp >::GetRHI(),
			WaterDiffuseTexture);

		SetShaderValue(RHICmdList, ShaderRHI, AttenuationColorModParameter, ShaderParams.FluidRenderingMaterial->WaterAttenuationColorMod);
		SetShaderValue(RHICmdList, ShaderRHI, DiffuseColorModParameter, ShaderParams.FluidRenderingMaterial->WaterDiffuseColorMod);

		// for copied scene color
		if (SceneColorCopyTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				SceneColorCopyTexture,
				SceneColorCopyTextureSampler,
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
				FSceneRenderTargets::Get(RHICmdList).GetLightAttenuationTexture());
		}

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			SkyInscatteredColorParameter,
			ShaderParams.SkyInscatteredColor);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			SkyDiffuseColorParameter,
			ShaderParams.SkyDiffuseColor);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			DirectionalLightDirectionParameter,
			ShaderParams.DirectionalLightDirection);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			DirectionalLightColorParameter,
			ShaderParams.DirectionalLightColor);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			ReflectivityParameter,
			ShaderParams.FluidRenderingMaterial->WaterReflectivity);

		SetShaderValue(RHICmdList, ShaderRHI, ScreenDepthOffsetParam, ShaderParams.FluidRenderingMaterial->ScreenDepthOffset);

		if (WavesEnabledParam.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, WavesEnabledParam, uint32(RenderingParams.bWavesEnabled ? 1 : 0));
		}
		if (DiffuseParticlesEnabledParam.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, DiffuseParticlesEnabledParam, uint32(RenderingParams.bDiffuseParticlesEnabled ? 1 : 0));
		}

// 		SetTextureParameter(
// 			RHICmdList,
// 			ShaderRHI,
// 			ParticlesDensityTextureParameter,
// 			ParticlesDensityTextureSamplerParameter,
// 			TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
// 			RenderingParams.ParticlesDensityTexture);

		if (FalloffInfoParameter.IsBound())
		{
			float FalloffInfo[4] = {
				ShaderParams.FluidRenderingMaterial->DistanceFalloffStart,
				ShaderParams.FluidRenderingMaterial->DistanceFalloffStart + ShaderParams.FluidRenderingMaterial->DistanceFalloffLength,
				ShaderParams.FluidRenderingMaterial->DensityFalloffStart,
				ShaderParams.FluidRenderingMaterial->DensityFalloffStart + ShaderParams.FluidRenderingMaterial->DensityFalloffLength
			};
			SetShaderValueArray(RHICmdList, ShaderRHI, FalloffInfoParameter, FalloffInfo, 4);
		}
		if (SceneDepthCopyTextureParameter.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				SceneDepthCopyTextureParameter,
				ShaderParams.SceneDepthCopyTexture);
		}
		SetShaderValue(RHICmdList, ShaderRHI, ShowTexCoordsParameter, FluidConsoleVariables::ShowTexCoords);

		if (BlobCullInfoParameter.IsBound())
		{
			float BlobCullInfo[2] = {
				ShaderParams.FluidRenderingMaterial->BlobCullDistance,
				ShaderParams.FluidRenderingMaterial->BlobCullDensity
			};
			SetShaderValueArray(RHICmdList, ShaderRHI, BlobCullInfoParameter, BlobCullInfo, 2);
		}
		if (FluidFoamTextureParam.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				FluidFoamTextureParam,
				FluidFoamTextureSamplerParam,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp >::GetRHI(),
				ShaderParams.FluidFoamTexture != nullptr ? ShaderParams.FluidFoamTexture : GBlackTexture->TextureRHI);
		}

		if (BubbleDiffuseColorParam.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, BubbleDiffuseColorParam, ShaderParams.FluidRenderingMaterial->BubbleDiffuseColor);
		}
		if (BubbleAttenuationColorParam.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, BubbleAttenuationColorParam, ShaderParams.FluidRenderingMaterial->BubbleAttenuationColor);
		}
		if (DParticleIntensityModParam.IsBound())
		{
			float IntensityMod[3] = {
				ShaderParams.FluidRenderingMaterial->BubbleIntensityMod,
				ShaderParams.FluidRenderingMaterial->FoamIntensityMod,
				ShaderParams.FluidRenderingMaterial->SprayIntensityMod
			};
			SetShaderValueArray(RHICmdList, ShaderRHI, DParticleIntensityModParam, IntensityMod, 3);
		}
		if (DParticleIntensityExpParam.IsBound())
		{
			float IntensityExp[3] = {
				ShaderParams.FluidRenderingMaterial->BubbleIntensityExp,
				ShaderParams.FluidRenderingMaterial->FoamIntensityExp,
				ShaderParams.FluidRenderingMaterial->SprayIntensityExp
			};
			SetShaderValueArray(RHICmdList, ShaderRHI, DParticleIntensityExpParam, IntensityExp, 3);
		}
		if (FoamColorParam.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, FoamColorParam, ShaderParams.FluidRenderingMaterial->FoamColor);
		}
		if (SprayColorParam.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, SprayColorParam, ShaderParams.FluidRenderingMaterial->SprayColor);
		}
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
	FTranslucentLightingParameters TranslucentLightingParameters;

	FShaderResourceParameter FluidDepthTextureParam;
	FShaderResourceParameter FluidDepthTextureSamplerParam;
	FShaderResourceParameter FluidDepthDistTextureParam;
	FShaderResourceParameter FluidDepthDistTextureSamplerParam;

	FShaderResourceParameter FluidTexCoordTextureParam;
	FShaderResourceParameter FluidTexCoordTextureSamplerParam;
	FShaderResourceParameter FluidTexCoordWTextureParam;
	FShaderResourceParameter FluidTexCoordWTextureSamplerParam;

	FShaderParameter FluidTexCoordsWeightsParam;

	FShaderResourceParameter WavesTextureParameter;
	FShaderResourceParameter WavesTextureSamplerParameter;
	FShaderParameter WavesAmplitudeParameter;
	FShaderParameter WavesLengthPeriodParameter;

	FShaderParameter WorldToVolumeParameter;
	FShaderParameter VolumeToWorldParameter;
	FShaderParameter ViewOriginInVolumeCSParameter;

	FShaderResourceParameter BrickMapTextureParameter;
	FShaderResourceParameter LiquidSurfaceTextureParameter;
	FShaderResourceParameter LiquidSurfaceTextureSamplerParameter;

	FShaderParameter DistanceExpCoeffParameter;
	FShaderParameter DistanceOffsetParameter;
	FShaderResourceParameter AttenuationTextureParameter;
	FShaderResourceParameter AttenuationTextureSamplerParameter;
	FShaderResourceParameter DiffuseTextureParameter;
	FShaderResourceParameter DiffuseTextureSamplerParameter;
	FShaderParameter AttenuationColorModParameter;
	FShaderParameter DiffuseColorModParameter;
	FShaderResourceParameter SceneColorCopyTexture;
	FShaderResourceParameter SceneColorCopyTextureSampler;
	FShaderParameter SkyInscatteredColorParameter;
	FShaderParameter SkyDiffuseColorParameter;
	FShaderParameter DirectionalLightDirectionParameter;
	FShaderParameter DirectionalLightColorParameter;
	FShaderParameter ReflectivityParameter;

	FShaderParameter ScreenDepthOffsetParam;

	FShaderParameter WavesEnabledParam;
	FShaderParameter DiffuseParticlesEnabledParam;

// 	FShaderResourceParameter ParticlesDensityTextureParameter;
// 	FShaderResourceParameter ParticlesDensityTextureSamplerParameter;

	FShaderParameter FalloffInfoParameter;

	FShaderResourceParameter SceneDepthCopyTextureParameter;
	FShaderParameter ShowTexCoordsParameter;

	FShaderParameter BlobCullInfoParameter;

	FShaderResourceParameter FluidFoamTextureParam;
	FShaderResourceParameter FluidFoamTextureSamplerParam;

	FShaderParameter BubbleDiffuseColorParam;
	FShaderParameter BubbleAttenuationColorParam;
	FShaderParameter DParticleIntensityModParam;
	FShaderParameter DParticleIntensityExpParam;
	FShaderParameter FoamColorParam;
	FShaderParameter SprayColorParam;

	FSkyLightReflectionParameters SkyLightReflectionParameters;
};

IMPLEMENT_SHADER_TYPE(, FFluidRenderingFinalPS, TEXT("FluidRenderingV2"), TEXT("PSFinal"), SF_Pixel);

/** Fluid Rendering vertex shader */
class FFluidRenderingSurfaceVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFluidRenderingSurfaceVS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	/** Default constructor. */
	FFluidRenderingSurfaceVS() {}

	/** Initialization constructor. */
	FFluidRenderingSurfaceVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VolumeToWorldParameter.Bind(Initializer.ParameterMap, TEXT("VolumeToWorld"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumeToWorldParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, VolumeToWorldParameter, ShaderParams.VolumeToWorld);
	}

private:
	FShaderParameter VolumeToWorldParameter;
};

class FFluidRenderingSurfacePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFluidRenderingSurfacePS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	/** Default constructor. */
	FFluidRenderingSurfacePS() {}

	/** Initialization constructor. */
	FFluidRenderingSurfacePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);

		VolumeToWorldParameter.Bind(Initializer.ParameterMap, TEXT("VolumeToWorld"));
		ViewOriginInVolumeCSParameter.Bind(Initializer.ParameterMap, TEXT("ViewOriginInVolumeCS"));

		BrickMapTextureParameter.Bind(Initializer.ParameterMap, TEXT("BrickMapTexture"));
		LiquidSurfaceTextureParameter.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTexture"));
		LiquidSurfaceTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTextureSampler"));

		SurfaceDepthOffsetParameter.Bind(Initializer.ParameterMap, TEXT("SurfaceDepthOffset"));

		BlobCullInfoParameter.Bind(Initializer.ParameterMap, TEXT("BlobCullInfo"));
// 		ParticlesDensityTextureParameter.Bind(Initializer.ParameterMap, TEXT("ParticlesDensityTexture"));
// 		ParticlesDensityTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("ParticlesDensityTextureSampler"));

	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << VolumeToWorldParameter;
		Ar << ViewOriginInVolumeCSParameter;
		Ar << BrickMapTextureParameter;
		Ar << LiquidSurfaceTextureParameter;
		Ar << LiquidSurfaceTextureSamplerParameter;
		Ar << SurfaceDepthOffsetParameter;
		Ar << BlobCullInfoParameter;
// 		Ar << ParticlesDensityTextureParameter;
// 		Ar << ParticlesDensityTextureSamplerParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, VolumeToWorldParameter, ShaderParams.VolumeToWorld);
		SetShaderValue(RHICmdList, ShaderRHI, ViewOriginInVolumeCSParameter, ShaderParams.ViewOriginInVolumeCS);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BrickMapTextureParameter,
			RenderingParams.BrickMapTexture);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			LiquidSurfaceTextureParameter,
			LiquidSurfaceTextureSamplerParameter,
			TStaticSamplerState< SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
			RenderingParams.LiquidSurfaceTexture);

		SetShaderValue(RHICmdList, ShaderRHI, SurfaceDepthOffsetParameter, ShaderParams.SurfaceDepthOffset);

		if (BlobCullInfoParameter.IsBound())
		{
			float BlobCullInfo[2] = {
				ShaderParams.FluidRenderingMaterial->BlobCullDistance,
				ShaderParams.FluidRenderingMaterial->BlobCullDensity
			};
			SetShaderValueArray(RHICmdList, ShaderRHI, BlobCullInfoParameter, BlobCullInfo, 2);
		}

// 		if (ParticlesDensityTextureParameter.IsBound())
// 		{
// 			SetTextureParameter(
// 				RHICmdList,
// 				ShaderRHI,
// 				ParticlesDensityTextureParameter,
// 				ParticlesDensityTextureSamplerParameter,
// 				TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
// 				RenderingParams.ParticlesDensityTexture);
// 		}
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;

	FShaderParameter VolumeToWorldParameter;
	FShaderParameter ViewOriginInVolumeCSParameter;

	FShaderResourceParameter BrickMapTextureParameter;
	FShaderResourceParameter LiquidSurfaceTextureParameter;
	FShaderResourceParameter LiquidSurfaceTextureSamplerParameter;

	FShaderParameter SurfaceDepthOffsetParameter;

	FShaderParameter BlobCullInfoParameter;
//	FShaderResourceParameter ParticlesDensityTextureParameter;
//	FShaderResourceParameter ParticlesDensityTextureSamplerParameter;
};

IMPLEMENT_SHADER_TYPE(, FFluidRenderingSurfaceVS, TEXT("FluidRenderingV2"), TEXT("VSSurface"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FFluidRenderingSurfacePS, TEXT("FluidRenderingV2"), TEXT("PSSurface"), SF_Pixel);


/** Fluid Rendering vertex shader */
class FFluidRenderingVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFluidRenderingVS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	/** Default constructor. */
	FFluidRenderingVS() {}

	/** Initialization constructor. */
	FFluidRenderingVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VolumeToWorldParameter.Bind(Initializer.ParameterMap, TEXT("VolumeToWorld"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumeToWorldParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingShaderParameters& InShaderParams)
	{
		FGlobalShader::SetParameters(RHICmdList, GetVertexShader(), View);

		SetShaderValue(RHICmdList, GetVertexShader(), VolumeToWorldParameter, InShaderParams.VolumeToWorld);
	}

private:
	FShaderParameter VolumeToWorldParameter;
};

#if 0
/** Fluid Rendering pixel shader */
class FFluidRenderingPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFluidRenderingPS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	/** Default constructor. */
	FFluidRenderingPS() {}

	/** Initialization constructor. */
	FFluidRenderingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);

		VolumeToWorldParameter.Bind(Initializer.ParameterMap, TEXT("VolumeToWorld"));
		ViewOriginInVolumeCSParameter.Bind(Initializer.ParameterMap, TEXT("ViewOriginInVolumeCS"));

		TransmittanceParameter.Bind(Initializer.ParameterMap, TEXT("Transmittance"));
		ScatteringAlbedoParameter.Bind(Initializer.ParameterMap, TEXT("ScatteringAlbedo"));
		ScatteringColorParameter.Bind(Initializer.ParameterMap, TEXT("ScatteringColor"));

		BrickMapTextureParameter.Bind(Initializer.ParameterMap, TEXT("BrickMapTexture"));
		LiquidSurfaceTextureParameter.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTexture"));
		LiquidSurfaceTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("LiquidSurfaceTextureSampler"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << VolumeToWorldParameter;
		Ar << ViewOriginInVolumeCSParameter;
		Ar << TransmittanceParameter;
		Ar << ScatteringAlbedoParameter;
		Ar << ScatteringColorParameter;
		Ar << BrickMapTextureParameter;
		Ar << LiquidSurfaceTextureParameter;
		Ar << LiquidSurfaceTextureSamplerParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FPrimitiveSceneProxy* SceneProxy, const FFluidRenderingShaderParameters& InShaderParams, const FFluidRenderingParams& InRenderingParams)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, VolumeToWorldParameter, InShaderParams.VolumeToWorld);
		SetShaderValue(RHICmdList, ShaderRHI, ViewOriginInVolumeCSParameter, InShaderParams.ViewOriginInVolumeCS);

		SetShaderValue(RHICmdList, ShaderRHI, TransmittanceParameter, powf(InShaderParams.FluidRenderingMaterial->Transmittance, 0.01f));
		SetShaderValue(RHICmdList, ShaderRHI, ScatteringAlbedoParameter, InShaderParams.FluidRenderingMaterial->ScatteringAlbedo);
		SetShaderValue(RHICmdList, ShaderRHI, ScatteringColorParameter, FLinearColor::FromSRGBColor(InShaderParams.FluidRenderingMaterial->ScatteringColor));

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BrickMapTextureParameter,
			InRenderingParams.BrickMapTexture);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			LiquidSurfaceTextureParameter,
			LiquidSurfaceTextureSamplerParameter,
			TStaticSamplerState< SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
			InRenderingParams.LiquidSurfaceTexture);
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;

	FShaderParameter VolumeToWorldParameter;
	FShaderParameter ViewOriginInVolumeCSParameter;

	FShaderParameter TransmittanceParameter;
	FShaderParameter ScatteringAlbedoParameter;
	FShaderParameter ScatteringColorParameter;

	FShaderResourceParameter BrickMapTextureParameter;
	FShaderResourceParameter LiquidSurfaceTextureParameter;
	FShaderResourceParameter LiquidSurfaceTextureSamplerParameter;

};
#endif

class FFluidRenderingBaseVS : public FFluidRenderingVS
{
	DECLARE_SHADER_TYPE(FFluidRenderingBaseVS, Global);

public:
	FFluidRenderingBaseVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FFluidRenderingVS(Initializer)
	{
	}

	FFluidRenderingBaseVS() {}
};

IMPLEMENT_SHADER_TYPE(, FFluidRenderingBaseVS, TEXT("FluidRendering"), TEXT("VSMain"), SF_Vertex);
#if 0
class FFluidRenderingBasePS : public FFluidRenderingPS
{
	DECLARE_SHADER_TYPE(FFluidRenderingBasePS, Global);

public:
	FFluidRenderingBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FFluidRenderingPS(Initializer)
	{
		TranslucencyShadowParameters.Bind(Initializer.ParameterMap);
		VolumeToShadowMatrix.Bind(Initializer.ParameterMap, TEXT("VolumeToShadowMatrix"));
		ShadowUVMinMax.Bind(Initializer.ParameterMap, TEXT("ShadowUVMinMax"));
		DirectionalLightDirection.Bind(Initializer.ParameterMap, TEXT("DirectionalLightDirection"));
		DirectionalLightColor.Bind(Initializer.ParameterMap, TEXT("DirectionalLightColor"));

		TranslucentLightingParameters.Bind(Initializer.ParameterMap);
		IndirectLightingSHCoefficients.Bind(Initializer.ParameterMap, TEXT("IndirectLightingSHCoefficients"));
		PointSkyBentNormal.Bind(Initializer.ParameterMap, TEXT("PointSkyBentNormal"));

		DistanceExpCoeffParameter.Bind(Initializer.ParameterMap, TEXT("DistanceExpCoeff"));
		AttenuationTextureParameter.Bind(Initializer.ParameterMap, TEXT("AttenuationTexture"));
		AttenuationTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("AttenuationTextureSampler"));
		DiffuseTextureParameter.Bind(Initializer.ParameterMap, TEXT("DiffuseTexture"));
		DiffuseTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("DiffuseTextureSampler"));
		AttenuationColorModParameter.Bind(Initializer.ParameterMap, TEXT("AttenuationColorMod"));
		DiffuseColorModParameter.Bind(Initializer.ParameterMap, TEXT("DiffuseColorMod"));

		SceneColorCopyTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorCopyTexture"));
		SceneColorCopyTextureSampler.Bind(Initializer.ParameterMap, TEXT("SceneColorCopyTextureSampler"));

		SkyDiffuseColorParameter.Bind(Initializer.ParameterMap, TEXT("SkyDiffuseColor"));

		LevelValueParameter.Bind(Initializer.ParameterMap, TEXT("LevelValue"));

		ParticleCountParameter.Bind(Initializer.ParameterMap, TEXT("ParticleCount"));
		ParticleRadiusParameter.Bind(Initializer.ParameterMap, TEXT("ParticleRadius"));

		CellStartsTextureParameter.Bind(Initializer.ParameterMap, TEXT("CellStartsTexture"));
		CellEndsTextureParameter.Bind(Initializer.ParameterMap, TEXT("CellEndsTexture"));
		SortedParticleIndicesParameter.Bind(Initializer.ParameterMap, TEXT("SortedParticleIndices"));
		ParticlesPositionTextureParameter.Bind(Initializer.ParameterMap, TEXT("ParticlesPositionTexture"));
		ParticlesTexCoordsTextureParameter[0].Bind(Initializer.ParameterMap, TEXT("ParticlesTexCoordsTexture0"));
		ParticlesTexCoordsTextureParameter[1].Bind(Initializer.ParameterMap, TEXT("ParticlesTexCoordsTexture1"));

		ParticlesTexCoordsWeights.Bind(Initializer.ParameterMap, TEXT("ParticlesTexCoordsWeights"));

		WavesTextureParameter.Bind(Initializer.ParameterMap, TEXT("WavesTexture"));
		WavesTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("WavesTextureSampler"));

		ScreenDepthOffsetParam.Bind(Initializer.ParameterMap, TEXT("ScreenDepthOffset"));
	}

	FFluidRenderingBasePS() {}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FFluidRenderingPS::Serialize(Ar);
		Ar << TranslucencyShadowParameters;
		Ar << VolumeToShadowMatrix;
		Ar << ShadowUVMinMax;
		Ar << DirectionalLightDirection;
		Ar << DirectionalLightColor;
		Ar << TranslucentLightingParameters;
		Ar << IndirectLightingSHCoefficients;
		Ar << PointSkyBentNormal;
		Ar << DistanceExpCoeffParameter;
		Ar << AttenuationTextureParameter;
		Ar << AttenuationTextureSamplerParameter;
		Ar << DiffuseTextureParameter;
		Ar << DiffuseTextureSamplerParameter;
		Ar << AttenuationColorModParameter;
		Ar << DiffuseColorModParameter;
		Ar << SceneColorCopyTexture;
		Ar << SceneColorCopyTextureSampler;
		Ar << SkyDiffuseColorParameter;
		Ar << LevelValueParameter;
		Ar << ParticleCountParameter;
		Ar << ParticleRadiusParameter;
		Ar << CellStartsTextureParameter;
		Ar << CellEndsTextureParameter;
		Ar << SortedParticleIndicesParameter;
		Ar << ParticlesPositionTextureParameter;
		Ar << ParticlesTexCoordsTextureParameter[0];
		Ar << ParticlesTexCoordsTextureParameter[1];
		Ar << ParticlesTexCoordsWeights;
		Ar << WavesTextureParameter;
		Ar << WavesTextureSamplerParameter;
		Ar << ScreenDepthOffsetParam;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FPrimitiveSceneProxy* SceneProxy, const FFluidRenderingShaderParameters& InShaderParams, const FFluidRenderingParams& InRenderingParams, const FProjectedShadowInfo* TranslucentSelfShadow)
	{
		FFluidRenderingPS::SetParameters(RHICmdList, View, SceneProxy, InShaderParams, InRenderingParams);

		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		// Set these even if ElementData.TranslucentSelfShadow is NULL to avoid a d3d debug error from the shader expecting texture SRV's when a different type are bound
		TranslucencyShadowParameters.Set(RHICmdList, this);

		TranslucentLightingParameters.Set(RHICmdList, ShaderRHI, &View);
#if 0
		if (TranslucentSelfShadow)
		{
			FVector4 ShadowmapMinMax;
			FMatrix VolumeToShadowMatrixValue = InShaderParams.VolumeToWorld * TranslucentSelfShadow->GetWorldToShadowMatrix(ShadowmapMinMax);

			SetShaderValue(RHICmdList, ShaderRHI, VolumeToShadowMatrix, VolumeToShadowMatrixValue);
			SetShaderValue(RHICmdList, ShaderRHI, ShadowUVMinMax, ShadowmapMinMax);

			const FLightSceneProxy* const LightProxy = TranslucentSelfShadow->GetLightSceneInfo().Proxy;
			SetShaderValue(RHICmdList, ShaderRHI, DirectionalLightDirection, LightProxy->GetDirection());
			//@todo - support fading from both views
			const float FadeAlpha = TranslucentSelfShadow->FadeAlphas[0];
			// Incorporate the diffuse scale of 1 / PI into the light color
			const FVector4 DirectionalLightColorValue(FVector(LightProxy->GetColor() * FadeAlpha / PI), FadeAlpha);
			SetShaderValue(RHICmdList, ShaderRHI, DirectionalLightColor, DirectionalLightColorValue);
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, DirectionalLightColor, FVector4(0, 0, 0, 0));
		}
#endif
		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			DirectionalLightDirection,
			InShaderParams.DirectionalLightDirection);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			DirectionalLightColor,
			InShaderParams.DirectionalLightColor);

		if (SceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation
			&& SceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation->IsValid()
			&& View.Family->EngineShowFlags.GlobalIllumination)
		{
			const FIndirectLightingCacheAllocation& LightingAllocation = *SceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;

			SetShaderValueArray(
				RHICmdList,
				ShaderRHI,
				IndirectLightingSHCoefficients,
				&LightingAllocation.SingleSamplePacked,
				ARRAY_COUNT(LightingAllocation.SingleSamplePacked));

			SetShaderValue(
				RHICmdList,
				ShaderRHI,
				PointSkyBentNormal,
				LightingAllocation.CurrentSkyBentNormal);
		}
		else
		{
			const FVector4 ZeroArray[sizeof(FSHVectorRGB2) / sizeof(FVector4)] = { FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0) };

			SetShaderValueArray(
				RHICmdList,
				ShaderRHI,
				IndirectLightingSHCoefficients,
				&ZeroArray,
				ARRAY_COUNT(ZeroArray));

			SetShaderValue(
				RHICmdList,
				ShaderRHI,
				PointSkyBentNormal,
				FVector::ZeroVector);
		}

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			DistanceExpCoeffParameter,
			InShaderParams.DistanceExpCoeff);

		FTextureRHIParamRef WaterAttenuationTexture = InShaderParams.AttenuationTexture;
		if (InShaderParams.FluidRenderingMaterial->WaterAttenuationColorMap &&
			InShaderParams.FluidRenderingMaterial->WaterAttenuationColorMap->Resource)
		{
			WaterAttenuationTexture = InShaderParams.FluidRenderingMaterial->WaterAttenuationColorMap->Resource->TextureRHI;
		}
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			AttenuationTextureParameter,
			AttenuationTextureSamplerParameter,
			TStaticSamplerState< SF_Bilinear, AM_Clamp, AM_Clamp >::GetRHI(),
			WaterAttenuationTexture);

		FTextureRHIParamRef WaterDiffuseTexture = InShaderParams.DiffuseTexture;
		if (InShaderParams.FluidRenderingMaterial->WaterDiffuseColorMap &&
			InShaderParams.FluidRenderingMaterial->WaterDiffuseColorMap->Resource)
		{
			WaterDiffuseTexture = InShaderParams.FluidRenderingMaterial->WaterDiffuseColorMap->Resource->TextureRHI;
		}
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DiffuseTextureParameter,
			DiffuseTextureSamplerParameter,
			TStaticSamplerState< SF_Bilinear, AM_Clamp, AM_Clamp >::GetRHI(),
			WaterDiffuseTexture);

		SetShaderValue(RHICmdList, ShaderRHI, AttenuationColorModParameter, InShaderParams.FluidRenderingMaterial->WaterAttenuationColorMod);
		SetShaderValue(RHICmdList, ShaderRHI, DiffuseColorModParameter, InShaderParams.FluidRenderingMaterial->WaterDiffuseColorMod);

		// for copied scene color
		if (SceneColorCopyTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				SceneColorCopyTexture,
				SceneColorCopyTextureSampler,
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
				FSceneRenderTargets::Get(RHICmdList).GetLightAttenuationTexture());
		}

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			SkyDiffuseColorParameter,
			InShaderParams.SkyDiffuseColor);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			LevelValueParameter,
			InRenderingParams.LevelValue);


		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			ParticleCountParameter,
			InRenderingParams.ParticleCount);

		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			ParticleRadiusParameter,
			InRenderingParams.ParticleRadius);

		SetSRVParameter(
			RHICmdList,
			ShaderRHI,
			SortedParticleIndicesParameter,
			InRenderingParams.ParticleIndicesSRV);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			ParticlesPositionTextureParameter,
			InRenderingParams.ParticlesPositionTexture);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			ParticlesTexCoordsTextureParameter[0],
			InRenderingParams.ParticlesTexCoordsTexture[0]);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			ParticlesTexCoordsTextureParameter[1],
			InRenderingParams.ParticlesTexCoordsTexture[1]);

		const FVector4 TexCoordsWeight(InRenderingParams.ParticlesTexCoordsWeight[0], InRenderingParams.ParticlesTexCoordsWeight[1], 0, 0);
		SetShaderValue(
			RHICmdList,
			ShaderRHI,
			ParticlesTexCoordsWeights,
			TexCoordsWeight);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			WavesTextureParameter,
			InRenderingParams.WavesTexture);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			WavesTextureParameter,
			WavesTextureSamplerParameter,
			TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap>::GetRHI(),
			InRenderingParams.WavesTexture);
		
		SetShaderValue(RHICmdList, ShaderRHI, ScreenDepthOffsetParam, InShaderParams.FluidRenderingMaterial->ScreenDepthOffset);
	}

private:
	FTranslucencyShadowProjectionShaderParameters TranslucencyShadowParameters;
	FShaderParameter VolumeToShadowMatrix;
	FShaderParameter ShadowUVMinMax;
	FShaderParameter DirectionalLightDirection;
	FShaderParameter DirectionalLightColor;

	FTranslucentLightingParameters TranslucentLightingParameters;
	FShaderParameter IndirectLightingSHCoefficients;
	FShaderParameter PointSkyBentNormal;

	FShaderParameter DistanceExpCoeffParameter;
	FShaderResourceParameter AttenuationTextureParameter;
	FShaderResourceParameter AttenuationTextureSamplerParameter;
	FShaderResourceParameter DiffuseTextureParameter;
	FShaderResourceParameter DiffuseTextureSamplerParameter;

	FShaderParameter AttenuationColorModParameter;
	FShaderParameter DiffuseColorModParameter;

	FShaderResourceParameter SceneColorCopyTexture;
	FShaderResourceParameter SceneColorCopyTextureSampler;

	FShaderParameter SkyDiffuseColorParameter;
	FShaderParameter LevelValueParameter;

	FShaderParameter ParticleCountParameter;
	FShaderParameter ParticleRadiusParameter;

	FShaderResourceParameter CellStartsTextureParameter;
	FShaderResourceParameter CellEndsTextureParameter;
	FShaderResourceParameter SortedParticleIndicesParameter;
	FShaderResourceParameter ParticlesPositionTextureParameter;
	FShaderResourceParameter ParticlesTexCoordsTextureParameter[2];

	FShaderParameter ParticlesTexCoordsWeights;

	FShaderResourceParameter WavesTextureParameter;
	FShaderResourceParameter WavesTextureSamplerParameter;

	FShaderParameter ScreenDepthOffsetParam;
};
//IMPLEMENT_SHADER_TYPE(, FFluidRenderingBasePS, TEXT("FluidRendering"), TEXT("PSMain"), SF_Pixel);
#endif

template <bool DebugMode>
class TFluidRenderingBasePS : public FFluidRenderingFinalPS
{
	DECLARE_SHADER_TYPE(TFluidRenderingBasePS, Global);

public:
	TFluidRenderingBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FFluidRenderingFinalPS(Initializer)
	{
		LevelValueParameter.Bind(Initializer.ParameterMap, TEXT("LevelValue"));
	}

	TFluidRenderingBasePS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FFluidRenderingFinalPS::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DEBUG_MODE"), (uint32)(DebugMode ? 1 : 0));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FFluidRenderingFinalPS::Serialize(Ar);
		Ar << LevelValueParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, const FFluidRenderingShaderParameters& ShaderParams)
	{
		FFluidRenderingFinalPS::SetParameters(RHICmdList, View, RenderingParams, ShaderParams);

		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		if (LevelValueParameter.IsBound())
		{
			SetShaderValue(
				RHICmdList,
				ShaderRHI,
				LevelValueParameter,
				RenderingParams.LevelValue);
		}
	}
	
private:
	FShaderParameter LevelValueParameter;
};

IMPLEMENT_SHADER_TYPE(template<>, TFluidRenderingBasePS<false>, TEXT("FluidRendering"), TEXT("PSMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TFluidRenderingBasePS<true>, TEXT("FluidRendering"), TEXT("PSMain"), SF_Pixel);


void FFluidRendering::UpdateOceanTextures(const UFluidRenderingMaterial* FluidRenderingMaterial)
{
	PlanktonConcentration = FluidRenderingMaterial->WaterPlanktonConcentration;
	double LogDistM = OceanSpectrum::ComputeLogDistMultiplier(PlanktonConcentration);
	DistanceExpCoeff = 1.0 / LogDistM;

	uint32 Stride, SizeX;
	float* AttenuationTextureData = (float*)RHILockTexture2D(OceanAttenuationTextureRHI, 0, RLM_WriteOnly, Stride, false);

	SizeX = OceanAttenuationTextureRHI->GetSizeX();
	for (uint32 X = 0; X < SizeX; ++X)
	{
		float U = float(X) / (SizeX - 1);
		float Dist = -log(1 - U)*LogDistM;

		Tuple3d Color = OceanSpectrum::ComputeWaterAttenuationColor(Dist, PlanktonConcentration);

		AttenuationTextureData[0] = Color[0];
		AttenuationTextureData[1] = Color[1];
		AttenuationTextureData[2] = Color[2];
		AttenuationTextureData[3] = sRGB_to_Y(Color);
		AttenuationTextureData += 4;
	}

	RHIUnlockTexture2D(OceanAttenuationTextureRHI, 0, false);

	uint8* DiffuseTextureData = (uint8*)RHILockTexture2D(OceanDiffuseTextureRHI, 0, RLM_WriteOnly, Stride, false);
#if USE_2D_DIFFUSE_TEXTURE
	SizeX = OceanDiffuseTextureRHI->GetSizeX();
	uint32 SizeY = OceanDiffuseTextureRHI->GetSizeY();
	for (uint32 Y = 0; Y < SizeY; ++Y)
	{
		float cosTh = float(Y - SizeY / 2 + 1) / (SizeY / 2);
		for (uint32 X = 0; X < SizeX; ++X)
		{
			float U = float(X) / (SizeX - 1);
			float Dist = -log(1 - U)*LogDistM;

			Tuple3d Color = OceanSpectrum::ComputeWaterDiffuseColor(Dist, cosTh, PlanktonConcentration);

			float* DiffuseColor = (float*)(DiffuseTextureData + Y * Stride) + X * 4;

			DiffuseColor[0] = Color[0];
			DiffuseColor[1] = Color[1];
			DiffuseColor[2] = Color[2];
			DiffuseColor[3] = 0;
		}
	}
#else
	SizeX = OceanAttenuationTextureRHI->GetSizeX();
	for (uint32 X = 0; X < SizeX; ++X)
	{
		float U = float(X) / (SizeX - 1);
		float Dist = -log(1 - U)*LogDistM;

		Tuple3d Color = OceanSpectrum::ComputeWaterDiffuseColor(Dist, 0.0, PlanktonConcentration);

		float* DiffuseColor = (float*)(DiffuseTextureData) + X * 4;

		DiffuseColor[0] = Color[0];
		DiffuseColor[1] = Color[1];
		DiffuseColor[2] = Color[2];
		DiffuseColor[3] = 0;
	}
#endif
	RHIUnlockTexture2D(OceanDiffuseTextureRHI, 0, false);
}

void FFluidRendering::InitRHI()
{
	const int32 NumVertices = 8;
	const int32 NumIndices = 36;

	FRHIResourceCreateInfo CreateInfo;
	VertexBufferRHI = RHICreateVertexBuffer(NumVertices * sizeof(FFluidVertex), BUF_Static, CreateInfo);
	IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), NumIndices * sizeof(int32), BUF_Static, CreateInfo);

	void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, NumVertices * sizeof(FFluidVertex), RLM_WriteOnly);
	void* IndexBufferData = RHILockIndexBuffer(IndexBufferRHI, 0, NumIndices * sizeof(int32), RLM_WriteOnly);

	BuildGeometry(static_cast<FFluidVertex*>(VertexBufferData), static_cast<int32*>(IndexBufferData));

	RHIUnlockIndexBuffer(IndexBufferRHI);
	RHIUnlockVertexBuffer(VertexBufferRHI);


	FVertexDeclarationElementList Elements;
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FFluidVertex, Position), VET_Float3, 0, sizeof(FFluidVertex)));

	VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);

	OceanAttenuationTextureRHI = RHICreateTexture2D(512, 1, PF_A32B32G32R32F/*PF_FloatRGBA*/, 1, 1, 0, CreateInfo);
#if USE_2D_DIFFUSE_TEXTURE
	OceanDiffuseTextureRHI = RHICreateTexture2D(512, 128, PF_A32B32G32R32F/*PF_FloatRGBA*/, 1, 1, 0, CreateInfo);
#else
	OceanDiffuseTextureRHI = RHICreateTexture2D(512, 1, PF_A32B32G32R32F/*PF_FloatRGBA*/, 1, 1, 0, CreateInfo);
#endif

	PlanktonConcentration = 0.0f;
}

void FFluidRendering::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
	IndexBufferRHI.SafeRelease();
	VertexBufferRHI.SafeRelease();
}

void FFluidRendering::BuildGeometry(FFluidVertex* OutVerts, int32* OutIndices)
{
	const int32 StartVertex = 0;

	const FVector BoxMin(0, 0, 0);
	const FVector BoxMax(1, 1, 1);

	OutVerts[StartVertex + 0] = FFluidVertex(FVector(BoxMin.X, BoxMin.Y, BoxMin.Z));
	OutVerts[StartVertex + 1] = FFluidVertex(FVector(BoxMax.X, BoxMin.Y, BoxMin.Z));
	OutVerts[StartVertex + 2] = FFluidVertex(FVector(BoxMin.X, BoxMax.Y, BoxMin.Z));
	OutVerts[StartVertex + 3] = FFluidVertex(FVector(BoxMax.X, BoxMax.Y, BoxMin.Z));
	OutVerts[StartVertex + 4] = FFluidVertex(FVector(BoxMin.X, BoxMin.Y, BoxMax.Z));
	OutVerts[StartVertex + 5] = FFluidVertex(FVector(BoxMax.X, BoxMin.Y, BoxMax.Z));
	OutVerts[StartVertex + 6] = FFluidVertex(FVector(BoxMin.X, BoxMax.Y, BoxMax.Z));
	OutVerts[StartVertex + 7] = FFluidVertex(FVector(BoxMax.X, BoxMax.Y, BoxMax.Z));

	int32 Index = 0;
	//X-neg face
	OutIndices[Index++] = (StartVertex + 6); OutIndices[Index++] = (StartVertex + 4); OutIndices[Index++] = (StartVertex + 0);
	OutIndices[Index++] = (StartVertex + 0); OutIndices[Index++] = (StartVertex + 2); OutIndices[Index++] = (StartVertex + 6);

	//Y-neg face
	OutIndices[Index++] = (StartVertex + 5); OutIndices[Index++] = (StartVertex + 1); OutIndices[Index++] = (StartVertex + 0);
	OutIndices[Index++] = (StartVertex + 0); OutIndices[Index++] = (StartVertex + 4); OutIndices[Index++] = (StartVertex + 5);

	//Z-neg face
	OutIndices[Index++] = (StartVertex + 3); OutIndices[Index++] = (StartVertex + 2); OutIndices[Index++] = (StartVertex + 0);
	OutIndices[Index++] = (StartVertex + 0); OutIndices[Index++] = (StartVertex + 1); OutIndices[Index++] = (StartVertex + 3);

	//X-pos face
	OutIndices[Index++] = (StartVertex + 1); OutIndices[Index++] = (StartVertex + 5); OutIndices[Index++] = (StartVertex + 7);
	OutIndices[Index++] = (StartVertex + 7); OutIndices[Index++] = (StartVertex + 3); OutIndices[Index++] = (StartVertex + 1);

	//Y-pos face
	OutIndices[Index++] = (StartVertex + 2); OutIndices[Index++] = (StartVertex + 3); OutIndices[Index++] = (StartVertex + 7);
	OutIndices[Index++] = (StartVertex + 7); OutIndices[Index++] = (StartVertex + 6); OutIndices[Index++] = (StartVertex + 2);

	//Z-pos face
	OutIndices[Index++] = (StartVertex + 4); OutIndices[Index++] = (StartVertex + 6); OutIndices[Index++] = (StartVertex + 7);
	OutIndices[Index++] = (StartVertex + 7); OutIndices[Index++] = (StartVertex + 5); OutIndices[Index++] = (StartVertex + 4);
}


void FFluidRendering::RenderFluidSurface(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FFluidRenderingParams& RenderingParams, FFluidRenderingShaderParameters& ShaderParams, TRefCountPtr<IPooledRenderTarget> ZBufferTarget)
{
	const bool bWriteSurfaceDepth = ShaderParams.FluidRenderingMaterial->bWriteSurfaceDepth;

	auto FeatureLevel = View.GetFeatureLevel();
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	//copy scene depth
	ShaderParams.SceneDepthCopyTexture = ZBufferTarget->GetRenderTargetItem().ShaderResourceTexture;
	RHICmdList.CopyToResolveTarget(SceneContext.GetSceneDepthSurface(), ShaderParams.SceneDepthCopyTexture, true, FResolveParams());

	SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth,
		bWriteSurfaceDepth ? FExclusiveDepthStencil::DepthWrite_StencilWrite : FExclusiveDepthStencil::DepthRead_StencilNop);

	// viewport to match view size
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

	TShaderMapRef<FFluidRenderingBaseVS > VertexShader(GetGlobalShaderMap(FeatureLevel));
	//TShaderMapRef<FFluidRenderingBasePS > PixelShader(GetGlobalShaderMap(FeatureLevel));
	FFluidRenderingFinalPS* PixelShader;
	FGlobalBoundShaderState* BoundShaderState;
	if (RenderingParams.DebugMode)
	{
		BoundShaderState = &BoundShaderState4BasePass1;
		PixelShader = *TShaderMapRef<TFluidRenderingBasePS<true> >(GetGlobalShaderMap(FeatureLevel));
	}
	else
	{
		BoundShaderState = &BoundShaderState4BasePass0;
		PixelShader = *TShaderMapRef<TFluidRenderingBasePS<false> >(GetGlobalShaderMap(FeatureLevel));
	}

	// Bound shader state.
	SetGlobalBoundShaderState(
		RHICmdList,
		FeatureLevel,
		*BoundShaderState,
		VertexDeclarationRHI,
		*VertexShader,
		PixelShader,
		0
		);

	VertexShader->SetParameters(RHICmdList, View, ShaderParams);
	PixelShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);

	RHICmdList.SetStreamSource(
		0,
		VertexBufferRHI,
		/*Stride=*/ sizeof(FFluidVertex),
		/*Offset=*/ 0
		);

	RHICmdList.SetBlendState(TStaticBlendState<CW_RGB>::GetRHI());
	if (bWriteSurfaceDepth)
	{
		//RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), FLUID_STENCIL_VALUE);
	}
	else
	{
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI()); //DepthRead_StencilNop
	}

	RHICmdList.DrawIndexedPrimitive(
		IndexBufferRHI,
		PT_TriangleList,
		/*BaseVertexIndex=*/ 0,
		/*MinIndex=*/ 0,
		/*NumVertices=*/ 8,
		/*StartIndex=*/ 0,
		/*NumPrimitives=*/ 6 * 2,
		/*NumInstances=*/ 0
		);

	SceneContext.FinishRenderingSceneColor(RHICmdList, true);
}

void FFluidRendering::RenderFluid(FDeferredShadingSceneRenderer& SceneRenderer, FRHICommandListImmediate& RHICmdList)
{
	if (FluidConsoleVariables::RenderMode == 2) return;
	if (SceneRenderer.Scene->FXSystem == nullptr)
	{
		return;
	}
	FFluidRenderingParams RenderingParams;
	if (!SceneRenderer.Scene->FXSystem->GetFluidRenderingParams(RenderingParams))
	{
		return;
	}
	if (RenderingParams.BrickMapTexture == nullptr ||
		RenderingParams.LiquidSurfaceTexture == nullptr ||
		RenderingParams.ParticlesPositionTexture == nullptr)
	{
		return;
	}

	FPrimitiveSceneInfo* PrimitiveSceneInfo = nullptr;
	FPrimitiveSceneProxy* PrimitiveSceneProxy = nullptr;
	int32 TargetViewIndex = -1;

	for (int32 ViewIndex = 0; ViewIndex < SceneRenderer.Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = SceneRenderer.Views[ViewIndex];
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < View.VisibleDynamicPrimitives.Num(); PrimitiveIndex++)
		{
			if (FFluidSceneProxy::IsEqualType(View.VisibleDynamicPrimitives[PrimitiveIndex]->Proxy))
			{
				PrimitiveSceneInfo = const_cast<FPrimitiveSceneInfo*>(View.VisibleDynamicPrimitives[PrimitiveIndex]);
				PrimitiveSceneProxy = PrimitiveSceneInfo->Proxy;
				TargetViewIndex = ViewIndex;
				break;
			}
		}
		if (PrimitiveSceneProxy)
		{
			break;
		}
	}
	if (PrimitiveSceneProxy == nullptr)
	{
		return;
	}
	check(TargetViewIndex != -1);
	FViewInfo& View = SceneRenderer.Views[TargetViewIndex];

	//const FProjectedShadowInfo* TranslucentSelfShadow = SceneRenderer.PrepareTranslucentShadowMap(RHICmdList, View, PrimitiveSceneInfo, TPT_NonSeparateTransluceny);

	FFluidSceneProxy* FluidSceneProxy = static_cast<FFluidSceneProxy*>(PrimitiveSceneProxy);
	FFluidRenderingShaderParameters ShaderParams;
	ShaderParams.FluidRenderingMaterial = FluidSceneProxy->GetFluidRenderingMaterial_RenderingThread();
	if (ShaderParams.FluidRenderingMaterial == nullptr)
	{
		return;
	}
	if (ShaderParams.FluidRenderingMaterial->WaterPlanktonConcentration != PlanktonConcentration)
	{
		SCOPED_DRAW_EVENT(RHICmdList, UpdateOceanTextures);
		UpdateOceanTextures(ShaderParams.FluidRenderingMaterial);
	}
	{
		const FBoxSphereBounds& LocalBounds = PrimitiveSceneProxy->GetLocalBounds();

		FVector HalfVoxelSize;
		HalfVoxelSize.X = LocalBounds.BoxExtent.X / RenderingParams.LiquidSurfaceTexture->GetSizeX();
		HalfVoxelSize.Y = LocalBounds.BoxExtent.Y / RenderingParams.LiquidSurfaceTexture->GetSizeY();
		HalfVoxelSize.Z = LocalBounds.BoxExtent.Z / RenderingParams.LiquidSurfaceTexture->GetSizeZ();

		FVector Delta = LocalBounds.Origin - LocalBounds.BoxExtent + HalfVoxelSize;
		FVector Scale = LocalBounds.BoxExtent * 2.0f;

		FMatrix VolumeToLocal = FScaleMatrix(Scale) * FTranslationMatrix(Delta);
		FMatrix LocalToVolume = FTranslationMatrix(-Delta) * FScaleMatrix(Scale.Reciprocal());

		ShaderParams.VolumeToWorld = VolumeToLocal * PrimitiveSceneProxy->GetLocalToWorld();
		ShaderParams.WorldToVolume = PrimitiveSceneProxy->GetLocalToWorld().InverseFast() * LocalToVolume;
		//ShaderParams.ScreenToVolume = ScreenToWorld * ShaderParams.WorldToVolume;
		ShaderParams.ViewOriginInVolumeCS = ShaderParams.WorldToVolume.TransformPosition(View.ViewMatrices.GetViewOrigin());

		ShaderParams.DistanceExpCoeff = DistanceExpCoeff * ShaderParams.FluidRenderingMaterial->WaterDistanceScale * 0.01f; //0.01 = cm to meter conversion!
		ShaderParams.AttenuationTexture = OceanAttenuationTextureRHI;
		ShaderParams.DiffuseTexture = OceanDiffuseTextureRHI;

		const FLightSceneProxy* LightProxy = nullptr;
		for (FLightPrimitiveInteraction* Interaction = PrimitiveSceneInfo->LightList; Interaction; Interaction = Interaction->GetNextLight())
		{
			const FLightSceneInfo* LightSceneInfo = Interaction->GetLight();
			if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
			{
				LightProxy = LightSceneInfo->Proxy;
				break;
			}
		}

		if (LightProxy)
		{
			ShaderParams.DirectionalLightDirection = LightProxy->GetDirection();
			ShaderParams.DirectionalLightColor = LightProxy->GetColor() / PI;
		}
		else
		{
			ShaderParams.DirectionalLightDirection = FVector(0, 0, -1);
			ShaderParams.DirectionalLightColor = FLinearColor::Black;
		}

		ShaderParams.SkyDiffuseColor = FLinearColor(ForceInitToZero);
		if (PrimitiveSceneProxy->GetScene().GetRenderScene()->SkyLight)
		{
			const FSHVectorRGB3& SkyIrradiance = PrimitiveSceneProxy->GetScene().GetRenderScene()->SkyLight->IrradianceEnvironmentMap;
			ShaderParams.SkyDiffuseColor = Dot(SkyIrradiance, FSHVector3::CalcDiffuseTransfer(FVector(0, 0, 1))) / PI;
			ShaderParams.SkyInscatteredColor = SkyIrradiance.CalcIntegral() / (4 * PI);
		}
	}

	ShaderParams.FluidFoamTexture = nullptr;

	FTranslucencyDrawingPolicyFactory::CopySceneColor(RHICmdList, View, PrimitiveSceneProxy);

	auto FeatureLevel = View.GetFeatureLevel();
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());

	TRefCountPtr<IPooledRenderTarget> ZBufferTarget;
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_DepthStencil, FClearValueBinding::DepthFar, TexCreate_FastVRAM, TexCreate_DepthStencilTargetable, false));
		Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ZBufferTarget, _TEXT("FluidZBuffer"));
	}


	//render Depth & Dist
	TRefCountPtr<IPooledRenderTarget> DepthTarget;
	TRefCountPtr<IPooledRenderTarget> DepthDistTarget;

	bool bNeedTexCoords = RenderingParams.bWavesEnabled;
	if (bNeedTexCoords || FluidConsoleVariables::RenderMode == 0)
	{
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_R32_FLOAT, FClearValueBinding::None, TexCreate_FastVRAM, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DepthTarget, _TEXT("FluidDepth"));
		}
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_R32_FLOAT, FClearValueBinding::None, TexCreate_FastVRAM, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DepthDistTarget, _TEXT("FluidDepthDist"));
		}

		ShaderParams.SurfaceDepthOffset = RenderingParams.ParticleRadius * RenderingParams.TexCoordRadiusMultiplier;

		//Pass0 - Depth
		if (FluidConsoleVariables::RenderMode == 1)
		{
			SCOPED_DRAW_EVENT(RHICmdList, FluidDepthMode1);
			ShaderParams.ParticleRadius = RenderingParams.ParticleRadius;

			SetRenderTarget(RHICmdList, DepthTarget->GetRenderTargetItem().TargetableTexture, ZBufferTarget->GetRenderTargetItem().TargetableTexture);

			// Clear the depth & zbuffer
			//RHICmdList.Clear(true, FLinearColor(FLT_MAX, 0, 0, 0), true, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect()); CHECKME
			RHICmdList.ClearColorTexture(DepthTarget->GetRenderTargetItem().TargetableTexture, FLinearColor(FLT_MAX, 0, 0, 0), FIntRect());
			RHICmdList.ClearDepthStencilTexture(ZBufferTarget->GetRenderTargetItem().TargetableTexture, EClearDepthStencil::Depth, (float)ERHIZBuffer::FarPlane, 0, FIntRect());

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			TShaderMapRef<FFluidRenderingParticlesVS<FR_ShaderPass0> > VertexShader(GetGlobalShaderMap(FeatureLevel));
			TShaderMapRef<FFluidRenderingParticlesGS<FR_ShaderPass0> > GeometryShader(GetGlobalShaderMap(FeatureLevel));
			TShaderMapRef<FFluidRenderingParticlesPS<FR_ShaderPass0> > PixelShader(GetGlobalShaderMap(FeatureLevel));

			// Bound shader state.
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(
				RHICmdList,
				FeatureLevel,
				BoundShaderState,
				GEmptyVertexDeclaration.VertexDeclarationRHI,
				*VertexShader,
				*PixelShader,
				*GeometryShader
				);

			VertexShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
			PixelShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
			GeometryShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);

			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			//enable DepthTest & DepthWrite
			//RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), FLUID_STENCIL_VALUE);

			RHICmdList.SetStreamSource(0, NULL, 0, 0);
			RHICmdList.DrawPrimitive(PT_PointList, 0, RenderingParams.ParticleCount, 1);
		}
		else
		{
			SCOPED_DRAW_EVENT(RHICmdList, FluidDepthMode0);
			FTextureRHIParamRef RenderTargets[2] =
			{
				DepthTarget->GetRenderTargetItem().TargetableTexture,
				DepthDistTarget->GetRenderTargetItem().TargetableTexture,
			};
			SetRenderTargets(RHICmdList, 2, RenderTargets, ZBufferTarget->GetRenderTargetItem().TargetableTexture, 0, NULL);

			FLinearColor ClearColors[2] =
			{
				FLinearColor(FLT_MAX, 0, 0, 0),
				FLinearColor(FLT_MAX, 0, 0, 0)
			};
			//RHICmdList.ClearMRT(true, 2, ClearColors, true, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect()); CHECKME
			RHICmdList.ClearColorTextures(2, RenderTargets, ClearColors, FIntRect());
			RHICmdList.ClearDepthStencilTexture(ZBufferTarget->GetRenderTargetItem().TargetableTexture, EClearDepthStencil::Depth, (float)ERHIZBuffer::FarPlane, 0, FIntRect());

			// viewport to match view size
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			TShaderMapRef<FFluidRenderingSurfaceVS > VertexShader(GetGlobalShaderMap(FeatureLevel));
			TShaderMapRef<FFluidRenderingSurfacePS > PixelShader(GetGlobalShaderMap(FeatureLevel));

			// Bound shader state.
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(
				RHICmdList,
				FeatureLevel,
				BoundShaderState,
				VertexDeclarationRHI,
				*VertexShader,
				*PixelShader,
				nullptr
				);

			VertexShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
			PixelShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);

			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			//RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), FLUID_STENCIL_VALUE);

			RHICmdList.SetStreamSource(
				0,
				VertexBufferRHI,
				/*Stride=*/ sizeof(FFluidVertex),
				/*Offset=*/ 0
				);

			RHICmdList.DrawIndexedPrimitive(
				IndexBufferRHI,
				PT_TriangleList,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/ 8,
				/*StartIndex=*/ 0,
				/*NumPrimitives=*/ 6 * 2,
				/*NumInstances=*/ 0
				);

			RHICmdList.CopyToResolveTarget(DepthTarget->GetRenderTargetItem().TargetableTexture, DepthTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			RHICmdList.CopyToResolveTarget(DepthDistTarget->GetRenderTargetItem().TargetableTexture, DepthDistTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
		}
	}

	//render TexCoords
#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
	TRefCountPtr<IPooledRenderTarget> TexCoordTarget;
	TRefCountPtr<IPooledRenderTarget> TexCoordWTarget;
#else
	TRefCountPtr<IPooledRenderTarget> ZBufferTarget_HalfRes;
	TRefCountPtr<IPooledRenderTarget> DepthTarget_HalfRes;
	TRefCountPtr<IPooledRenderTarget> TexCoordTarget_HalfRes;
	TRefCountPtr<IPooledRenderTarget> TexCoordWTarget_HalfRes;
#endif
	if (bNeedTexCoords)
	{
#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_A32B32G32R32F/*PF_FloatRGBA*/, FClearValueBinding::None, TexCreate_FastVRAM, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TexCoordTarget, _TEXT("FluidTexCoord"));
		}
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_R32_FLOAT, FClearValueBinding::None, TexCreate_FastVRAM, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TexCoordWTarget, _TEXT("FluidTexCoordW"));
		}
#else
		FIntPoint BufferSizeXY_HalfRes = SceneContext.GetBufferSizeXY();
		BufferSizeXY_HalfRes.X /= SPRITE_SCALE_FACTOR;
		BufferSizeXY_HalfRes.Y /= SPRITE_SCALE_FACTOR;
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSizeXY_HalfRes, PF_DepthStencil, FClearValueBinding::DepthFar, TexCreate_FastVRAM, TexCreate_DepthStencilTargetable, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ZBufferTarget_HalfRes, _TEXT("FluidZBuffer_HalfRes"));
		}
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSizeXY_HalfRes, PF_R32_FLOAT, FClearValueBinding::None, TexCreate_FastVRAM, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DepthTarget_HalfRes, _TEXT("FluidDepth_HalfRes"));
		}
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSizeXY_HalfRes, PF_A32B32G32R32F/*PF_FloatRGBA*/, FClearValueBinding::None, TexCreate_FastVRAM, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TexCoordTarget_HalfRes, _TEXT("FluidTexCoord_HalfRes"));
		}
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSizeXY_HalfRes, PF_R32_FLOAT, FClearValueBinding::None, TexCreate_FastVRAM, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TexCoordWTarget_HalfRes, _TEXT("FluidTexCoordW_HalfRes"));
		}
#endif

		if (FluidConsoleVariables::RenderMode == 1)
		{
#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
			RHICmdList.CopyToResolveTarget(DepthTarget->GetRenderTargetItem().TargetableTexture, DepthTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
#else
			RHICmdList.CopyToResolveTarget(DepthTarget->GetRenderTargetItem().TargetableTexture, DepthTarget_HalfRes->GetRenderTargetItem().TargetableTexture, false, FResolveParams());
			RHICmdList.CopyToResolveTarget(DepthTarget_HalfRes->GetRenderTargetItem().TargetableTexture, DepthTarget_HalfRes->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			RHICmdList.CopyToResolveTarget(ZBufferTarget->GetRenderTargetItem().TargetableTexture, ZBufferTarget_HalfRes->GetRenderTargetItem().TargetableTexture, false, FResolveParams());
#endif
		}
		else
		{
#if USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
			RHICmdList.CopyToResolveTarget(DepthTarget->GetRenderTargetItem().TargetableTexture, DepthTarget_HalfRes->GetRenderTargetItem().TargetableTexture, false, FResolveParams());
			RHICmdList.CopyToResolveTarget(DepthTarget_HalfRes->GetRenderTargetItem().TargetableTexture, DepthTarget_HalfRes->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			RHICmdList.CopyToResolveTarget(ZBufferTarget->GetRenderTargetItem().TargetableTexture, ZBufferTarget_HalfRes->GetRenderTargetItem().TargetableTexture, false, FResolveParams());
#endif
		}
		// Moving this pass to half-resolution. It will require half-res depth and a separate upsampling blit pass.

#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
		ShaderParams.FluidDepthTexture = DepthTarget->GetRenderTargetItem().ShaderResourceTexture;
#else
		ShaderParams.FluidDepthTexture = DepthTarget_HalfRes->GetRenderTargetItem().ShaderResourceTexture;
#endif

		ShaderParams.ParticleRadius = RenderingParams.ParticleRadius * RenderingParams.TexCoordRadiusMultiplier;
		//Pass1 - TexCoords
		{

			SCOPED_DRAW_EVENT(RHICmdList, FluidTexCoords);
			//atatarinov_begin

			FTextureRHIParamRef RenderTargets[2] =
			{
	#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
				TexCoordTarget->GetRenderTargetItem().TargetableTexture,
				TexCoordWTarget->GetRenderTargetItem().TargetableTexture,
	#else			
				TexCoordTarget_HalfRes->GetRenderTargetItem().TargetableTexture,
				TexCoordWTarget_HalfRes->GetRenderTargetItem().TargetableTexture,
	#endif
			};

#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
			SetRenderTargets(RHICmdList, 2, RenderTargets, ZBufferTarget->GetRenderTargetItem().TargetableTexture, 0, NULL);
#else
			SetRenderTargets(RHICmdList, 2, RenderTargets, ZBufferTarget_HalfRes->GetRenderTargetItem().TargetableTexture, 0, NULL);
#endif

			//atatarinov_end

			FLinearColor ClearColors[2] =
			{
				FLinearColor(0, 0, 0, 0),
				FLinearColor(0, 0, 0, 0)
			};
			//RHICmdList.ClearMRT(true, 2, ClearColors, false, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect()); CHECKME
			RHICmdList.ClearColorTextures(2, RenderTargets, ClearColors, FIntRect());
#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
			RHICmdList.ClearDepthStencilTexture(ZBufferTarget->GetRenderTargetItem().TargetableTexture, EClearDepthStencil::Depth, (float)ERHIZBuffer::FarPlane, 0, FIntRect());
#else
			RHICmdList.ClearDepthStencilTexture(ZBufferTarget_HalfRes->GetRenderTargetItem().TargetableTexture, EClearDepthStencil::Depth, (float)ERHIZBuffer::FarPlane, 0, FIntRect());
#endif

			// viewport to match view size
			//atatarinov_begin

#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
#else
			RHICmdList.SetViewport(View.ViewRect.Min.X / SPRITE_SCALE_FACTOR, View.ViewRect.Min.Y / SPRITE_SCALE_FACTOR, 0.0f, View.ViewRect.Max.X / SPRITE_SCALE_FACTOR, View.ViewRect.Max.Y / SPRITE_SCALE_FACTOR, 1.0f);
#endif

			//atatarinov_end

			TShaderMapRef<FFluidRenderingParticlesVS<FR_ShaderPass1> > VertexShader(GetGlobalShaderMap(FeatureLevel));
			TShaderMapRef<FFluidRenderingParticlesGS<FR_ShaderPass1> > GeometryShader(GetGlobalShaderMap(FeatureLevel));
			TShaderMapRef<FFluidRenderingParticlesPS<FR_ShaderPass1> > PixelShader(GetGlobalShaderMap(FeatureLevel));

			// Bound shader state.
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(
				RHICmdList,
				FeatureLevel,
				BoundShaderState,
				GEmptyVertexDeclaration.VertexDeclarationRHI,
				*VertexShader,
				*PixelShader,
				*GeometryShader
				);

			VertexShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
			PixelShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
			GeometryShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);

			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One, CW_RED, BO_Add, BF_One, BF_One>::GetRHI());
			//RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), FLUID_STENCIL_VALUE);

			RHICmdList.SetStreamSource(0, NULL, 0, 0);
			RHICmdList.DrawPrimitive(PT_PointList, 0, RenderingParams.ParticleCount, 1);

			//atatarinov_begin

#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
			RHICmdList.CopyToResolveTarget(TexCoordTarget->GetRenderTargetItem().TargetableTexture, TexCoordTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			RHICmdList.CopyToResolveTarget(TexCoordWTarget->GetRenderTargetItem().TargetableTexture, TexCoordWTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
#else
			RHICmdList.CopyToResolveTarget(TexCoordTarget_HalfRes->GetRenderTargetItem().TargetableTexture, TexCoordTarget_HalfRes->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			RHICmdList.CopyToResolveTarget(TexCoordWTarget_HalfRes->GetRenderTargetItem().TargetableTexture, TexCoordWTarget_HalfRes->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
#endif

			//atatarinov_end
		}

		//atatarinov_begin

#if !USE_HALF_RES_FOR_WAVE_NOISE_SPRITES
		ShaderParams.FluidTexCoordTexture = TexCoordTarget->GetRenderTargetItem().ShaderResourceTexture;
		ShaderParams.FluidTexCoordWTexture = TexCoordWTarget->GetRenderTargetItem().ShaderResourceTexture;
#else
		ShaderParams.FluidTexCoordTexture = TexCoordTarget_HalfRes->GetRenderTargetItem().ShaderResourceTexture;
		ShaderParams.FluidTexCoordWTexture = TexCoordWTarget_HalfRes->GetRenderTargetItem().ShaderResourceTexture;
#endif

		//atatarinov_end
	}
	else
	{
		ShaderParams.FluidTexCoordTexture = nullptr;
		ShaderParams.FluidTexCoordWTexture = nullptr;
	}

	//render Diffuse Particles
	TRefCountPtr<IPooledRenderTarget> FoamTarget;

	if (RenderingParams.bDiffuseParticlesEnabled)
	{
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_FloatRGBA, FClearValueBinding::None, TexCreate_FastVRAM, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
			Desc.NumSamples = 1;// SceneContext.GetSceneColor()->GetDesc().NumSamples;
								//Desc.ArraySize = 3;
								//Desc.bIsArray = true;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, FoamTarget, _TEXT("FluidFoam"));
		}

		SCOPED_DRAW_EVENT(RHICmdList, FluidDiffuseParticles);

		FTextureRHIParamRef RenderTargets[1] =
		{
			FoamTarget->GetRenderTargetItem().TargetableTexture,
		};

		SetRenderTargets(RHICmdList, 1, RenderTargets, ZBufferTarget->GetRenderTargetItem().TargetableTexture, 0, NULL);

		FLinearColor ClearColors[1] =
		{
			FLinearColor(0, 0, 0, 0)
		};
		//RHICmdList.ClearMRT(true, 1, ClearColors, false, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect()); CHECKME
		RHICmdList.ClearColorTextures(1, RenderTargets, ClearColors, FIntRect());
		RHICmdList.ClearDepthStencilTexture(ZBufferTarget->GetRenderTargetItem().TargetableTexture, EClearDepthStencil::Depth, (float)ERHIZBuffer::FarPlane, 0, FIntRect());


		// viewport to match view size
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		TShaderMapRef<FFluidRenderingDiffuseParticlesVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<FFluidRenderingDiffuseParticlesGS> GeometryShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<FFluidRenderingDiffuseParticlesPS> PixelShader(GetGlobalShaderMap(FeatureLevel));

		// Bound shader state.
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(
			RHICmdList,
			FeatureLevel,
			BoundShaderState,
			GEmptyVertexDeclaration.VertexDeclarationRHI,
			*VertexShader,
			*PixelShader,
			*GeometryShader
			);

		VertexShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
		PixelShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
		GeometryShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);

		RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		RHICmdList.SetStreamSource(0, NULL, 0, 0);
		RHICmdList.DrawPrimitiveIndirect(PT_PointList, RenderingParams.DiffuseParticlesDrawArgsBuffer, 0);

		RHICmdList.CopyToResolveTarget(FoamTarget->GetRenderTargetItem().TargetableTexture, FoamTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());

		ShaderParams.FluidFoamTexture = FoamTarget->GetRenderTargetItem().ShaderResourceTexture;
	}

	//Final render
	if (FluidConsoleVariables::RenderMode == 1)
	{
		SCOPED_DRAW_EVENT(RHICmdList, RenderFluidSurfaceMode1);
		RenderFluidSurface(RHICmdList, View, RenderingParams, ShaderParams, ZBufferTarget);
	}
	else
	{
		ShaderParams.FluidDepthTexture = DepthTarget->GetRenderTargetItem().ShaderResourceTexture;
		ShaderParams.FluidDepthDistTexture = DepthDistTarget->GetRenderTargetItem().ShaderResourceTexture;

		SCOPED_DRAW_EVENT(RHICmdList, RenderFluidSurfaceMode0);
		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite);

		// viewport to match view size
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		//disable DepthTest & enable DepthWrite
		//RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), FLUID_STENCIL_VALUE);

		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FFluidRenderingFinalPS> PixelShader(View.ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		VertexShader->SetParameters(RHICmdList, View);
		PixelShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);

		// Draw a quad mapping scene color to the view's render target
		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Size(),
			SceneContext.GetBufferSizeXY(),
			*VertexShader,
			EDRF_Default);

		SceneContext.FinishRenderingSceneColor(RHICmdList, true);
	}
#if 0
	//render Diffuse Particles
	if (RenderingParams.bDiffuseParticlesEnabled)
	{
		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite);

		// viewport to match view size
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		TShaderMapRef<FFluidRenderingDiffuseParticlesVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<FFluidRenderingDiffuseParticlesGS> GeometryShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<FFluidRenderingDiffuseParticlesPS> PixelShader(GetGlobalShaderMap(FeatureLevel));

		// Bound shader state.
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(
			RHICmdList,
			FeatureLevel,
			BoundShaderState,
			GEmptyVertexDeclaration.VertexDeclarationRHI,
			*VertexShader,
			*PixelShader,
			*GeometryShader
			);

		VertexShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
		PixelShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);
		GeometryShader->SetParameters(RHICmdList, View, RenderingParams, ShaderParams);

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		//disable DepthTest & enable DepthWrite
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		//RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		RHICmdList.SetStreamSource(0, NULL, 0, 0);
		RHICmdList.DrawPrimitiveIndirect(PT_PointList, RenderingParams.DiffuseParticlesDrawArgsBuffer, 0);

		SceneContext.FinishRenderingSceneColor(RHICmdList, true);
	}
#endif
}
