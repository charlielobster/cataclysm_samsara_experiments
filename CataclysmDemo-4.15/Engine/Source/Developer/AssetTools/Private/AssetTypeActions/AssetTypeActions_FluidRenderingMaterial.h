// CATACLYSM
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AssetTypeActions_Base.h"
#include "Fluid/FluidRenderingMaterial.h"

class FAssetTypeActions_FluidRenderingMaterial : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_FluidRenderingMaterial", "Fluid Rendering Material"); }
	virtual FColor GetTypeColor() const override { return FColor(64, 192, 64); }
	virtual UClass* GetSupportedClass() const override { return UFluidRenderingMaterial::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }
};
