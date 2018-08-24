#pragma once
// CATACLYSM 
#include "FluidSimulationCommon.h"
#include "IndirectAppendBuffer.h"

struct FLevelset;

class FSparseGrid
{
public:
	enum ETileBPP
	{
		Bits_32 = 0,
		Bits_LS32 = 1,
#if LS_MULTIPLIER == 1
		Bits_Num = 1,
#else
		Bits_Num = 2,
#endif
		Bits_64 = 2,
	};
	enum ETexDataType
	{
		TDT_FLOAT = 0x80,
		TDT_FLOAT2 = 0x40,
		TDT_FLOAT4 = 0x20,
		TDT_UINT = 0x10,
	};
	struct TypedVTRStructured : public FVTRStructured<USE_FAKE_VTR>
	{

		TypedVTRStructured(EPixelFormat format, bool isLS = false);

		void Initialize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 AdditionalUsage = 0);

		EPixelFormat PixelFormat;
		ETileBPP BPP;
		ETexDataType TDT;
	};
	uint32 UnmappedCount;

	// A Tile is a VTR allocation unit in a tile pool, it's address is based on the type of resource.
	// 32bit resources have 32x32x16 size tiles
	// 64bit resources have 32x16x16 size tiles
	// 128bit resources have 16x16x16 size tiles.

	// A Brick is an 8x8x8 piece of a tile. We keep track of the brick map, and make sure the tile mappings for all resources overlap.
	TArray<uint32> BrickMap;
	TArray<FBrickIndex> ActiveBricks;// address into the brick map of the active bricks.  Use this to look up the tile address in the TileMap
	TArray<FBrickIndex> NeededBricks;// address into the brick map of bricks we need to add.

private:
	uint32 NumTilesInPool[Bits_Num]; // The current number of tiles total in the pool, both free and active.
	TArray<uint32> FreeTileList[Bits_Num]; // List of available tiles offsets (addresses, indexes, whatever) that are not mapped into a pool.
	TArray<uint32> TileMap[Bits_Num]; // The actual 3d tile map, NOT_MAPPED for unmapped, tile address [0 ... NumTilesInPool[i] - 1], if mapped.  May hold NEEDS_MAPPED or KEEP_MAPPED flags if we are within a frame.
	TArray<uint32> ActiveTiles[Bits_Num];// address into the tile map of the active tiles.  Use this to look up the tile address in the TileMap
	TArray<uint32> NeededTiles[Bits_Num];// address into the tile map of tiles we need to add.  They should map to NEEDES_MAPPED addresses in the tile map.
	TArray<uint32> TilesToUnmap[Bits_Num];// address into the tile map of tiles we need to unmap.

	// Params used for update tile mappings
	TArray<FTiledResourceCoordinate> Coords;
	TArray<uint32> Offsets;
	TArray<uint32> Flags;
	TArray<FTileRegionSize> Sizes;
	TArray<uint32> Counts;

public:
	FIndirectAppendBuffer* ActiveBricksOnGPU;

	FSparseGrid();
	~FSparseGrid();

	void Destroy();

	static void GetTileAddresses(const FBrickIndex &idx, uint32* addresses);

	int32 NumActiveBricks() const { return ActiveBricks.Num(); }

	void ProcessActiveBricksFromGPU(FBrickIndex* brickList, uint32 numBricks);

    void UpdateTiledResources(FRHICommandListImmediate& RHICmdList, const TArray<TypedVTRStructured*>& TileResources);

	void VisualizeDomain(FPrimitiveDrawInterface* PDI, const FMatrix& BrickToWorld, const FMatrix& VoxelToWorld, const FMatrix&LevelsetToWorld, int32 ShowFlags);
};