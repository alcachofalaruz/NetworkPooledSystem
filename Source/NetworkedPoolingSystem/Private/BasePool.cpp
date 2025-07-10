// Copyright JOSEUEM, 2024


#include "BasePool.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "GeneralProjectSettings.h"
#include "PoolInterface.h"
#include "PoolSubsystem.h"
#include "PoolSystemSettings.h"
#include "Engine/AssetManager.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

ABasePool::ABasePool(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	NetPriority = 5.0f;
	SetReplicatingMovement(false);
}

void ABasePool::BeginPlay()
{
	Super::BeginPlay();
	
	PoolObjects.SetOwningPool(this);
}

void ABasePool::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}
}

void ABasePool::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, PoolObjects);
}

UObject* ABasePool::PreSpawnPoolObject(TSubclassOf<UObject> InClass, AActor* InOwner)
{
	return nullptr;
}

void ABasePool::FinishSpawningPoolObject(UObject* Target, const FTransform& Transform)
{
	UE_LOG(LogPoolSubsystem, Verbose, TEXT("finished spawning pool object %s"), *GetNameSafe(Target));

	BP_OnFinishSpawningPoolObject(Target, Transform);
}

void ABasePool::ResetToDefaultValues(UObject* Object)
{
	const UPoolSystemSettings& PoolSystemSettings = *GetDefault<UPoolSystemSettings>();
	if (!PoolSystemSettings.bUseAutomaticPropertyReset)
	{
		return;
	}
	
	UObject* DefaultObject = Object->GetClass()->GetDefaultObject();
	bool bImplementsInterface = Object->GetClass()->ImplementsInterface(UPoolInterface::StaticClass());
	static TArray<FString> EmptyArray;
	const TArray<FString>& PropertyExcludeList = bImplementsInterface ? IPoolInterface::Execute_GetPropertyResetExcludeList(Object) : EmptyArray;
	
	for (TFieldIterator<FProperty> PropIt(DefaultObject->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		
		// check if trivially copyable and if it is from our project
		// we do not want to be resetting variables from base engine classes to avoid unwanted behaviours
		if (PropertyExcludeList.Contains(Property->GetName()) || !IsClassFromProject(Property->GetOwnerClass()) || !CanResetProperty(Property))
		{
			continue;
		}
		
		// Get the property value from the default object
		void* DefaultValue = Property->ContainerPtrToValuePtr<void>(DefaultObject);

		// Set the property value to the default value
		void* ObjectValue = Property->ContainerPtrToValuePtr<void>(Object);
		Property->CopyCompleteValue(ObjectValue, DefaultValue);
	}
}

void ABasePool::ReturnToPool(UObject* Object)
{
	check(Object);

	UE_LOG(LogPoolSubsystem, Verbose, TEXT("Returning object to pool %s"), *GetNameSafe(Object));
	
	GetWorld()->GetTimerManager().ClearAllTimersForObject(Object);
	FLatentActionManager& LatentActionManager = GetWorld()->GetLatentActionManager();
	LatentActionManager.RemoveActionsForObject(Object);
	ResetToDefaultValues(Object);

	PoolObjects.Add(Object, true);
	ForceNetUpdate();
}

void ABasePool::PreAllocateObjects(TArray<TSoftClassPtr<UObject>> PreAllocastionClasses, int32 PreAllocationNumber)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogPoolSubsystem, Log, TEXT("Pre allocating objects for pool %s"), *GetNameSafe(this));
	
	auto AllocateObjects = [this, PreAllocationNumber](TSubclassOf<UObject> Class)
	{
		UPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UPoolSubsystem>();
		for (int i = 0; i < PreAllocationNumber; ++i)
		{
			UObject* SpawnedObject = PoolSubsystem->RequestPoolObject<UObject>(Class, this, false);
			PoolSubsystem->ReturnToPool(SpawnedObject);
		}	
	};

	if (PreAllocastionClasses.IsEmpty())
	{
		AllocateObjects(TargetClass);
		return;
	}

	for (const TSoftClassPtr<UObject>& Class : PreAllocastionClasses)
	{
		AllocateObjects(Class.LoadSynchronous());
	}
}

