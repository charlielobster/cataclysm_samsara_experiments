// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleSimulationGPU.h: Interface to GPU particle simulation.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"

/*------------------------------------------------------------------------------
	Constants to tune memory and performance for GPU particle simulation.
------------------------------------------------------------------------------*/

/** The texture size allocated for GPU simulation. */
extern const int32 GParticleSimulationTextureSizeX;
extern const int32 GParticleSimulationTextureSizeY;

/** The tile size. Texture space is allocated in TileSize x TileSize units. */
extern const int32 GParticleSimulationTileSize;
extern const int32 GParticlesPerTile;

/** How many tiles are in the simulation textures. */
extern const int32 GParticleSimulationTileCountX;
extern const int32 GParticleSimulationTileCountY;
extern const int32 GParticleSimulationTileCount;

// CATACLYSM Begin
// This was moved from ParticleGpuSimulation.cpp

class FParticleTileVertexBuffer : public FVertexBuffer
{
public:
	/** Shader resource of the vertex buffer. */
	FShaderResourceViewRHIRef VertexBufferSRV;
	/** The number of tiles held by this vertex buffer. */
	int32 TileCount;
	/** The number of tiles held by this vertex buffer, aligned for tile rendering. */
	int32 AlignedTileCount;

	/** Default constructor. */
	FParticleTileVertexBuffer()
		: TileCount(0)
		, AlignedTileCount(0)
	{
	}
	
	
	FShaderResourceViewRHIParamRef GetShaderParam() { return VertexBufferSRV; }

	/**
	 * Initializes the vertex buffer from a list of tiles.
	 */
	void Init( const TArray<uint32>& Tiles );

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override;

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override;
};

class FParticleIndicesVertexBuffer : public FVertexBuffer
{
public:

	/** Shader resource view of the vertex buffer. */
	FShaderResourceViewRHIRef VertexBufferSRV;

	/** Release RHI resources. */
	virtual void ReleaseRHI() override;
};

class FGPUParticleVertexBuffer : public FParticleIndicesVertexBuffer
{
public:

	/** The number of particles referenced by this vertex buffer. */
	int32 ParticleCount;

	/** Default constructor. */
	FGPUParticleVertexBuffer()
		: ParticleCount(0)
	{
	}

	/**
	 * Initializes the vertex buffer from a list of tiles.
	 */
	void Init( const TArray<uint32>& Tiles );

	/** Initialize RHI resources. */
	virtual void InitRHI() override;
};

// CATACLYSM End
