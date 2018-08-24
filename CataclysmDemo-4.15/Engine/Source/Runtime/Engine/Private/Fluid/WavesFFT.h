#pragma once
// CATACLYSM 
#include "FluidSimulationCommon.h"


class FWavesFFT
{
public:
	FWavesFFT(uint32 InSize, float VoxelWidth, float WavesLengthPeriod, float WavesMaxWaveLength, float WavesWindSpeed);
	~FWavesFFT();

	void Destroy();

	void GenerateSpectrum(float VoxelWidth, float WavesLengthPeriod, float WavesMaxWaveLength, float WavesWindSpeed);

	FTexture2DRHIRef Update(FRHICommandListImmediate& RHICmdList, float Time);

private:
	uint32 Size;
	uint32 SizeBits;
	float GridStep;

	FTexture2DRHIRef SpectrumTexture;
	FTexture2DRHIRef TextureFFT[2];
	FUnorderedAccessViewRHIRef TextureFFT_UAV[2];
};
