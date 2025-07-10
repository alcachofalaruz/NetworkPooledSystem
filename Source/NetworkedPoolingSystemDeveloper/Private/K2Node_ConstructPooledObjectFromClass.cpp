// Copyright JOSEUEM, 2024

#include "K2Node_ConstructPooledObjectFromClass.h"


#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "Containers/EnumAsByte.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EditorCategoryUtils.h"
#include "Engine/Blueprint.h"
#include "FindInBlueprintManager.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "PoolSubsystem.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "FindInBlueprintManager.h"
#include "K2Node_Self.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"


struct FK2Node_GenericCreateObject_Utils
{
	static bool CanSpawnObjectOfClass(TSubclassOf<UObject> ObjectClass, bool bAllowAbstract)
	{
		// Initially include types that meet the basic requirements.
		// Note: CLASS_Deprecated is an inherited class flag, so any subclass of an explicitly-deprecated class also cannot be spawned.
		bool bCanSpawnObject = (nullptr != *ObjectClass)
			&& (bAllowAbstract || !ObjectClass->HasAnyClassFlags(CLASS_Abstract))
			&& !ObjectClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists);

		// UObject is a special case where if we are allowing abstract we are going to allow it through even though it doesn't have BlueprintType on it
		if (bCanSpawnObject && (!bAllowAbstract || (*ObjectClass != UObject::StaticClass())))
		{
			static const FName BlueprintTypeName(TEXT("BlueprintType"));
			static const FName NotBlueprintTypeName(TEXT("NotBlueprintType"));
			static const FName DontUseGenericSpawnObjectName(TEXT("DontUseGenericSpawnObject"));

			auto IsClassAllowedLambda = [](const UClass* InClass)
			{
				return InClass != AActor::StaticClass()
					&& InClass != UActorComponent::StaticClass();
			};

			// Exclude all types in the initial set by default.
			bCanSpawnObject = false;
			const UClass* CurrentClass = ObjectClass;

			// Climb up the class hierarchy and look for "BlueprintType." If "NotBlueprintType" is seen first, or if the class is not allowed, then stop searching.
			while (!bCanSpawnObject && CurrentClass != nullptr && !CurrentClass->GetBoolMetaData(NotBlueprintTypeName) && IsClassAllowedLambda(CurrentClass))
			{
				// Include any type that either includes or inherits 'BlueprintType'
				bCanSpawnObject = CurrentClass->GetBoolMetaData(BlueprintTypeName);

				// Stop searching if we encounter 'BlueprintType' with 'DontUseGenericSpawnObject'
				if (bCanSpawnObject && CurrentClass->GetBoolMetaData(DontUseGenericSpawnObjectName))
				{
					bCanSpawnObject = false;
					break;
				}

				CurrentClass = CurrentClass->GetSuperClass();
			}

			// If we validated the given class, continue walking up the hierarchy to make sure we exclude it if it's an Actor or ActorComponent derivative.
			while (bCanSpawnObject && CurrentClass != nullptr)
			{
				bCanSpawnObject &= IsClassAllowedLambda(CurrentClass);

				CurrentClass = CurrentClass->GetSuperClass();
			}
		}

		return bCanSpawnObject;
	}
};

struct FK2Node_ConstructObjectFromClassHelper
{
	static FName GetWorldContextPinName()
	{
		return TEXT("WorldContextObject");
	}

	static FName GetClassPinName()
	{
		return TEXT("Class");
	}
};

#define LOCTEXT_NAMESPACE "K2Node_ConstructObjectFromClass"

UK2Node_ConstructPooledObjectFromClass::UK2Node_ConstructPooledObjectFromClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeTooltip = LOCTEXT("NodeTooltip", "Attempts to spawn a pooled object");
}

