// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2015 NVIDIA Corporation. All rights reserved.

// This file was generated by NvParameterized/scripts/GenParameterized.pl


#ifndef HEADER_ClothingGraphicalLodParameters_0p3_h
#define HEADER_ClothingGraphicalLodParameters_0p3_h

#include "NvParametersTypes.h"

#ifndef NV_PARAMETERIZED_ONLY_LAYOUTS
#include "nvparameterized/NvParameterized.h"
#include "nvparameterized/NvParameterizedTraits.h"
#include "NvParameters.h"
#include "NvTraitsInternal.h"
#endif

namespace nvidia
{
namespace parameterized
{

#if PX_VC
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#endif

namespace ClothingGraphicalLodParameters_0p3NS
{

struct PhysicsSubmeshPartitioning_Type;
struct SkinClothMapB_Type;
struct SkinClothMapC_Type;
struct TetraLink_Type;

struct STRING_DynamicArray1D_Type
{
	NvParameterized::DummyStringStruct* buf;
	bool isAllocated;
	int32_t elementSize;
	int32_t arraySizes[1];
};

struct U32_DynamicArray1D_Type
{
	uint32_t* buf;
	bool isAllocated;
	int32_t elementSize;
	int32_t arraySizes[1];
};

struct SkinClothMapB_DynamicArray1D_Type
{
	SkinClothMapB_Type* buf;
	bool isAllocated;
	int32_t elementSize;
	int32_t arraySizes[1];
};

struct SkinClothMapC_DynamicArray1D_Type
{
	SkinClothMapC_Type* buf;
	bool isAllocated;
	int32_t elementSize;
	int32_t arraySizes[1];
};

struct TetraLink_DynamicArray1D_Type
{
	TetraLink_Type* buf;
	bool isAllocated;
	int32_t elementSize;
	int32_t arraySizes[1];
};

struct PhysicsSubmeshPartitioning_DynamicArray1D_Type
{
	PhysicsSubmeshPartitioning_Type* buf;
	bool isAllocated;
	int32_t elementSize;
	int32_t arraySizes[1];
};

struct SkinClothMapC_Type
{
	physx::PxVec3 vertexBary;
	uint32_t faceIndex0;
	physx::PxVec3 normalBary;
	uint32_t vertexIndexPlusOffset;
};
struct PhysicsSubmeshPartitioning_Type
{
	uint32_t graphicalSubmesh;
	uint32_t physicalSubmesh;
	uint32_t numSimulatedVertices;
	uint32_t numSimulatedVerticesAdditional;
	uint32_t numSimulatedIndices;
};
struct SkinClothMapB_Type
{
	physx::PxVec3 vtxTetraBary;
	uint32_t vertexIndexPlusOffset;
	physx::PxVec3 nrmTetraBary;
	uint32_t faceIndex0;
	uint32_t tetraIndex;
	uint32_t submeshIndex;
};
struct TetraLink_Type
{
	physx::PxVec3 vertexBary;
	uint32_t tetraIndex0;
	physx::PxVec3 normalBary;
	uint32_t _dummyForAlignment;
};

struct ParametersStruct
{

	STRING_DynamicArray1D_Type platforms;
	uint32_t lod;
	uint32_t physicalMeshId;
	NvParameterized::Interface* renderMeshAsset;
	void* renderMeshAssetPointer;
	U32_DynamicArray1D_Type immediateClothMap;
	SkinClothMapB_DynamicArray1D_Type skinClothMapB;
	SkinClothMapC_DynamicArray1D_Type skinClothMapC;
	float skinClothMapThickness;
	float skinClothMapOffset;
	TetraLink_DynamicArray1D_Type tetraMap;
	uint32_t renderMeshAssetSorting;
	PhysicsSubmeshPartitioning_DynamicArray1D_Type physicsSubmeshPartitioning;

};

static const uint32_t checksum[] = { 0x41f36544, 0x55c9d3e4, 0xf4daab87, 0xbb7a1957, };

} // namespace ClothingGraphicalLodParameters_0p3NS

#ifndef NV_PARAMETERIZED_ONLY_LAYOUTS
class ClothingGraphicalLodParameters_0p3 : public NvParameterized::NvParameters, public ClothingGraphicalLodParameters_0p3NS::ParametersStruct
{
public:
	ClothingGraphicalLodParameters_0p3(NvParameterized::Traits* traits, void* buf = 0, int32_t* refCount = 0);

	virtual ~ClothingGraphicalLodParameters_0p3();

	virtual void destroy();

	static const char* staticClassName(void)
	{
		return("ClothingGraphicalLodParameters");
	}

	const char* className(void) const
	{
		return(staticClassName());
	}

	static const uint32_t ClassVersion = ((uint32_t)0 << 16) + (uint32_t)3;

	static uint32_t staticVersion(void)
	{
		return ClassVersion;
	}

	uint32_t version(void) const
	{
		return(staticVersion());
	}

	static const uint32_t ClassAlignment = 8;

