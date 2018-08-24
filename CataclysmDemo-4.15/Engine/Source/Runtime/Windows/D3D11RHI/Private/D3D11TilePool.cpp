// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// CATACLYSM Extend RHI for VTR
#include "D3D11RHIPrivate.h"

FTilePoolRHIRef FD3D11DynamicRHI::RHICreateTilePool(FTextureRHIParamRef TextureRHI, uint32 Size)
{
	if (TextureRHI->GetTexture3D() != NULL)
	{
		TRefCountPtr<ID3D11Device3> Direct3DDevice3;
		VERIFYD3D11RESULT(Direct3DDevice->QueryInterface(__uuidof(ID3D11Device3), (void**)Direct3DDevice3.GetInitReference()));        
		
		uint32 MaxNumTiles;
		D3D11_PACKED_MIP_DESC MipPacking;
		D3D11_TILE_SHAPE Shape;

		uint32 SubresourceTilings = 1;
		D3D11_SUBRESOURCE_TILING Tiling;
		Direct3DDevice3->GetResourceTiling((ID3D11Texture3D*)TextureRHI->GetNativeResource(), &MaxNumTiles, &MipPacking, &Shape, &SubresourceTilings, 0, &Tiling);

		uint32 NumTiles = (Size + D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES - 1) / D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
		uint32 TileX = Tiling.WidthInTiles;
		uint32 TileY = Tiling.HeightInTiles;
		uint32 TileZ = Tiling.DepthInTiles;
		check(NumTiles <= TileX * TileY * TileZ);

		D3D11_BUFFER_DESC Desc;
		ZeroMemory( &Desc, sizeof( D3D11_BUFFER_DESC ) );
		Desc.ByteWidth = D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES * NumTiles;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = 0;
		Desc.CPUAccessFlags = 0;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;
		Desc.StructureByteStride = 0;

		TRefCountPtr<ID3D11Buffer> TilePool;
		VERIFYD3D11RESULT(Direct3DDevice->CreateBuffer(&Desc, NULL, TilePool.GetInitReference()));

		UpdateBufferStats(TilePool, true);

		return new FD3D11TilePool(TilePool, TileX, TileY, TileZ, NumTiles, D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES);
	}

	return new FRHITilePool(0, 0, 0, 0, 0);
}

void FD3D11DynamicRHI::RHIResizeTilePool(FTilePoolRHIParamRef TilePoolRHI, uint32 Size)
{
	FD3D11TilePool* TilePool = ResourceCast(TilePoolRHI);
	TRefCountPtr<ID3D11DeviceContext3> Direct3DDeviceIMContext3;
	VERIFYD3D11RESULT(Direct3DDeviceIMContext->QueryInterface(__uuidof(ID3D11DeviceContext3), (void**)Direct3DDeviceIMContext3.GetInitReference()));        

	uint32 NumTiles = (Size + D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES - 1) / D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
	check(NumTiles <= TilePool->GetMaxNumTiles());

	VERIFYD3D11RESULT(Direct3DDeviceIMContext3->ResizeTilePool(TilePool->Resource, NumTiles * D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES));

	TilePool->UpdateNumTiles(NumTiles);
}

void FD3D11DynamicRHI::RHIUpdateTileMappings(FTextureRHIParamRef TextureRHI, uint32 NumTiledResourceRegions, const TArray<FTiledResourceCoordinate>& StartCoordinates, const TArray<FTileRegionSize>& Sizes,
											 FTilePoolRHIParamRef TilePoolRHI, uint32 NumRanges, const TArray<uint32>& Flags, const TArray<uint32>& StartOffsets, const TArray<uint32>& Counts)
{
	FD3D11TilePool* TilePool = ResourceCast(TilePoolRHI);

	check(StartCoordinates.Num() == 0 || StartCoordinates.Num() == NumTiledResourceRegions);
	check(Sizes.Num() == 0 || Sizes.Num() == NumTiledResourceRegions);
	check(Flags.Num() == 0 || Flags.Num() == NumRanges);
	check(StartOffsets.Num()==0 || StartOffsets.Num() == NumRanges);
	check(Counts.Num() == 0 || Counts.Num() == NumRanges);

	TRefCountPtr<ID3D11DeviceContext3> Direct3DDeviceIMContext3;
	VERIFYD3D11RESULT(Direct3DDeviceIMContext->QueryInterface(__uuidof(ID3D11DeviceContext3), (void**)Direct3DDeviceIMContext3.GetInitReference()));

	const FTiledResourceCoordinate * pStartCoordinates = StartCoordinates.Num() ? StartCoordinates.GetData() : nullptr;
	const FTileRegionSize * pSizes = Sizes.Num() ? Sizes.GetData() : nullptr;
	const uint32 * pFlags = Flags.Num() ? Flags.GetData() : nullptr;
	const uint32 * pStartOffsets = StartOffsets.Num() ? StartOffsets.GetData() : nullptr;
	const uint32 * pCounts = Counts.Num() ? Counts.GetData() : nullptr;

	// The direct passing of StartCoordinates, Sizes, and Flags works because the FTiledResourceCoordinate==D3D11_TILED_RESOURCE_COORDINATE, FTileRegionSize==D3D11_TILE_REGION_SIZE, and ETileRangeFlag==D3D11_TILE_RANGE_FLAG
	VERIFYD3D11RESULT(Direct3DDeviceIMContext3->UpdateTileMappings(
		(ID3D11Texture3D*)TextureRHI->GetNativeResource(), NumTiledResourceRegions, 
		(const D3D11_TILED_RESOURCE_COORDINATE*)pStartCoordinates,
		(const D3D11_TILE_REGION_SIZE*)pSizes,
		TilePool->Resource,
		NumRanges, 
		pFlags,
		pStartOffsets, 
		pCounts,
		D3D11_TILE_MAPPING_NO_OVERWRITE));//0));// TODO Why can't we use this? 
}