// Copyright JOSEUEM, 2024

#pragma once

#include "CoreMinimal.h"
#include "BasePool.h"
#include "PoolInterface.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "PoolSubsystem.generated.h"

class ABasePool;
/**
 * 
 */
UCLASS()
class NETWORKEDPOOLINGSYSTEM_API UPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	
	template<class T>
	T* RequestPoolObject(TSubclassOf<UObject> Class, AActor* Owner, bool bDeferred = false);

	template<class T>
	T* FinishSpawningPoolObject(UObject* Target, const FTransform& Transform = FTransform::Identity);

	/*Checks wether or not this actor is currently being used, or waiting on the pool*/
	UFUNCTION(BlueprintPure, Category="Object Pooling")
	static bool IsPooledObjectActive(UObject* Object);
	
	UFUNCTION(BlueprintCallable, Category="Object Pooling")
	static void ReturnToPool(UObject* TargetObject);
	
	// ===== Exclusive use for K2 spawn node ===== 
	UFUNCTION(BlueprintCallable, Category = "Pool", meta=(WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true", BlueprintInternalUseOnly = "true", DeterminesOutputType = "ActorClass"))
	static AActor* K2_BeginSpawningPoolActor(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, AActor* Owner = nullptr, ESpawnActorScaleMethod TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale);

	UFUNCTION(BlueprintCallable, Category = "Pool", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction = "true", BlueprintInternalUseOnly = "true"))
	static AActor* K2_FinishSpawningPoolActor(const UObject* WorldContextObject, AActor* Actor, const FTransform& SpawnTransform, ESpawnActorScaleMethod TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale);

	UFUNCTION(BlueprintCallable, Category = "Pool", meta=(WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true", BlueprintInternalUseOnly = "true", DeterminesOutputType = "ObjectClass"))
	static UObject* K2_BeginSpawnPoolObject(const UObject* WorldContextObject, TSubclassOf<UObject> ObjectClass);

	UFUNCTION(BlueprintCallable, Category = "Pool", meta=(WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true", BlueprintInternalUseOnly = "true", DeterminesOutputType = "ObjectClass"))
	static UObject* K2_FinishSpawnPoolObject(const UObject* WorldContextObject, UObject* Object);
	// ==========================================

	void RegisterPool(ABasePool* Pool);
private:
	ABasePool* FindClassInPool(TSubclassOf<UObject> Class, TArray<ABasePool*>& PoolToUse);
	ABasePool* FindPool(UClass* Class);
	ABasePool* FindPool(UObject* Target);
	void InitializePools();

	static void SetActorTransform(const FTransform& SpawnTransform, ESpawnActorScaleMethod TransformScaleMethod, AActor* SpawnedActor);
private:
	UPROPERTY()
	TArray<ABasePool*> AuthPools;

	UPROPERTY()
	TArray<ABasePool*> ClientPools;
};

template <class T>
T* UPoolSubsystem::RequestPoolObject(TSubclassOf<UObject> Class, AActor* Owner, bool bDeferred)
{
	if (ABasePool* Pool = FindPool(Class))
	{
		UObject* SpawnedPoolObject = Pool->PreSpawnPoolObject(Class, Owner);
		bool bImplementsInterface = SpawnedPoolObject && SpawnedPoolObject->GetClass()->ImplementsInterface(UPoolInterface::StaticClass());
		
		if (bImplementsInterface)
		{
			IPoolInterface::Execute_OnPoolObjectContruct(SpawnedPoolObject);
		}
		
		if (!bDeferred)
		{
			Pool->FinishSpawningPoolObject(SpawnedPoolObject, FTransform::Identity);
			if (bImplementsInterface)
			{
				IPoolInterface::Execute_OnPoolObjectActivate(SpawnedPoolObject);
			}
		}
		
		return Cast<T>(SpawnedPoolObject);
	}

	UE_LOG(LogPoolSubsystem, Error, TEXT("Failed to get pool object %s"), *GetNameSafe(Class));

	return nullptr;
}

template <class T>
T* UPoolSubsystem::FinishSpawningPoolObject(UObject* Target, const FTransform& Transform)
{
	if (!ensure(Target))
	{
		return nullptr;
	}
	
	if (ABasePool* Pool = FindPool(Target->GetClass()))
	{
		Pool->FinishSpawningPoolObject(Target, Transform);

		bool bImplementsInterface = Target->GetClass()->ImplementsInterface(UPoolInterface::StaticClass());
		if (bImplementsInterface)
		{
			IPoolInterface::Execute_OnPoolObjectActivate(Target);
		}
		
		return Cast<T>(Target);
	}

	return nullptr;
}
