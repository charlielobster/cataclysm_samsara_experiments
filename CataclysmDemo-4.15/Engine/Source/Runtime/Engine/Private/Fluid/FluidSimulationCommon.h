#pragma once

// CATACLYSM 
#include "../../../../../Shaders/FluidCommonDefines.usf"


static_assert(LS_MULTIPLIER == 1 || USE_HUGE_DOMAIN == 0, "Can't use HUGE domain with a LS_MULTILIER of 2.");
static_assert((USE_SMALL_DOMAIN + USE_MEDIUM_DOMAIN + USE_LARGE_DOMAIN + USE_ALPHA_DOMAIN + USE_HUGE_DOMAIN) <= 1, "Pick only one domain size, defaults to large");

static_assert(NUM_BRICKS_X == DOMAIN_SIZE_X / BRICK_SIZE_X, "Domain size, brick size, and num bricks don't mach in X dimension.");
static_assert(NUM_BRICKS_Y == DOMAIN_SIZE_Y / BRICK_SIZE_Y, "Domain size, brick size, and num bricks don't mach in Y dimension.");
static_assert(NUM_BRICKS_Z == DOMAIN_SIZE_Z / BRICK_SIZE_Z, "Domain size, brick size, and num bricks don't mach in Z dimension.");

static_assert(LS_DOMAIN_SIZE_X <= 2048 && LS_DOMAIN_SIZE_Y <= 2048 && LS_DOMAIN_SIZE_Z <= 1024, "Domain size is too large to hold levelset voxel address in a uint32");

static_assert(CTA_SIZE_X <= BRICK_SIZE_X, "cooperative thread array size larger then brick size in X");
static_assert(CTA_SIZE_Y <= BRICK_SIZE_Y, "cooperative thread array size larger then brick size in Y");
static_assert(CTA_SIZE_Z <= BRICK_SIZE_Z, "cooperative thread array size larger then brick size in Z");

static_assert(LS_MULTIPLIER == 2 || LS_MULTIPLIER == 1, "Levelset multiplier must be 1 or 2.");

// USE_FAKE_VTR to 1 will NOT use VTR resources.  If you use large domains and fake vtr, you probably won't have enough memory.  Fake vtr is faster because
// it does not have to re-map the VTR resources.
#if USE_SMALL_DOMAIN
#define USE_FAKE_VTR 1
#elif USE_MEDIUM_DOMAIN
#define USE_FAKE_VTR 1
#else
#define USE_FAKE_VTR 0
#endif

#define VTR_TILE_SIZE_IN_BYTES 65536
#if USE_SMALL_DOMAIN
#define VTR_DEFAULT_NUM_TILES 512
#else
#define VTR_DEFAULT_NUM_TILES 1024
#endif
#define LS_VTR_DEFAULT_NUM_TILES (VTR_DEFAULT_NUM_TILES*LS_MULTIPLIER*LS_MULTIPLIER)

#define MAX_NUM_CHANGED_BRICKS 65536

#define MAX_NUM_COUNT_BRICKS 65536

DECLARE_LOG_CATEGORY_EXTERN(CataclysmInfo, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(CataclysmErrors, Log, All);

struct FBrickIndex
{
public:
	uint32 Index;

	FBrickIndex(uint32 idx)
	{
		Index = idx;
	}
	FBrickIndex(uint32 x, uint32 y, uint32 z)
	{
		Index = CoordToIdx(x, y, z);
	}
	inline static uint32 CoordToIdx(uint32 x, uint32 y, uint32 z) 
	{
		return BRICK_IDX_MASK & ((((z << BRICK_IDX_BITS_Y) | y) << BRICK_IDX_BITS_X) | x);
	}

	inline void GetCoordinates(uint32& x, uint32& y, uint32& z) const
	{
		x = Index & BRICK_IDX_MASK_X;
		y = (Index >> BRICK_IDX_BITS_X) & BRICK_IDX_MASK_Y;
		z = (Index >> (BRICK_IDX_BITS_X + BRICK_IDX_BITS_Y)) & BRICK_IDX_MASK_Z;
	}
};

bool operator==(const FBrickIndex& A, const FBrickIndex& B);
bool operator!=(const FBrickIndex& A, const FBrickIndex& B);

enum EFluidRegionType
{
	Trigger = 0,	// area for sending back overlap events
	Field,		// area for changing fluid behavior
};

enum EFluidRegionFlags
{
	KillInside = 0x01, // Kill particles outside the region
	KillOutside = 0x02, // Kill Particles indide the region
};