TArray<FString> GetGameFeatureNames()
{
	TArray<FString> FeatureNames;
	FString GameFeaturesDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("GameFeatures"));

	IFileManager& FileManager = IFileManager::Get();
	FileManager.FindFilesRecursive(FeatureNames, *GameFeaturesDir, TEXT("*"), false, true);

	// Trim the full path to just the directory names
	for (FString& FeatureName : FeatureNames)
	{
		FeatureName = FPaths::GetCleanFilename(FeatureName);
	}

	return FeatureNames;
}

bool ABasePool::IsObjectFree(UObject* InObject)
{
	FPoolObjectItem& ItemData = PoolObjects.Find(InObject);
	return ItemData.bIsFree;
}

bool ABasePool::CanResetProperty(FProperty* Property) const
{
	if (Property->PropertyFlags & CPF_Transient)
	{
		return false;
	}

	// Check if the property type can be easily reset
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FProperty* InnerProperty = ArrayProperty->Inner;
		
		return CanResetProperty(InnerProperty);
	}

	// only reset trivially copyable variables, if you want more types to be reset this would be the place, beware it could cause side effects
	return Property->IsA<FBoolProperty>() ||
			Property->IsA<FIntProperty>() ||
			Property->IsA<FFloatProperty>() ||
			Property->IsA<FStructProperty>() && (Property->PropertyFlags & CPF_IsPlainOldData) && !(Property->PropertyFlags & CPF_Transient);
}

// Helper function to initialize the Game Feature Names set
TSet<FString> InitializeGameFeatureNames()
{
	TSet<FString> FeatureNames;
	const TArray<FString> GameFeatureNames = GetGameFeatureNames(); // Assume this function retrieves all feature names

	for (const FString& FeatureName : GameFeatureNames)
	{
		FeatureNames.Add(FeatureName);
	}

	return FeatureNames;
}

// Function to extract the feature name from the package name
FString ExtractFeatureNameFromPackage(const FString& PackageName)
{
	FString FeatureName;
	FString Left, Right;

	// Split the package name by the first slash to get the feature name section
	if (PackageName.Split(TEXT("/Game/"), &Left, &Right))
	{
		// Extract the feature name from the part before "/Game/"
		if (Left.Split(TEXT("/"), nullptr, &FeatureName))
		{
			return FeatureName;
		}
	}

	// Return empty if extraction fails
	return FString();
}

bool ABasePool::IsClassFromProject(UClass* Class)
{
	if (!Class)
	{
		return false;
	}

	FString PackageName = Class->GetPackage()->GetName();
	FSoftObjectPath SoftObjectPath(PackageName);

	UAssetManager& AssetManager = UAssetManager::Get();
	const IAssetRegistry& AssetRegistry = AssetManager.GetAssetRegistry();

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftObjectPath, /*bIncludeOnlyOnDiskAssets=*/false, /*bSkipARFilteredAssets=*/true);

	if (!AssetData.IsValid())
	{
		return false;
	}

	// Initialize Game Feature Names only once
	static const TSet<FString> GameFeatureNameSet = InitializeGameFeatureNames();

	FString ExtractedFeatureName = ExtractFeatureNameFromPackage(PackageName);

	// Check if the extracted feature name is in the set of known game feature names
	if (GameFeatureNameSet.Contains(ExtractedFeatureName))
	{
		return true;
	}

	// Check if it's part of the main game directory or matches script convention
	return PackageName.StartsWith(TEXT("/Game/")) || 
		   PackageName.StartsWith(FString::Printf(TEXT("/Script/%s"), FApp::GetProjectName()));
}