	static const uint32_t* staticChecksum(uint32_t& bits)
	{
		bits = 8 * sizeof(ClothingGraphicalLodParameters_0p3NS::checksum);
		return ClothingGraphicalLodParameters_0p3NS::checksum;
	}

	static void freeParameterDefinitionTable(NvParameterized::Traits* traits);

	const uint32_t* checksum(uint32_t& bits) const
	{
		return staticChecksum(bits);
	}

	const ClothingGraphicalLodParameters_0p3NS::ParametersStruct& parameters(void) const
	{
		ClothingGraphicalLodParameters_0p3* tmpThis = const_cast<ClothingGraphicalLodParameters_0p3*>(this);
		return *(static_cast<ClothingGraphicalLodParameters_0p3NS::ParametersStruct*>(tmpThis));
	}

	ClothingGraphicalLodParameters_0p3NS::ParametersStruct& parameters(void)
	{
		return *(static_cast<ClothingGraphicalLodParameters_0p3NS::ParametersStruct*>(this));
	}

	virtual NvParameterized::ErrorType getParameterHandle(const char* long_name, NvParameterized::Handle& handle) const;
	virtual NvParameterized::ErrorType getParameterHandle(const char* long_name, NvParameterized::Handle& handle);

	void initDefaults(void);

protected:

	virtual const NvParameterized::DefinitionImpl* getParameterDefinitionTree(void);
	virtual const NvParameterized::DefinitionImpl* getParameterDefinitionTree(void) const;


	virtual void getVarPtr(const NvParameterized::Handle& handle, void*& ptr, size_t& offset) const;

private:

	void buildTree(void);
	void initDynamicArrays(void);
	void initStrings(void);
	void initReferences(void);
	void freeDynamicArrays(void);
	void freeStrings(void);
	void freeReferences(void);

	static bool mBuiltFlag;
	static NvParameterized::MutexType mBuiltFlagMutex;
};

class ClothingGraphicalLodParameters_0p3Factory : public NvParameterized::Factory
{
	static const char* const vptr;

public:

	virtual void freeParameterDefinitionTable(NvParameterized::Traits* traits)
	{
		ClothingGraphicalLodParameters_0p3::freeParameterDefinitionTable(traits);
	}

	virtual NvParameterized::Interface* create(NvParameterized::Traits* paramTraits)
	{
		// placement new on this class using mParameterizedTraits

		void* newPtr = paramTraits->alloc(sizeof(ClothingGraphicalLodParameters_0p3), ClothingGraphicalLodParameters_0p3::ClassAlignment);
		if (!NvParameterized::IsAligned(newPtr, ClothingGraphicalLodParameters_0p3::ClassAlignment))
		{
			NV_PARAM_TRAITS_WARNING(paramTraits, "Unaligned memory allocation for class ClothingGraphicalLodParameters_0p3");
			paramTraits->free(newPtr);
			return 0;
		}

		memset(newPtr, 0, sizeof(ClothingGraphicalLodParameters_0p3)); // always initialize memory allocated to zero for default values
		return NV_PARAM_PLACEMENT_NEW(newPtr, ClothingGraphicalLodParameters_0p3)(paramTraits);
	}

	virtual NvParameterized::Interface* finish(NvParameterized::Traits* paramTraits, void* bufObj, void* bufStart, int32_t* refCount)
	{
		if (!NvParameterized::IsAligned(bufObj, ClothingGraphicalLodParameters_0p3::ClassAlignment)
		        || !NvParameterized::IsAligned(bufStart, ClothingGraphicalLodParameters_0p3::ClassAlignment))
		{
			NV_PARAM_TRAITS_WARNING(paramTraits, "Unaligned memory allocation for class ClothingGraphicalLodParameters_0p3");
			return 0;
		}

		// Init NvParameters-part
		// We used to call empty constructor of ClothingGraphicalLodParameters_0p3 here
		// but it may call default constructors of members and spoil the data
		NV_PARAM_PLACEMENT_NEW(bufObj, NvParameterized::NvParameters)(paramTraits, bufStart, refCount);

		// Init vtable (everything else is already initialized)
		*(const char**)bufObj = vptr;

		return (ClothingGraphicalLodParameters_0p3*)bufObj;
	}

	virtual const char* getClassName()
	{
		return (ClothingGraphicalLodParameters_0p3::staticClassName());
	}

	virtual uint32_t getVersion()
	{
		return (ClothingGraphicalLodParameters_0p3::staticVersion());
	}

	virtual uint32_t getAlignment()
	{
		return (ClothingGraphicalLodParameters_0p3::ClassAlignment);
	}

	virtual const uint32_t* getChecksum(uint32_t& bits)
	{
		return (ClothingGraphicalLodParameters_0p3::staticChecksum(bits));
	}
};
#endif // NV_PARAMETERIZED_ONLY_LAYOUTS

} // namespace parameterized
} // namespace nvidia

#if PX_VC
#pragma warning(pop)
#endif

#endif
