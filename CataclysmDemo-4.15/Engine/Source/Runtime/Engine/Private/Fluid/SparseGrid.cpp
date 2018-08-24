// CATACLYSM 
#include "SparseGrid.h"
#include "EnginePrivatePCH.h"
#include "../Particles/ParticleSimulationGPU.h"

// Tile size information and how it relates to the grid and the bricks.
#define VTR_32BPP_SIZE_X 32
#define VTR_32BPP_SIZE_Y 32
#define VTR_32BPP_SIZE_Z 16

#define VTR_64BPP_SIZE_X 32
#define VTR_64BPP_SIZE_Y 16
#define VTR_64BPP_SIZE_Z 16

const int VTR_SIZES_X[] = { VTR_32BPP_SIZE_X , VTR_32BPP_SIZE_X , VTR_64BPP_SIZE_X };
const int VTR_SIZES_Y[] = { VTR_32BPP_SIZE_Y , VTR_32BPP_SIZE_Y , VTR_64BPP_SIZE_Y };
const int VTR_SIZES_Z[] = { VTR_32BPP_SIZE_Z , VTR_32BPP_SIZE_Z , VTR_64BPP_SIZE_Z };

// The number of bits it takes to hold the address of a brick in a tile... so if the tile is 32x16x8 then this would be 2 1 0 for 8x8x8 brick
#if BRICK_SIZE_X == 8
#define BRICK_TO_VTR_32BPP_BITS_X 2
#define BRICK_TO_VTR_32BPP_BITS_Y 2
#define BRICK_TO_VTR_32BPP_BITS_Z 1

#define BRICK_TO_VTR_64BPP_BITS_X 2
#define BRICK_TO_VTR_64BPP_BITS_Y 1
#define BRICK_TO_VTR_64BPP_BITS_Z 1

#elif BRICK_SIZE_X == 4
#define BRICK_TO_VTR_32BPP_BITS_X 3
#define BRICK_TO_VTR_32BPP_BITS_Y 3
#define BRICK_TO_VTR_32BPP_BITS_Z 2

#define BRICK_TO_VTR_64BPP_BITS_X 3
#define BRICK_TO_VTR_64BPP_BITS_Y 2
#define BRICK_TO_VTR_64BPP_BITS_Z 2

#elif
static_assert(0, "Need to fix this for a new brick size.")

#endif
#if LS_BRICK_SIZE_X == 16
#define LS_BRICK_TO_VTR_32BPP_BITS_X 1
#define LS_BRICK_TO_VTR_32BPP_BITS_Y 1
#define LS_BRICK_TO_VTR_32BPP_BITS_Z 0

#elif LS_BRICK_SIZE_X == 8
#define LS_BRICK_TO_VTR_32BPP_BITS_X 2
#define LS_BRICK_TO_VTR_32BPP_BITS_Y 2
#define LS_BRICK_TO_VTR_32BPP_BITS_Z 1

#elif LS_BRICK_SIZE_X == 4
#define LS_BRICK_TO_VTR_32BPP_BITS_X 3
#define LS_BRICK_TO_VTR_32BPP_BITS_Y 3
#define LS_BRICK_TO_VTR_32BPP_BITS_Z 2

#elif
static_assert(0, "Need to fix this for a new levelset brick size.")

#endif

#define LS_NUM_VTR_32BPP_X (LS_DOMAIN_SIZE_X/VTR_32BPP_SIZE_X)
#define LS_NUM_VTR_32BPP_Y (LS_DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y)
#define LS_NUM_VTR_32BPP_Z (LS_DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z)
#define LS_NUM_VTR_32BPP (LS_NUM_VTR_32BPP_X*LS_NUM_VTR_32BPP_Y*LS_NUM_VTR_32BPP_Z)

#define NUM_VTR_32BPP_X (DOMAIN_SIZE_X/VTR_32BPP_SIZE_X)
#define NUM_VTR_32BPP_Y (DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y)
#define NUM_VTR_32BPP_Z (DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z)
#define NUM_VTR_32BPP (NUM_VTR_32BPP_X*NUM_VTR_32BPP_Y*NUM_VTR_32BPP_Z)

#define NUM_VTR_64BPP_X (DOMAIN_SIZE_X/VTR_64BPP_SIZE_X)
#define NUM_VTR_64BPP_Y (DOMAIN_SIZE_Y/VTR_64BPP_SIZE_Y)
#define NUM_VTR_64BPP_Z (DOMAIN_SIZE_Z/VTR_64BPP_SIZE_Z)
#define NUM_VTR_64BPP (NUM_VTR_64BPP_X*NUM_VTR_64BPP_Y*NUM_VTR_64BPP_Z)

#if USE_SMALL_DOMAIN
#define LS_VTR_32BPP_IDX_BITS_X  (3+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define LS_VTR_32BPP_IDX_BITS_Y  (4+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define LS_VTR_32BPP_IDX_BITS_Z  (2+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_32BPP_IDX_BITS_X  3 // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define VTR_32BPP_IDX_BITS_Y  4 // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define VTR_32BPP_IDX_BITS_Z  2 // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_64BPP_IDX_BITS_X  3 // bits in DOMAIN_SIZE_X/VTR_64BPP_SIZE_X - 1
#define VTR_64BPP_IDX_BITS_Y  5 // bits in DOMAIN_SIZE_Y/VTR_64BPP_SIZE_Y - 1
#define VTR_64BPP_IDX_BITS_Z  2 // bits in DOMAIN_SIZE_Z/VTR_64BPP_SIZE_Z - 1

