// Copyright JOSEUEM, 2024


#include "AbilityTask_SpawnPooledActor.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"
#include "PoolSubsystem.h"

UAbilityTask_SpawnPooledActor::UAbilityTask_SpawnPooledActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

UAbilityTask_SpawnPooledActor* UAbilityTask_SpawnPooledActor::SpawnPooledActor(UGameplayAbility* OwningAbility,
	FGameplayAbilityTargetDataHandle TargetData, TSubclassOf<AActor> Class)
{
	UAbilityTask_SpawnPooledActor* MyObj = NewAbilityTask<UAbilityTask_SpawnPooledActor>(OwningAbility);
	MyObj->CachedTargetDataHandle = MoveTemp(TargetData);
	return MyObj;
}

bool UAbilityTask_SpawnPooledActor::BeginSpawningActor(UGameplayAbility* OwningAbility,
	FGameplayAbilityTargetDataHandle TargetData, TSubclassOf<AActor> Class, AActor*& SpawnedActor)
{
	if (Ability && Ability->GetCurrentActorInfo()->IsNetAuthority() && ShouldBroadcastAbilityTaskDelegates())
	{
		UWorld* const World = GEngine->GetWorldFromContextObject(OwningAbility, EGetWorldErrorMode::LogAndReturnNull);
		UPoolSubsystem* PoolSubsystem = World ? World->GetSubsystem<UPoolSubsystem>() : nullptr;
		if (PoolSubsystem)
		{
			FTransform SpawnTransform;
			GetSpawnTransform(SpawnTransform);
			SpawnedActor = PoolSubsystem->K2_BeginSpawningPoolActor(this, Class, SpawnTransform, nullptr, ESpawnActorScaleMethod::OverrideRootScale);
		}
	}
	
	if (SpawnedActor == nullptr)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			DidNotSpawn.Broadcast(nullptr);
		}
		return false;
	}

	return true;
}

void UAbilityTask_SpawnPooledActor::GetSpawnTransform(FTransform& SpawnTransform)
{
	bool bTransformSet = false;
	if (FGameplayAbilityTargetData* LocationData = CachedTargetDataHandle.Get(0))		//Hardcode to use data 0. It's OK if data isn't useful/valid.
	{
		//Set location. Rotation is unaffected.
		if (LocationData->HasHitResult())
		{
			SpawnTransform.SetLocation(LocationData->GetHitResult()->Location);
			bTransformSet = true;
		}
		else if (LocationData->HasEndPoint())
		{
			SpawnTransform = LocationData->GetEndPointTransform();
			bTransformSet = true;
		}
	}
	if (!bTransformSet)
	{
		if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
		{
			SpawnTransform = ASC->GetOwner()->GetTransform();
		}
	}
}

void UAbilityTask_SpawnPooledActor::FinishSpawningActor(UGameplayAbility* OwningAbility,
                                                        FGameplayAbilityTargetDataHandle TargetData, AActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		FTransform SpawnTransform;
		GetSpawnTransform(SpawnTransform);

		UWorld* const World = GEngine->GetWorldFromContextObject(OwningAbility, EGetWorldErrorMode::LogAndReturnNull);
		UPoolSubsystem* PoolSubsystem = World ? World->GetSubsystem<UPoolSubsystem>() : nullptr;
		if (PoolSubsystem)
		{
			SpawnedActor->SetInstigator(Cast<APawn>(OwningAbility->GetAvatarActorFromActorInfo()));
			SpawnedActor->SetOwner(OwningAbility->GetAvatarActorFromActorInfo());
			PoolSubsystem->K2_FinishSpawningPoolActor(this,SpawnedActor, SpawnTransform);
		}

		if (ShouldBroadcastAbilityTaskDelegates())
		{
			Success.Broadcast(SpawnedActor);
		}
	}

	EndTask();
}
