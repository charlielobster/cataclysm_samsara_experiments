// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ActorComponentDetails.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Engine/EngineBaseTypes.h"
#include "Components/ActorComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "ObjectEditorUtils.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "ActorComponentDetails"

TSharedRef<IDetailCustomization> FActorComponentDetails::MakeInstance()
{
	return MakeShareable( new FActorComponentDetails );
}

void FActorComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	AddExperimentalWarningCategory(DetailBuilder);

	TSharedPtr<IPropertyHandle> PrimaryTickProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UActorComponent, PrimaryComponentTick));

	// Defaults only show tick properties
	if (PrimaryTickProperty->IsValidHandle() && DetailBuilder.GetDetailsView().HasClassDefaultObject())
	{
		IDetailCategoryBuilder& TickCategory = DetailBuilder.EditCategory("ComponentTick");

		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bStartWithTickEnabled)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickInterval)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bTickEvenWhenPaused)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bAllowTickOnDedicatedServer)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickGroup)), EPropertyLocation::Advanced);
	}

	PrimaryTickProperty->MarkHiddenByCustomization();

	TArray<TWeakObjectPtr<UObject>> WeakObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(WeakObjectsBeingCustomized);

	bool bHideReplicates = false;
	for (TWeakObjectPtr<UObject>& WeakObjectBeingCustomized : WeakObjectsBeingCustomized)
	{
		if (UObject* ObjectBeingCustomized = WeakObjectBeingCustomized.Get())
		{
			if (UActorComponent* Component = Cast<UActorComponent>(ObjectBeingCustomized))
			{
				if (!Component->GetComponentClassCanReplicate())
				{
					bHideReplicates = true;
					break;
				}
			}
			else
			{
				bHideReplicates = true;
				break;
			}
		}
	}

	if (bHideReplicates)
	{
		TSharedPtr<IPropertyHandle> ReplicatesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UActorComponent, bReplicates));
		ReplicatesProperty->MarkHiddenByCustomization();
	}
}

void FActorComponentDetails::AddExperimentalWarningCategory( IDetailLayoutBuilder& DetailBuilder )
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailBuilder.GetDetailsView().GetSelectedObjects();

	bool bIsExperimental = false;
	bool bIsEarlyAccess = false;

	for (const TWeakObjectPtr<UObject>& SelectedObjectPtr : SelectedObjects)
	{
		if (UObject* SelectedObject = SelectedObjectPtr.Get())
		{
			bool bObjectClassIsExperimental, bObjectClassIsEarlyAccess;
			FObjectEditorUtils::GetClassDevelopmentStatus(SelectedObject->GetClass(), bObjectClassIsExperimental, bObjectClassIsEarlyAccess);
			bIsExperimental |= bObjectClassIsExperimental;
			bIsEarlyAccess |= bObjectClassIsEarlyAccess;
		}
	}


	if (bIsExperimental || bIsEarlyAccess)
	{
		const FName CategoryName(TEXT("Warning"));
		const FText CategoryDisplayName = LOCTEXT("WarningCategoryDisplayName", "Warning");
		FString ClassUsed = DetailBuilder.GetTopLevelProperty().ToString();
		const FText WarningText = bIsExperimental ? FText::Format(LOCTEXT("ExperimentalClassWarning", "Uses experimental class: {0}"), FText::FromString(ClassUsed))
			: FText::Format(LOCTEXT("EarlyAccessClassWarning", "Uses early access class: {0}"), FText::FromString(ClassUsed));
		const FText SearchString = WarningText;
		const FText Tooltip = bIsExperimental ? (LOCTEXT("ExperimentalClassTooltip", "Here be dragons!  Uses one or more unsupported 'experimental' classes")) : (LOCTEXT("EarlyAccessClassTooltip", "Uses one or more 'early access' classes"));
		const FString ExcerptName = bIsExperimental ? TEXT("ComponentUsesExperimentalClass") : TEXT("ComponentUsesEarlyAccessClass");
		const FSlateBrush* WarningIcon = FEditorStyle::GetBrush(bIsExperimental ? "PropertyEditor.ExperimentalClass" : "PropertyEditor.EarlyAccessClass");

		IDetailCategoryBuilder& WarningCategory = DetailBuilder.EditCategory(CategoryName, CategoryDisplayName, ECategoryPriority::Transform);

		FDetailWidgetRow& WarningRow = WarningCategory.AddCustomRow(SearchString)
			.WholeRowContent()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("SettingsEditor.CheckoutWarningBorder"))
				.BorderBackgroundColor(FColor (166, 137, 0))
				[
					SNew(SHorizontalBox)
					.ToolTip(IDocumentation::Get()->CreateToolTip(Tooltip, nullptr, TEXT("Shared/LevelEditor"), ExcerptName))
					.Visibility(EVisibility::Visible)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SImage)
						.Image(WarningIcon)
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(WarningText)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			];
	}
}

#undef LOCTEXT_NAMESPACE