#elif USE_MEDIUM_DOMAIN
#define LS_VTR_32BPP_IDX_BITS_X  (3+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define LS_VTR_32BPP_IDX_BITS_Y  (4+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define LS_VTR_32BPP_IDX_BITS_Z  (3+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_32BPP_IDX_BITS_X  3 // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define VTR_32BPP_IDX_BITS_Y  4 // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define VTR_32BPP_IDX_BITS_Z  3 // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_64BPP_IDX_BITS_X  3 // bits in DOMAIN_SIZE_X/VTR_64BPP_SIZE_X - 1
#define VTR_64BPP_IDX_BITS_Y  5 // bits in DOMAIN_SIZE_Y/VTR_64BPP_SIZE_Y - 1
#define VTR_64BPP_IDX_BITS_Z  3 // bits in DOMAIN_SIZE_Z/VTR_64BPP_SIZE_Z - 1

#elif USE_HUGE_DOMAIN
#define LS_VTR_32BPP_IDX_BITS_X  (6+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define LS_VTR_32BPP_IDX_BITS_Y  (6+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define LS_VTR_32BPP_IDX_BITS_Z  (6+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_32BPP_IDX_BITS_X  6 // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define VTR_32BPP_IDX_BITS_Y  6 // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define VTR_32BPP_IDX_BITS_Z  6 // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_64BPP_IDX_BITS_X  6 // bits in DOMAIN_SIZE_X/VTR_64BPP_SIZE_X - 1
#define VTR_64BPP_IDX_BITS_Y  7 // bits in DOMAIN_SIZE_Y/VTR_64BPP_SIZE_Y - 1
#define VTR_64BPP_IDX_BITS_Z  6 // bits in DOMAIN_SIZE_Z/VTR_64BPP_SIZE_Z - 1

#elif USE_ALPHA_DOMAIN
#define LS_VTR_32BPP_IDX_BITS_X  (5+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define LS_VTR_32BPP_IDX_BITS_Y  (5+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define LS_VTR_32BPP_IDX_BITS_Z  (5+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_32BPP_IDX_BITS_X  5 // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define VTR_32BPP_IDX_BITS_Y  5 // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define VTR_32BPP_IDX_BITS_Z  5 // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_64BPP_IDX_BITS_X  5 // bits in DOMAIN_SIZE_X/VTR_64BPP_SIZE_X - 1
#define VTR_64BPP_IDX_BITS_Y  6 // bits in DOMAIN_SIZE_Y/VTR_64BPP_SIZE_Y - 1
#define VTR_64BPP_IDX_BITS_Z  5 // bits in DOMAIN_SIZE_Z/VTR_64BPP_SIZE_Z - 1

#else // USE_LARGE_DOMAIN
#define LS_VTR_32BPP_IDX_BITS_X  (4+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define LS_VTR_32BPP_IDX_BITS_Y  (4+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define LS_VTR_32BPP_IDX_BITS_Z  (4+LS_MULTIPLIER-1) // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_32BPP_IDX_BITS_X  4 // bits in DOMAIN_SIZE_X/VTR_32BPP_SIZE_X - 1
#define VTR_32BPP_IDX_BITS_Y  4 // bits in DOMAIN_SIZE_Y/VTR_32BPP_SIZE_Y - 1
#define VTR_32BPP_IDX_BITS_Z  4 // bits in DOMAIN_SIZE_Z/VTR_32BPP_SIZE_Z - 1

#define VTR_64BPP_IDX_BITS_X  4 // bits in DOMAIN_SIZE_X/VTR_64BPP_SIZE_X - 1
#define VTR_64BPP_IDX_BITS_Y  5 // bits in DOMAIN_SIZE_Y/VTR_64BPP_SIZE_Y - 1
#define VTR_64BPP_IDX_BITS_Z  4 // bits in DOMAIN_SIZE_Z/VTR_64BPP_SIZE_Z - 1

#endif