void UK2Node_ConstructPooledObjectFromClass::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* CallCreateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallCreateNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UPoolSubsystem, K2_BeginSpawnPoolObject), UPoolSubsystem::StaticClass());
	CallCreateNode->AllocateDefaultPins();
	static const FName WorldContextObject_ParamName(TEXT("WorldContextObject"));

	// connect GenericCreateObject's self pin to the self node
	bool bSucceeded = ExpandDefaultToSelfPin(CompilerContext, SourceGraph, CallCreateNode);

	// store off the class to spawn before we mutate pin connections:
	UClass* ClassToSpawn = GetClassToSpawn();

	UEdGraphPin* SpawnWorldContextPin = GetWorldContextPin();

	//connect world context
	if (UseWorldContext())
	{
		UEdGraphPin* CallCreateWorldContextPin = CallCreateNode->FindPinChecked(WorldContextObject_ParamName);
		bSucceeded &= SpawnWorldContextPin && CallCreateNode && CompilerContext.MovePinLinksToIntermediate(*SpawnWorldContextPin, *CallCreateWorldContextPin).CanSafeConnect();

	}
	
	//connect exe
	{
		UEdGraphPin* SpawnExecPin = GetExecPin();
		UEdGraphPin* CallExecPin = CallCreateNode->GetExecPin();
		bSucceeded &= SpawnExecPin && CallExecPin && CompilerContext.MovePinLinksToIntermediate(*SpawnExecPin, *CallExecPin).CanSafeConnect();
	}

	//connect class
	{
		UEdGraphPin* SpawnClassPin = GetClassPin();
		UEdGraphPin* CallClassPin = CallCreateNode->FindPin(TEXT("ObjectClass"));
		bSucceeded &= SpawnClassPin && CallClassPin && CompilerContext.MovePinLinksToIntermediate(*SpawnClassPin, *CallClassPin).CanSafeConnect();
	}

	UEdGraphPin* CallResultPin = nullptr;
	//connect result
	{
		UEdGraphPin* SpawnResultPin = GetResultPin();
		CallResultPin = CallCreateNode->GetReturnValuePin();

		// cast HACK. It should be safe. The only problem is native code generation.
		if (SpawnResultPin && CallResultPin)
		{
			CallResultPin->PinType = SpawnResultPin->PinType;
		}
		bSucceeded &= SpawnResultPin && CallResultPin && CompilerContext.MovePinLinksToIntermediate(*SpawnResultPin, *CallResultPin).CanSafeConnect();
	}

	//////////////////////////////////////////////////////////////////////////
	// create 'finish spawn' call node
	UK2Node_CallFunction* CallFinishSpawnNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallFinishSpawnNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UPoolSubsystem, K2_FinishSpawnPoolObject), UPoolSubsystem::StaticClass());
	CallFinishSpawnNode->AllocateDefaultPins();

	UEdGraphPin* CallFinishExec = CallFinishSpawnNode->GetExecPin();
	UEdGraphPin* CallFinishThen = CallFinishSpawnNode->GetThenPin();
	UEdGraphPin* CallFinishActor = CallFinishSpawnNode->FindPinChecked(TEXT("Object"));
	UEdGraphPin* CallFinishContextPin = CallFinishSpawnNode->FindPinChecked(TEXT("WorldContextObject"));
	UEdGraphPin* CallFinishResult = CallFinishSpawnNode->GetReturnValuePin();

	// Move 'then' connection from spawn node to 'finish spawn'
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *CallFinishThen);

	// Copy the world context connection from the spawn node to 'begin spawn' if necessary
	if (SpawnWorldContextPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*SpawnWorldContextPin, *CallFinishContextPin);
	}

	// Connect output actor from 'begin' to 'finish'
	CallCreateNode->GetReturnValuePin()->MakeLinkTo(CallFinishActor);
	CallFinishResult->PinType = GetResultPin()->PinType;
	CompilerContext.MovePinLinksToIntermediate(*GetResultPin(), *CallFinishResult);
	//assign exposed values and connect then
	{
		UEdGraphPin* LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes(CompilerContext, SourceGraph, CallCreateNode, this, CallResultPin, ClassToSpawn);
		UEdGraphPin* SpawnNodeThen = GetThenPin();
		// Make exec connection between 'then' on last node and 'finish'
		LastThen->MakeLinkTo(CallFinishExec);
	}

	BreakAllNodeLinks();

	if (!bSucceeded)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("GenericCreateObject_Error", "ICE: GenericCreateObject error @@").ToString(), this);
	}
}

void UK2Node_ConstructPooledObjectFromClass::EarlyValidation(FCompilerResultsLog& MessageLog) const
{
	Super::EarlyValidation(MessageLog);
	UEdGraphPin* ClassPin = GetClassPin(&Pins);
	const bool bAllowAbstract = ClassPin && ClassPin->LinkedTo.Num();
	UClass* ClassToSpawn = GetClassToSpawn();
	if (!FK2Node_GenericCreateObject_Utils::CanSpawnObjectOfClass(ClassToSpawn, bAllowAbstract))
	{
		MessageLog.Error(*FText::Format(LOCTEXT("GenericCreateObject_WrongClassFmt", "Cannot construct objects of type '{0}' in @@"), FText::FromString(GetPathNameSafe(ClassToSpawn))).ToString(), this);
	}
}

