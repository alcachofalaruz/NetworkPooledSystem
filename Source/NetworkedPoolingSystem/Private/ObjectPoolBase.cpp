// Copyright JOSEUEM, 2024


#include "ObjectPoolBase.h"

AObjectPoolBase::AObjectPoolBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TargetClass = UObject::StaticClass();
}

UObject* AObjectPoolBase::PreSpawnPoolObject(TSubclassOf<UObject> InClass, AActor* InOwner)
{
	if (UObject* PoolObject = FindInPool<UObject>(InClass))
	{
		UE_LOG(LogPoolSubsystem, Verbose, TEXT("Reusing pool object %s"), *GetNameSafe(PoolObject));

		return PoolObject;
	}

	// Ignore owner since we are an object
	UObject* SpawnedObject = NewObject<UObject>(this, InClass);
	UE_LOG(LogPoolSubsystem, Verbose, TEXT("Spawning new pool object %s"), *GetNameSafe(SpawnedObject));
	PoolObjects.Add(SpawnedObject, false);
	ForceNetUpdate();
	BP_OnPreSpawnPoolObject(SpawnedObject);
	return SpawnedObject;
}