static_assert(LS_NUM_VTR_32BPP_X == (1 << LS_VTR_32BPP_IDX_BITS_X), "Levelset Domain size, tile 32 size, and LS_NUM_VTR_32BPP don't mach in X dimension.");
static_assert(LS_NUM_VTR_32BPP_Y == (1 << LS_VTR_32BPP_IDX_BITS_Y), "Levelset Domain size, tile 32 size, and LS_NUM_VTR_32BPP don't mach in Y dimension.");
static_assert(LS_NUM_VTR_32BPP_Z == (1 << LS_VTR_32BPP_IDX_BITS_Z), "Levelset Domain size, tile 32 size, and LS_NUM_VTR_32BPP don't mach in Z dimension.");
static_assert(NUM_VTR_32BPP_X == (1 << VTR_32BPP_IDX_BITS_X), "Domain size, tile 32 size, and NUM_VTR_32BPP don't mach in X dimension.");
static_assert(NUM_VTR_32BPP_Y == (1 << VTR_32BPP_IDX_BITS_Y), "Domain size, tile 32 size, and NUM_VTR_32BPP don't mach in Y dimension.");
static_assert(NUM_VTR_32BPP_Z == (1 << VTR_32BPP_IDX_BITS_Z), "Domain size, tile 32 size, and NUM_VTR_32BPP don't mach in Z dimension.");
static_assert(NUM_VTR_64BPP_X == (1 << VTR_64BPP_IDX_BITS_X), "Domain size, tile 64 size, and NUM_VTR_64BPP don't mach in X dimension.");
static_assert(NUM_VTR_64BPP_Y == (1 << VTR_64BPP_IDX_BITS_Y), "Domain size, tile 64 size, and NUM_VTR_64BPP don't mach in Y dimension.");
static_assert(NUM_VTR_64BPP_Z == (1 << VTR_64BPP_IDX_BITS_Z), "Domain size, tile 64 size, and NUM_VTR_64BPP don't mach in Z dimension.");

#define LS_VTR_32BPP_IDX_MASK (LS_NUM_VTR_32BPP - 1)
#define LS_VTR_32BPP_IDX_MASK_X (LS_NUM_VTR_32BPP_X - 1)
#define LS_VTR_32BPP_IDX_MASK_Y (LS_NUM_VTR_32BPP_Y - 1)
#define LS_VTR_32BPP_IDX_MASK_Z (LS_NUM_VTR_32BPP_Z - 1)

#define VTR_32BPP_IDX_MASK (NUM_VTR_32BPP - 1)
#define VTR_32BPP_IDX_MASK_X (NUM_VTR_32BPP_X - 1)
#define VTR_32BPP_IDX_MASK_Y (NUM_VTR_32BPP_Y - 1)
#define VTR_32BPP_IDX_MASK_Z (NUM_VTR_32BPP_Z - 1)

#define VTR_64BPP_IDX_MASK (NUM_VTR_64BPP - 1)
#define VTR_64BPP_IDX_MASK_X (NUM_VTR_64BPP_X - 1)
#define VTR_64BPP_IDX_MASK_Y (NUM_VTR_64BPP_Y - 1)
#define VTR_64BPP_IDX_MASK_Z (NUM_VTR_64BPP_Z - 1)

const uint32 kPaddingTiles = 50;

// The brick map's maximum address is going to be BRICK_IDX_MASK
const uint32 NOT_MAPPED   = 0x20000000;
const uint32 NEEDS_MAPPED = 0x40000000;
const uint32 KEEP_MAPPED  = 0x80000000;
const uint32 MAPPED_MASK  = 0xE0000000;
static_assert((MAPPED_MASK & BRICK_IDX_MASK) == 0, "Address for bricks is too large to worked with mapped mask");

inline void GetCoordsFromAddress(int32 bpp, uint32 address, uint32& x, uint32& y, uint32& z)
{
	switch (bpp)
	{
	case FSparseGrid::Bits_32:
		x = address & VTR_32BPP_IDX_MASK_X;
		y = (address >> VTR_32BPP_IDX_BITS_X) & VTR_32BPP_IDX_MASK_Y;
		z = (address >> (VTR_32BPP_IDX_BITS_X + VTR_32BPP_IDX_BITS_Y)) & VTR_32BPP_IDX_MASK_Z;
		break;
	case FSparseGrid::Bits_64:
		x = address & VTR_64BPP_IDX_MASK_X;
		y = (address >> VTR_64BPP_IDX_BITS_X) & VTR_64BPP_IDX_MASK_Y;
		z = (address >> (VTR_64BPP_IDX_BITS_X + VTR_64BPP_IDX_BITS_Y)) & VTR_64BPP_IDX_MASK_Z;
		break;
	case FSparseGrid::Bits_LS32:
		x = address & LS_VTR_32BPP_IDX_MASK_X;
		y = (address >> LS_VTR_32BPP_IDX_BITS_X) & LS_VTR_32BPP_IDX_MASK_Y;
		z = (address >> (LS_VTR_32BPP_IDX_BITS_X + LS_VTR_32BPP_IDX_BITS_Y)) & LS_VTR_32BPP_IDX_MASK_Z;
		break;
	default:
		check(0);// "Called GetCoordsFromAddress without a valid bpp
	}
}

FSparseGrid::TypedVTRStructured::TypedVTRStructured(EPixelFormat format, bool isLS)
	: PixelFormat(format)
{
	if (format == PF_G16R16F) TDT = TDT_FLOAT2, BPP = Bits_32;
	else if (format == PF_R32_FLOAT) TDT = TDT_FLOAT, BPP = (isLS ? Bits_LS32 : Bits_32);
//	else if (format == PF_FloatRGBA) TDT = TDT_FLOAT4, BPP = Bits_64;
	else if (format == PF_R32_UINT) TDT = TDT_UINT, BPP = (isLS ? Bits_LS32 : Bits_32);
	else check(0 && "pixel format needs to be added to TypedVTRStructured.");
}