bool UK2Node_ConstructPooledObjectFromClass::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	return Super::IsCompatibleWithGraph(TargetGraph) && (!Blueprint || (FBlueprintEditorUtils::FindUserConstructionScript(Blueprint) != TargetGraph && Blueprint->GeneratedClass->GetDefaultObject()->ImplementsGetWorld()));
}

void UK2Node_ConstructPooledObjectFromClass::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_ConstructPooledObjectFromClass::GetMenuCategory() const
{
	return FText::FromString(TEXT("Pooling"));
}

bool UK2Node_ConstructPooledObjectFromClass::ExpandDefaultToSelfPin(FKismetCompilerContext& CompilerContext,
                                                                    UEdGraph* SourceGraph, UK2Node_CallFunction* CallCreateNode)
{
	// ensure valid input
	if (!SourceGraph || !CallCreateNode)
	{
		return false;
	}

	// Connect a self reference pin if there is a TScriptInterface default to self
	if (const UFunction* TargetFunc = CallCreateNode->GetTargetFunction())
	{
		const FString& MetaData = TargetFunc->GetMetaData(FBlueprintMetadata::MD_DefaultToSelf);
		if (!MetaData.IsEmpty())
		{
			// Get the Self Pin from this so we can default it correctly
			const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
			if (UEdGraphPin* DefaultToSelfPin = Schema->FindSelfPin(*this, EGPD_Input))
			{
				// If it has no links then spawn a new self node here
				if (DefaultToSelfPin->LinkedTo.IsEmpty())
				{
					UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
					SelfNode->AllocateDefaultPins();
					UEdGraphPin* SelfPin = SelfNode->FindPinChecked(UEdGraphSchema_K2::PSC_Self);
					// Make a connection from this intermediate self pin to here
					return Schema->TryCreateConnection(DefaultToSelfPin, SelfPin);
				}
			}
		}
	}
	return true;
}

void UK2Node_ConstructPooledObjectFromClass::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	if (UEdGraphPin* ClassPin = GetClassPin())
	{
		SetPinToolTip(*ClassPin, LOCTEXT("ClassPinDescription", "The object class you want to construct"));
	}
	if (UEdGraphPin* ResultPin = GetResultPin())
	{
		SetPinToolTip(*ResultPin, LOCTEXT("ResultPinDescription", "The constructed object"));
	}
	if (UEdGraphPin* OuterPin = (UseOuter() ? GetOuterPin() : nullptr))
	{
		SetPinToolTip(*OuterPin, LOCTEXT("OuterPinDescription", "Owner of the constructed object"));
	}

	return Super::GetPinHoverText(Pin, HoverTextOut);
}

FText UK2Node_ConstructPooledObjectFromClass::GetBaseNodeTitle() const
{
	return FText::FromString("Construct Pooled Object from Class");
}

FText UK2Node_ConstructPooledObjectFromClass::GetDefaultNodeTitle() const
{
	return FText::FromString("Construct Pooled NONE");
}

FText UK2Node_ConstructPooledObjectFromClass::GetNodeTitleFormat() const
{
	return FText::FromString("Construct Pooled {ClassName}");
}

FText UK2Node_ConstructPooledObjectFromClass::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		return GetBaseNodeTitle();
	}
	else if (UClass* ClassToSpawn = GetClassToSpawn())
	{
		if (CachedNodeTitle.IsOutOfDate(this))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ClassName"), ClassToSpawn->GetDisplayNameText());
			// FText::Format() is slow, so we cache this to save on performance
			CachedNodeTitle.SetCachedText(FText::Format(GetNodeTitleFormat(), Args), this);
		}
		return CachedNodeTitle;
	}
	return GetDefaultNodeTitle();
}

FNodeHandlingFunctor* UK2Node_ConstructPooledObjectFromClass::CreateNodeHandler(
	FKismetCompilerContext& CompilerContext) const
{
	return new FNodeHandlingFunctor(CompilerContext);
}

void UK2Node_ConstructPooledObjectFromClass::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
}

#undef LOCTEXT_NAMESPACE

