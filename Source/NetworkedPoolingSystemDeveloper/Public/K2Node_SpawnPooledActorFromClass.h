// Copyright JOSEUEM, 2024

#pragma once

#include "CoreMinimal.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_SpawnPooledActorFromClass.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FString;
class UClass;
class UEdGraph;
class UEdGraphPin;
class UObject;
struct FLinearColor;
template <typename KeyType, typename ValueType> struct TKeyValuePair;

/**
 * 
 */
UCLASS()
class NETWORKEDPOOLINGSYSTEMDEVELOPER_API UK2Node_SpawnPooledActorFromClass : public UK2Node_ConstructObjectFromClass
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void PostPlacedNewNode() override;
	//~ End UEdGraphNode Interface.

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	//~ End UObject Interface
	
	//~ Begin UK2Node Interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	//~ End UK2Node Interface

	//~ Begin UK2Node_ConstructObjectFromClass Interface
	virtual UClass* GetClassPinBaseClass() const;
	virtual bool IsSpawnVarPin(UEdGraphPin* Pin) const override;
	//~ End UK2Node_ConstructObjectFromClass Interface

	virtual FText GetMenuCategory() const override;
private:
	void FixupScaleMethodPin();
	
	/** Get the spawn transform input pin */	
	UEdGraphPin* GetSpawnTransformPin() const;
	/** Get the collision handling method input pin */
	UEdGraphPin* GetCollisionHandlingOverridePin() const;
	/** Get the collision handling method input pin */
	UEdGraphPin* GetScaleMethodPin() const;
	UEdGraphPin* TryGetScaleMethodPin() const;
	/** Get the actor owner pin */
	UEdGraphPin* GetOwnerPin() const;

	void MaybeUpdateCollisionPin(TArray<UEdGraphPin*>& OldPins);
};