// 32, 64, LS32
// TODO: When we can safely start with a lower number of tiles and resize the tile pool, then change default to lower.
//const uint32 DefaultNumTiles[] = { VTR_DEFAULT_NUM_TILES >> 5, (VTR_DEFAULT_NUM_TILES >> 4), (VTR_DEFAULT_NUM_TILES >> 5)*LS_MULTIPLIER*LS_MULTIPLIER };
//const uint32 MaxNumTiles[] = { VTR_DEFAULT_NUM_TILES >> 5, (VTR_DEFAULT_NUM_TILES >> 4), (VTR_DEFAULT_NUM_TILES >> 5)*LS_MULTIPLIER*LS_MULTIPLIER };
const uint32 DefaultNumTiles[] =     { VTR_DEFAULT_NUM_TILES, VTR_DEFAULT_NUM_TILES*LS_MULTIPLIER*LS_MULTIPLIER*LS_MULTIPLIER, (VTR_DEFAULT_NUM_TILES << 1) };
const uint32 MaxNumTiles[] =     { VTR_DEFAULT_NUM_TILES, VTR_DEFAULT_NUM_TILES*LS_MULTIPLIER*LS_MULTIPLIER*LS_MULTIPLIER, (VTR_DEFAULT_NUM_TILES << 1) };

void FSparseGrid::TypedVTRStructured::Initialize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 AdditionalUsage)
{
	uint32 NumTilePoolBytes = VTR_TILE_SIZE_IN_BYTES * DefaultNumTiles[BPP];
	FVTRStructured<USE_FAKE_VTR>::Initialize(SizeX, SizeY, SizeZ, PixelFormat, NumMips, NumTilePoolBytes, AdditionalUsage);
}

FSparseGrid::FSparseGrid()
	: ActiveBricksOnGPU(nullptr)
{
#if !USE_FAKE_VTR
	for (int32 bpp = 0; bpp < Bits_Num; ++bpp)
	{
		NumTilesInPool[bpp] = DefaultNumTiles[bpp];
		FreeTileList[bpp].SetNumUninitialized(NumTilesInPool[bpp]);
		for (uint32 j = 0; j < NumTilesInPool[bpp]; ++j) FreeTileList[bpp][j] = NumTilesInPool[bpp] - 1 - j;
	}
	TileMap[Bits_32].Init(NOT_MAPPED, NUM_VTR_32BPP);
	if (Bits_64 < Bits_Num) TileMap[Bits_64].Init(NOT_MAPPED, NUM_VTR_64BPP);
	if (Bits_LS32 < Bits_Num) TileMap[Bits_LS32].Init(NOT_MAPPED, LS_NUM_VTR_32BPP);
#endif

	BrickMap.Init(NOT_MAPPED, NUM_BRICKS);
	ActiveBricksOnGPU = new FIndirectAppendBuffer(NUM_BRICKS, sizeof(FBrickIndex));
}

FSparseGrid::~FSparseGrid()
{

}

void FSparseGrid::Destroy()
{
	if (ActiveBricksOnGPU)
	{
		ActiveBricksOnGPU->Destroy();
		ActiveBricksOnGPU = nullptr;
	}
	delete this;
}

void FSparseGrid::GetTileAddresses(const FBrickIndex &idx, uint32* addresses)
{
	uint32 x, y, z, x32, y32, z32;
	idx.GetCoordinates(x, y, z);
	x32 = x >> BRICK_TO_VTR_32BPP_BITS_X;
	y32 = y >> BRICK_TO_VTR_32BPP_BITS_Y;
	z32 = z >> BRICK_TO_VTR_32BPP_BITS_Z;
	addresses[Bits_32] = (((z32 << VTR_32BPP_IDX_BITS_Y) | y32) << VTR_32BPP_IDX_BITS_X) | x32;
	if (Bits_64 < Bits_Num)
	{
		uint32 x64, y64, z64;
		x64 = x >> BRICK_TO_VTR_64BPP_BITS_X;
		y64 = y >> BRICK_TO_VTR_64BPP_BITS_Y;
		z64 = z >> BRICK_TO_VTR_64BPP_BITS_Z;
		addresses[Bits_64] = (((z64 << VTR_64BPP_IDX_BITS_Y) | y64) << VTR_64BPP_IDX_BITS_X) | x64;
	}
	if (Bits_LS32 < Bits_Num)
	{
		uint32 xLS, yLS, zLS;
		xLS = x >> LS_BRICK_TO_VTR_32BPP_BITS_X;
		yLS = y >> LS_BRICK_TO_VTR_32BPP_BITS_Y;
		zLS = z >> LS_BRICK_TO_VTR_32BPP_BITS_Z;
		addresses[Bits_LS32] = (((zLS << LS_VTR_32BPP_IDX_BITS_Y) | yLS) << LS_VTR_32BPP_IDX_BITS_X) | xLS;
	}
}

