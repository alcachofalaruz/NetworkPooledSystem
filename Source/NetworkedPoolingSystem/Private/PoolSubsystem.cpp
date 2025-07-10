// Copyright JOSEUEM, 2024

#include "PoolSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "ActorPoolBase.h"
#include "BasePool.h"
#include "ObjectPoolBase.h"
#include "PoolSystemSettings.h"

void UPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (AGameStateBase* GameState = InWorld.GetGameState())
	{
		InitializePools();
	}
	else
	{
		InWorld.GameStateSetEvent.AddWeakLambda(this, [this](AGameStateBase* GameState)
		{
			InitializePools();
		});
	}
}

bool UPoolSubsystem::IsPooledObjectActive(UObject* Object)
{
	if (!Object)
	{
		return false;
	}
	
	if (UPoolSubsystem* PoolSubsystem = Object->GetWorld()->GetSubsystem<UPoolSubsystem>())
	{
		ABasePool* Pool = PoolSubsystem->FindPool(Object);
		if (Pool)
		{
			return !Pool->IsObjectFree(Object);
		}
	}
	
	return false;
}

void UPoolSubsystem::ReturnToPool(UObject* TargetObject)
{
	if (!ensure(TargetObject))
	{
		return;
	}
	
	if (UPoolSubsystem* PoolSubsystem = TargetObject->GetWorld()->GetSubsystem<UPoolSubsystem>())
	{
		if (ABasePool* Pool = PoolSubsystem->FindPool(TargetObject))
		{
			bool bImplementsInterface = TargetObject->GetClass()->ImplementsInterface(UPoolInterface::StaticClass());
		
			if (bImplementsInterface)
			{
				IPoolInterface::Execute_OnPoolObjectDeactivate(TargetObject);
			}
			
			Pool->ReturnToPool(TargetObject);
		}
		else
		{
			UE_LOG(LogPoolSubsystem, Error, TEXT("Tried to return object to pool but did not find owning pool %s"), *GetNameSafe(TargetObject));
		}
	}
}

void UPoolSubsystem::InitializePools()
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const UPoolSystemSettings& PoolSystemSettings = *GetDefault<UPoolSystemSettings>();
	UE_LOG(LogPoolSubsystem, Log, TEXT("======= Initializing pools ======="));

	FString IsClient = GetWorld()->GetNetMode() == NM_Client ? "Client" : "Server";
	for (const FPoolsToSpawn& PoolToSpawn : PoolSystemSettings.Pools)
	{
		
		// we might want to spawn pools only server side.
		if (!PoolToSpawn.bAuthorityOnly || GetWorld()->GetNetMode() != NM_Client)
		{
			UClass* Class = PoolToSpawn.Class.LoadSynchronous();
			if (ensure(Class))
			{
				ABasePool* Pool = GetWorld()->SpawnActor<ABasePool>(Class, SpawnInfo);
				UE_LOG(LogPoolSubsystem, Log, TEXT("Spawned pool %s on %s"), *GetNameSafe(Pool), *IsClient);
				RegisterPool(Pool);
				Pool->PreAllocateObjects(PoolToSpawn.PreAllocastionClasses, PoolToSpawn.PreAllocationNumber);
			}
		}
	}

	// Spawn generic pools in case we dont want to do special handling on them.
	RegisterPool(GetWorld()->SpawnActor<ABasePool>(AActorPoolBase::StaticClass(), SpawnInfo));
	UE_LOG(LogPoolSubsystem, Log, TEXT("Spawned pool default actor pool on %s"), *IsClient);
	RegisterPool(GetWorld()->SpawnActor<ABasePool>(AObjectPoolBase::StaticClass(), SpawnInfo));
	UE_LOG(LogPoolSubsystem, Log, TEXT("Spawned pool default object pool on %s"), *IsClient);
}

void UPoolSubsystem::RegisterPool(ABasePool* Pool)
{
	/* On authority they are instantly registered, but if we have a replicated pool from the server
	 * it has to register itself once replicated, see ABasePool::BeginPlay()*/
	bool bIsClient = GetWorld()->GetNetMode() == NM_Client;
	bool bIsServerPool = (bIsClient && !Pool->HasAuthority()) || !bIsClient;
	TArray<ABasePool*>& PoolToUse =  bIsServerPool ? AuthPools : ClientPools;
	if (ensure(Pool) && ensure(!PoolToUse.Contains(Pool)))
	{
		PoolToUse.Add(Pool);
		/* sort from child classes to parent
		e.g if you have your custom projectile, you want to find that pool first
		rather than the generic actor pool */
		PoolToUse.Sort([](ABasePool& A, ABasePool& B)
		{
			return A.GetTargetclass()->IsChildOf(B.GetTargetclass());
		});
	}
}

