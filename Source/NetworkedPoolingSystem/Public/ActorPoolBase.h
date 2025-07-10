// Copyright JOSEUEM, 2024

#pragma once

#include "CoreMinimal.h"
#include "BasePool.h"
#include "ActorPoolBase.generated.h"

class UPoolSubsystem;

USTRUCT()
struct FDefaultComponentValues
{
	GENERATED_USTRUCT_BODY()

	TSoftObjectPtr<UActorComponent> Component;
	bool bAutoActivate = false;
	FTransform RelativeTransform;
	bool bVisible = false;
	bool bHiddenInGame = false;
	bool bSimulatesPhysics = false;
	bool bGravityEnabled = false;
	ECollisionEnabled::Type CollisionEnabled;
};

USTRUCT()
struct FDefaultComponentsValuesContainer
{
	GENERATED_USTRUCT_BODY()


	const FDefaultComponentValues& FindComponent(UActorComponent* InComponent);

	void Add(const FDefaultComponentValues& InComponentValues);

	TArray<FDefaultComponentValues> ComponentValues;
};

/**
 * 
 */
UCLASS()
class NETWORKEDPOOLINGSYSTEM_API AActorPoolBase : public ABasePool
{
	GENERATED_BODY()
public:
	AActorPoolBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual UObject* PreSpawnPoolObject(TSubclassOf<UObject> InClass, AActor* InOwner) override;
	virtual void FinishSpawningPoolObject(UObject* InTarget, const FTransform& InTransform) override;
	virtual void ReturnToPool(UObject* InObject) override;
	
private:
	virtual void Tick(float DeltaSeconds) override;
	virtual void RegisterWithPoolSubsystem(UPoolSubsystem* Subsystem);
	void TryRegisterWithPoolSubsystem();
	void TryStoreComponentsDefaultValues(AActor* InTarget);
	void ActiveActorComponents(AActor* InTarget);
	void ActivateMovementComponent(AActor* InTarget);
	void ResetComponentsTransform(AActor* InTarget);
	void DeactivateComponents(AActor* InTarget);
	void DisableActor(AActor* InTarget);
	void SetActorEnabled(AActor* InTarget, bool bEnabled);
	bool HasReplicatedProperties(AActor* TargetActor);
	bool IsActorReadyToSpawn(AActor* TargetActor);
	void SetReplicationEnabled(AActor* TargetActor, bool bEnabled);
	void ServerFinishSpawningActor(UObject* InTarget, const FTransform& InTransform);
	void ClientFinishSpawningActor(UObject* InTarget, const FTransform& InTransform);
	
	struct FPendingActorData
	{
		TObjectPtr<AActor> Actor;
		FTransform Transform;
	};
	TArray<FPendingActorData> WaitingToSpawnActorQueue;
	TMap<AActor*, FDefaultComponentsValuesContainer> ActorDefaultComponentValuesMap; 
};
