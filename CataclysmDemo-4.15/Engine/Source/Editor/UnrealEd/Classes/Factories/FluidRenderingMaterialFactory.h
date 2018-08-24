// CATACLYSM
#pragma once
#include "FluidRenderingMaterialFactory.generated.h"

UCLASS(hidecategories = Object)
class UFluidRenderingMaterialFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// Begin UFactory Interface	
};