void FSparseGrid::ProcessActiveBricksFromGPU(FBrickIndex* brickList, uint32 numBricks)
{
	// update the BrickMap with changes from the GPU.
	UnmappedCount = 0;
    for (uint32 i = 0; i < numBricks; i++)
    {
		// brick list holds only changed bricks from the GPU.  
		// They are either added, or they are removed.  If they are removed, they have 1 as 32nd bit of address.
		uint32 unmap = brickList[i].Index & 0x80000000;
		uint32 idx = brickList[i].Index & ~0x80000000;
		if (unmap)
		{
			BrickMap[idx] = NOT_MAPPED;
			UnmappedCount += 1;
		}
		else
		{
			check(BrickMap[idx] == NOT_MAPPED);
			BrickMap[idx] = idx;
			NeededBricks.Add(idx);
		}
    }	
	
	// Update the active brick list with the change list from the GPU.
	for (int32 i = 0; i < ActiveBricks.Num();)
	{
		FBrickIndex idx = ActiveBricks[i];
		if (BrickMap[idx.Index] == NOT_MAPPED)
		{
			ActiveBricks.RemoveAtSwap(i, 1, false);
		}
		else
		{
			++i;
		}
	}
	ActiveBricks.Append(NeededBricks);
	NeededBricks.Reset();

	// At this point, ActiveBricks and ActiveBricksOnGPU should have the same contents, but probably not in the same order.
}