AActor* UPoolSubsystem::K2_BeginSpawningPoolActor(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, AActor* Owner /*= nullptr*/, ESpawnActorScaleMethod TransformScaleMethod /*= ESpawnActorScaleMethod::MultiplyWithRoot*/)
{
	if (WorldContextObject)
	{
		if (UPoolSubsystem* PoolSubsystem = WorldContextObject->GetWorld()->GetSubsystem<UPoolSubsystem>())
		{
			AActor* SpawnedActor = PoolSubsystem->RequestPoolObject<AActor>(ActorClass, Owner, true);
			SetActorTransform(SpawnTransform, TransformScaleMethod, SpawnedActor);

			return SpawnedActor;
		}
	}

	return nullptr;
}

AActor* UPoolSubsystem::K2_FinishSpawningPoolActor(const UObject* WorldContextObject, AActor* Actor, const FTransform& SpawnTransform, ESpawnActorScaleMethod TransformScaleMethod)
{
	if (WorldContextObject)
	{
		if (UPoolSubsystem* PoolSubsystem = WorldContextObject->GetWorld()->GetSubsystem<UPoolSubsystem>())
		{
			AActor* SpawnedActor = PoolSubsystem->FinishSpawningPoolObject<AActor>(Actor, SpawnTransform);
			SetActorTransform(SpawnTransform, TransformScaleMethod, SpawnedActor);
			return SpawnedActor;
		}
	}

	return nullptr;
}

UObject* UPoolSubsystem::K2_BeginSpawnPoolObject(const UObject* WorldContextObject, TSubclassOf<UObject> ObjectClass)
{
	if (WorldContextObject)
	{
		if (UPoolSubsystem* PoolSubsystem = WorldContextObject->GetWorld()->GetSubsystem<UPoolSubsystem>())
		{
			return  PoolSubsystem->RequestPoolObject<UObject>(ObjectClass, nullptr, true);
		}
	}

	return nullptr;
}

UObject* UPoolSubsystem::K2_FinishSpawnPoolObject(const UObject* WorldContextObject, UObject* Object)
{
	if (WorldContextObject)
	{
		if (UPoolSubsystem* PoolSubsystem = WorldContextObject->GetWorld()->GetSubsystem<UPoolSubsystem>())
		{
			return  PoolSubsystem->FinishSpawningPoolObject<UObject>(Object, FTransform::Identity);
		}
	}

	return nullptr;
}

void UPoolSubsystem::SetActorTransform(const FTransform& SpawnTransform, ESpawnActorScaleMethod TransformScaleMethod, AActor* SpawnedActor)
{
	USceneComponent* const SceneRootComponent = SpawnedActor->GetRootComponent();
	if (SceneRootComponent != nullptr)
	{
		check(SceneRootComponent->GetOwner() == SpawnedActor);
				
		// Respect any non-default transform value that the root component may have received from the archetype that's owned
		// by the native CDO, so the final transform might not always necessarily equate to the passed-in UserSpawnTransform.
		const FTransform RootTransform(SceneRootComponent->GetRelativeRotation(), SceneRootComponent->GetRelativeLocation(), SceneRootComponent->GetRelativeScale3D());
		FTransform FinalRootComponentTransform = RootTransform;
		switch(TransformScaleMethod)
		{
		case ESpawnActorScaleMethod::OverrideRootScale:
			FinalRootComponentTransform = SpawnTransform;
			break;
		case ESpawnActorScaleMethod::MultiplyWithRoot:
		case ESpawnActorScaleMethod::SelectDefaultAtRuntime:
			FinalRootComponentTransform = RootTransform * SpawnTransform;
			break;
		}
		SceneRootComponent->SetWorldTransform(FinalRootComponentTransform, false, nullptr, ETeleportType::ResetPhysics);
	}
}

ABasePool* UPoolSubsystem::FindClassInPool(TSubclassOf<UObject> Class, TArray<ABasePool*>& PoolToUse)
{
	ABasePool* PotentialPool = nullptr;
	for (ABasePool* Pool : PoolToUse)
	{
		if (!Pool)
		{
			continue;
		}
		
		if (Pool->GetTargetclass() == Class)
		{
			return Pool;
		}

		if (!PotentialPool && Pool->ShouldIncludeChildrenClasses() && Class->IsChildOf(Pool->GetTargetclass().Get()))
		{
			PotentialPool = Pool;
		}
	}

	return PotentialPool;
}

ABasePool* UPoolSubsystem::FindPool(UClass* Class)
{
	TArray<ABasePool*>& PoolToUse =  GetWorld()->GetNetMode() == NM_Client ? ClientPools : AuthPools;
	return FindClassInPool(Class, PoolToUse);
}

ABasePool* UPoolSubsystem::FindPool(UObject* Target)
{
	AActor* TargetActor = Cast<AActor>(Target);

	if (!TargetActor)
	{
		return FindPool(Target->GetClass());
	}

	auto FindPoolForObject = [Target](ABasePool* Pool)
	{
		return Pool->DoesObjectBelongsToPool(Target);
	};
	
	ABasePool** Pool = AuthPools.FindByPredicate(FindPoolForObject);

	if (Pool)
	{
		return *Pool;
	}
	
	Pool = ClientPools.FindByPredicate(FindPoolForObject);
	if (Pool)
	{
		return *Pool;
	}

	return nullptr;
}
