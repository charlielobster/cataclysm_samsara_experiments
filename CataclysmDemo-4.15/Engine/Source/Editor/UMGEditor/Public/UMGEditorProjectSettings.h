// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/StringClassReference.h"
#include "UMGEditorProjectSettings.generated.h"

/**
 * Implements the settings for the UMG Editor Project Settings
 */
UCLASS(config=Engine, defaultconfig)
class UMGEDITOR_API UUMGEditorProjectSettings : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, config, Category = ShowHideWidgetClasses)
	bool bShowWidgetsFromEngineContent;

	UPROPERTY(EditAnywhere, config, Category = ShowHideWidgetClasses)
	bool bShowWidgetsFromDeveloperContent;

	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = ShowHideWidgetClasses)
	TArray<FString> CategoriesToHide;

	UPROPERTY(EditAnywhere, config, Category = ShowHideWidgetClasses, meta = (MetaClass = "Widget"))
	TArray<FStringClassReference> WidgetClassesToHide;
#endif
};