// WARNING: if the input here is not the same every frame, them new resources won't be processed correctly.
// This assumes that resources of they same pbb are all mapped the same. 
void FSparseGrid::UpdateTiledResources(FRHICommandListImmediate& RHICmdList, const TArray<TypedVTRStructured*>& SimVTRs)
{
	// When we are using a small domain, we are not actually using tiled resources...
#if !USE_FAKE_VTR
	// Keep track of the types of resources so we don't do too much extra work.
	uint32 TypeCounts[3] = {0,0,0};
	for (int32 i = 0; i < SimVTRs.Num(); ++i)
	{
		TypeCounts[SimVTRs[i]->BPP] += 1;
	}

	uint32 NumToKeep[3] = {0,0,0};
	// Need to keep the tile maps and active tile list up to date.
	for (int32 i = 0; i < ActiveBricks.Num(); ++i)
	{
		FBrickIndex idx = ActiveBricks[i];
		uint32 TileAddress[Bits_Num];
		GetTileAddresses(idx, TileAddress);
		for (int32 bpp = 0; bpp < Bits_Num; ++bpp)
		{
			uint32 tidx = TileAddress[bpp];
			if (TileMap[bpp][tidx] == NOT_MAPPED)
			{
				TileMap[bpp][tidx] = NEEDS_MAPPED;
				NeededTiles[bpp].Add(tidx);
			}
			else if (TileMap[bpp][tidx] != NEEDS_MAPPED)
			{
				if (!(TileMap[bpp][tidx] & KEEP_MAPPED))
				{
					TileMap[bpp][tidx] |= KEEP_MAPPED;
					NumToKeep[bpp] += 1;
				}
			}
		}
	}
	//
	// ActiveTiles, need to be kept up to date by removing tiles that should not be mapped, and keeping lists of tiles for the actual VTR re-mapping.
	for (int32 bpp = 0; bpp < Bits_Num; ++bpp)
	{

		// KeepTiles array is <= max num tiles because we never should have mapped more then we can.
		check(NumToKeep[bpp] <= MaxNumTiles[bpp]);
		// If we need more then max, then we will remove from needed... TODO do this smartly so that we remove the least important needed tile?
		while (NumToKeep[bpp] + NeededTiles[bpp].Num() > MaxNumTiles[bpp])
		{
			uint32 tidx = NeededTiles[bpp].Pop(false);
			TileMap[bpp][tidx] = NOT_MAPPED;
		}

		// If we have more needed tiles then we have in the free list, we either need to get them by unmaping, or by growing the tile pool size.
		uint32 NumToUnmap = NeededTiles[bpp].Num() <= FreeTileList[bpp].Num() ? 0 : NeededTiles[bpp].Num() - FreeTileList[bpp].Num();
		if (!NumToUnmap && /*!NeededTiles[bpp].Num() &&*/ FreeTileList[bpp].Num() < 0.25f*MaxNumTiles[bpp]) NumToUnmap = 16; // lets start clearing the list of the least recently used tiles.
		for (int32 i = 0; i < ActiveTiles[bpp].Num();)
		{
			uint32 tidx = ActiveTiles[bpp][i];
			if (TileMap[bpp][tidx] & KEEP_MAPPED)
			{
				// The tile is mapped here and on the GPU, so its mapping is correct and all we need to do is get the keep marker off its address.
				TileMap[bpp][tidx] &= ~MAPPED_MASK;
				++i;
			}
			else
			{
				if (NumToUnmap)
				{
					NumToUnmap -= 1;
					if (TypeCounts[bpp])
					{
						// The tile is mapped, but it should not be, so we need to un map it.
						TilesToUnmap[bpp].Add(tidx);
						FreeTileList[bpp].Add(TileMap[bpp][tidx]);
					}
					TileMap[bpp][tidx] = NOT_MAPPED;

					// Do a quick replace from back so we can keep iterating.
					ActiveTiles[bpp].RemoveAtSwap(i, 1, false);
				}
				else
				{
					// we have active tiles with no bricks in them, but we don't need to unmap them for now because we don't have any more needed tiles.
					// let them stay mapped forever unless we need them.
					++i;
				}
			}
		}
		// TODO Do we need to worry about the fact that we can have brick map with mapped bricks and tiles that are mapped in that area?  Another advantage of the TODO of
		// keeping the tile maps on the GPU and only transferring them as we need... we would not need to keep the brick map on this side at all.

		if (TypeCounts[bpp] == 0)
		{
			// We are not actually dealing with resources types that we have, so don't bother doing the extra work of making the data structures to remap
			ActiveTiles[bpp].Append(NeededTiles[bpp]);
			for (int32 i = 0; i < NeededTiles[bpp].Num(); ++i)
			{
				uint32 neededAt = NeededTiles[bpp][i];
				TileMap[bpp][neededAt] = neededAt; // fake offset into a fake tile pool... with used tile maps this will be an offset into the pool
			}
			NeededTiles[bpp].Reset();
		}
		else
		{
			// We have at least one resource of the type bpp so we have to make sure the tiles are set up correctly.
			check(NumTilesInPool[bpp] == (ActiveTiles[bpp].Num() + FreeTileList[bpp].Num()));
			int32 NumActiveTiles = ActiveTiles[bpp].Num();
			int32 NumFreeTiles = FreeTileList[bpp].Num();
			int32 NumUnmapTiles = TilesToUnmap[bpp].Num();
			int32 NumNeededTiles = NeededTiles[bpp].Num();
			// If we have more needed tiles then in the free list, then we have to resize the tile pool.
			bool resizingPool = false;
			if (NeededTiles[bpp].Num() > FreeTileList[bpp].Num())
			{
				// resize the tile pool and add the new indices to the FreeTileList
				// but don't grow past the max size allowed.
				uint32 GrowSize = FMath::Min(MaxNumTiles[bpp] - NumTilesInPool[bpp], NeededTiles[bpp].Num() - FreeTileList[bpp].Num() + kPaddingTiles);
				uint32 OldNum = FreeTileList[bpp].AddUninitialized(GrowSize);
				for (uint32 i = 0; i < GrowSize; ++i)
				{
					FreeTileList[bpp][OldNum + i] = NumTilesInPool[bpp] + i; // Add the new tile index for the new tiles that will be in the pool after re-sizing.
				}

                UE_LOG(LogRHI, Log, L"VTR: Resizing tile pools (%d bpp) to old/grow/total %d/%d/%d tiles.\n", 32 << bpp, NumTilesInPool[bpp], GrowSize, NumTilesInPool[bpp] + GrowSize);
				resizingPool = true;
				for (int32 i = 0; i < SimVTRs.Num(); i++)
				{
					TypedVTRStructured* VTR = SimVTRs[i];
					if (VTR->BPP == bpp)
					{
						VTR->ResizeTilePool(NumTilesInPool[bpp] + GrowSize);
					}
				}
				NumTilesInPool[bpp] += GrowSize;
			}

			// try individual resource tiling, several regions of size 1.
			// We need null mappings for all the bricks in BricksToUnmap, and
			// we need specific offsets for the NeededBricks.
			uint32 NumMappings = TilesToUnmap[bpp].Num() + NeededTiles[bpp].Num();
			
			Coords.SetNum(NumMappings, false);
			Offsets.SetNum(NumMappings, false);
			Flags.SetNum(NumMappings, false);
//			Sizes.SetNum(NumMappings, false);
			Counts.SetNum(NumMappings, false);
			
			int32 j = 0;
			for (; j < TilesToUnmap[bpp].Num(); ++j)
			{
				GetCoordsFromAddress(bpp, TilesToUnmap[bpp][j], Coords[j].X, Coords[j].Y, Coords[j].Z);
				Coords[j].Subresource = 0;
				Offsets[j] = 0; // offset is ignored for NULL mappings
				Flags[j] = TRF_NULL;
//				Sizes[j].NumTiles = 1;
//				Sizes[j].bUseBox = 0x00;
//				Sizes[j].Width = 1;
//				Sizes[j].Height = 1;
//				Sizes[j].Depth = 1;
				Counts[j] = 1;

			}
			for (int32 i = 0; i < NeededTiles[bpp].Num(); ++i, ++j)
			{
				uint32 tileOffset = FreeTileList[bpp].Pop(false);
				uint32 neededAt = NeededTiles[bpp][i];
				ActiveTiles[bpp].Add(neededAt);
				TileMap[bpp][neededAt] = tileOffset;
				GetCoordsFromAddress(bpp, neededAt, Coords[j].X, Coords[j].Y, Coords[j].Z);
				Coords[j].Subresource = 0;
				Offsets[j] = tileOffset;
				Flags[j] = TRF_None; // No flag needed when mapping a unique tile.
//				Sizes[j].NumTiles = 1;
//				Sizes[j].bUseBox = 0x00;
//				Sizes[j].Width = 1;
//				Sizes[j].Height = 1;
//				Sizes[j].Depth = 1;
				Counts[j] = 1;
			}
			TilesToUnmap[bpp].Reset();
			NeededTiles[bpp].Reset();
			
			if (NumMappings)
			{
				//UE_LOG(LogRHI, Log, L"VTRUpdate: bpp[%d] bricks(%d) Tiles{a%d/f%d/n%d/u%d}.\n", bpp, ActiveBricks.Num(), NumActiveTiles, NumFreeTiles, NumNeededTiles, NumUnmapTiles);

				SCOPED_DRAW_EVENTF(RHICmdList, UpdateTileMappings, TEXT("UpdateTileMappings_%d_m%d_u%d"), TypeCounts[bpp], NumNeededTiles, NumUnmapTiles);
				for (int32 i = 0; i < SimVTRs.Num(); i++)
				{
					TypedVTRStructured* VTR = SimVTRs[i];
					if (VTR->BPP == bpp)
					{ 
						if (resizingPool)
							UE_LOG(LogRHI, Log, L"VTR: UpdateTileMapping [%d] %d.\n", i, NumMappings);
						VTR->UpdateTileMappings(NumMappings, Coords, Sizes, NumMappings, Flags, Offsets, Counts);
					}
				}
			}
		}
	}
#endif
}

