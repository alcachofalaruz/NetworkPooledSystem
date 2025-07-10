// Copyright JOSEUEM, 2024

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "HAL/PlatformMath.h"
#include "Internationalization/Text.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "K2Node_ConstructPooledObjectFromClass.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UClass;
class UEdGraph;
class UEdGraphPin;
class UK2Node_CallFunction;
class UObject;
template <typename KeyType, typename ValueType> struct TKeyValuePair;
/**
 * 
 */
UCLASS()
class NETWORKEDPOOLINGSYSTEMDEVELOPER_API UK2Node_ConstructPooledObjectFromClass : public UK2Node_ConstructObjectFromClass
{
	GENERATED_UCLASS_BODY()

	FText GetBaseNodeTitle() const override;
	FText GetDefaultNodeTitle() const override;
	FText GetNodeTitleFormat() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	
	virtual bool UseOuter() const override { return false; }

	/**
	 * attaches a self node to the self pin of 'this' if the CallCreateNode function has DefaultToSelf in it's metadata
	 *
	 * @param	CompilerContext		the context to expand in - likely passed from ExpandNode
	 * @param	SourceGraph			the graph to expand in - likely passed from ExpandNode
	 * @param	CallCreateNode		the CallFunction node that 'this' is imitating
	 *
	 * @return	true on success.
	 */
	bool ExpandDefaultToSelfPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* CallCreateNode);
};