namespace face
{
	enum {
		LEFT = 0x01,
		RIGHT = 0x02,
		DOWN = 0x04,
		UP = 0x08,
		BACK = 0x10,
		FRONT = 0x20 
	};
}
void DrawWireBoxFaces(FPrimitiveDrawInterface* PDI, const FBox& Box, uint8 Faces, const FLinearColor& Color, uint8 DepthPriority, float Thickness, float DepthBias, bool bScreenSpace)
{
	FVector	B[2], P, Q;

	B[0] = Box.Min;
	B[1] = Box.Max;

	P.X = B[0].X;
	P.Y = B[0].Y;
	P.Z = B[0].Z;
	if (Faces & face::LEFT && Faces & face::DOWN)
	{
		Q.X = B[0].X;
		Q.Y = B[0].Y;
		Q.Z = B[1].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	if (Faces & face::LEFT && Faces & face::BACK)
	{
		Q.X = B[0].X;
		Q.Y = B[1].Y;
		Q.Z = B[0].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	if (Faces & face::BACK && Faces & face::DOWN)
	{
		Q.X = B[1].X;
		Q.Y = B[0].Y;
		Q.Z = B[0].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	P.X = B[1].X;
	P.Y = B[1].Y;
	P.Z = B[1].Z;
	if (Faces & face::FRONT && Faces & face::UP)
	{
		Q.X = B[0].X;
		Q.Y = B[1].Y;
		Q.Z = B[1].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	if (Faces & face::FRONT && Faces & face::RIGHT)
	{
		Q.X = B[1].X;
		Q.Y = B[0].Y;
		Q.Z = B[1].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	if (Faces & face::RIGHT && Faces & face::UP)
	{
		Q.X = B[1].X;
		Q.Y = B[1].Y;
		Q.Z = B[0].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	P.X = B[0].X;
	P.Y = B[1].Y;
	P.Z = B[1].Z;
	if (Faces & face::LEFT && Faces & face::UP)
	{
		Q.X = B[0].X;
		Q.Y = B[1].Y;
		Q.Z = B[0].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	if (Faces & face::FRONT && Faces & face::LEFT)
	{
		Q.X = B[0].X;
		Q.Y = B[0].Y;
		Q.Z = B[1].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	P.X = B[1].X;
	P.Y = B[1].Y;
	P.Z = B[0].Z;
	if (Faces & face::BACK && Faces & face::UP)
	{
		Q.X = B[0].X;
		Q.Y = B[1].Y;
		Q.Z = B[0].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	if (Faces & face::BACK && Faces & face::RIGHT)
	{
		Q.X = B[1].X;
		Q.Y = B[0].Y;
		Q.Z = B[0].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}

	P.X = B[1].X;
	P.Y = B[0].Y;
	P.Z = B[1].Z;
	if (Faces & face::RIGHT && Faces & face::DOWN)
	{
		Q.X = B[1].X;
		Q.Y = B[0].Y;
		Q.Z = B[0].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	if (Faces & face::FRONT && Faces & face::DOWN)
	{
		Q.X = B[0].X;
		Q.Y = B[0].Y;
		Q.Z = B[1].Z;
		PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}

}
void FSparseGrid::VisualizeDomain(FPrimitiveDrawInterface* PDI, const FMatrix& BrickToWorld, const FMatrix& VoxelToWorld, const FMatrix& LevelsetToWorld, int32 ShowFlags)
{
	const FColor BrickColor = FColorList::BrightGold;
	const FColor DomainColor = FColorList::Red;
	const FColor VoxelColor = FColorList::BlueViolet;
	const FColor CenterBrickColor = FColorList::GreenYellow;
	const FColor TileColor[3] = { FColorList::Orange, FColorList::LimeGreen, FColorList::Plum };
	if (ShowFlags & 0x01)
	{
		for (int32 i = 0; i < ActiveBricks.Num(); ++i)
		{
			uint32 X, Y, Z;
			ActiveBricks[i].GetCoordinates(X, Y, Z);
			uint8 Faces(0);
			if (X == 0 || BrickMap[FBrickIndex::CoordToIdx(X - 1, Y, Z)] == NOT_MAPPED) Faces |= face::LEFT;
			if (Y == 0 || BrickMap[FBrickIndex::CoordToIdx(X, Y - 1, Z)] == NOT_MAPPED) Faces |= face::DOWN;
			if (Z == 0 || BrickMap[FBrickIndex::CoordToIdx(X, Y, Z - 1)] == NOT_MAPPED) Faces |= face::BACK;
			if (X >= NUM_BRICKS_X - 1 || BrickMap[FBrickIndex::CoordToIdx(X + 1, Y, Z)] == NOT_MAPPED) Faces |= face::RIGHT;
			if (Y >= NUM_BRICKS_Y - 1 || BrickMap[FBrickIndex::CoordToIdx(X, Y + 1, Z)] == NOT_MAPPED) Faces |= face::UP;
			if (Z >= NUM_BRICKS_Z - 1 || BrickMap[FBrickIndex::CoordToIdx(X, Y, Z + 1)] == NOT_MAPPED) Faces |= face::FRONT;
			FVector Min = BrickToWorld.TransformPosition(FVector(X, Y, Z));
			FVector Max = BrickToWorld.TransformPosition(FVector(X + 1, Y + 1, Z + 1));
			FBox Box = FBox(Min, Max);
			DrawWireBoxFaces(PDI, Box, Faces, BrickColor, SDPG_World, 2.0, 0, true);
		}
	}
#if 1
	// Visualize the tile maps
	for (int32 bpp = 0; bpp < Bits_Num; ++bpp)
	{
		if ((ShowFlags >> (1 + bpp)) & 0x01)
		{
			for (int32 i = 0; i < ActiveTiles[bpp].Num(); ++i)
			{
				uint32 X, Y, Z;
				GetCoordsFromAddress(bpp, ActiveTiles[bpp][i], X, Y, Z);
				X *= VTR_SIZES_X[bpp];
				Y *= VTR_SIZES_Y[bpp];
				Z *= VTR_SIZES_Z[bpp];
				FMatrix ToWorld = bpp == Bits_LS32 ? LevelsetToWorld : VoxelToWorld;
				FVector Min = ToWorld.TransformPosition(FVector(X, Y, Z)) + FVector(1 + bpp, 1 + bpp, 1 + bpp);
				FVector Max = ToWorld.TransformPosition(FVector(X + VTR_SIZES_X[bpp], Y + VTR_SIZES_Y[bpp], Z + VTR_SIZES_Z[bpp])) - FVector(1 + bpp, 1 + bpp, 1 + bpp);
				FBox Box = FBox(Min, Max);
				DrawWireBox(PDI, Box, TileColor[bpp], SDPG_World, 2.0, 0, true);
			}
		}
	}
#endif

	// Visualize the entire Domain
	{
		FVector Min = BrickToWorld.TransformPosition(FVector(0,0,0));
		FVector Max = BrickToWorld.TransformPosition(FVector(NUM_BRICKS_X, NUM_BRICKS_Y, NUM_BRICKS_Z));

		FBox Box = FBox(Min, Max);
		DrawWireBox(PDI, Box, DomainColor, SDPG_World, 2.0, 0, true);
	}
	// Visualize the bricks in the center, with some voxels, even if it is not occupied.
	{
		// center 8 bricks
		for (uint32 Z = 0; Z < 1; Z++)
		{
			for (uint32 Y = 0; Y < 1; Y++)
			{
				for (uint32 X = 0; X < 1; X++)
				{
					uint32 A = (NUM_BRICKS_X >> 1) + X;
					uint32 B = (NUM_BRICKS_Y >> 1) + Y;
					uint32 C = (NUM_BRICKS_Z >> 1) + Z;
					FVector Min = BrickToWorld.TransformPosition(FVector(A,B,C));
					FVector Max = BrickToWorld.TransformPosition(FVector(A+1, B+1, C+1));
					FBox Box = FBox(Min, Max);
					DrawWireBox(PDI, Box, CenterBrickColor, SDPG_World, 2.0, 0, true);
				}
			}
		}
		// center voxels of one brick amount
		for (uint32 Z = 0; Z < 2; Z++)
		{
			for (uint32 Y = 0; Y < 2; Y++)
			{
				for (uint32 X = 0; X < 2; X++)
				{
					uint32 A = (DOMAIN_SIZE_X >> 1) + (BRICK_SIZE_X >> 1) - 1 + X;
					uint32 B = (DOMAIN_SIZE_Y >> 1) + (BRICK_SIZE_Y >> 1) - 1 + Y;
					uint32 C = (DOMAIN_SIZE_Z >> 1) + (BRICK_SIZE_Z >> 1) - 1 + Z;
					FVector Min = VoxelToWorld.TransformPosition(FVector(A,B,C));
					FVector Max = VoxelToWorld.TransformPosition(FVector(A+1, B+1, C+1));
					FBox Box = FBox(Min, Max);
					DrawWireBox(PDI, Box, VoxelColor, SDPG_World, 2.0, 0, true);
				}
			}
		}
	}

}