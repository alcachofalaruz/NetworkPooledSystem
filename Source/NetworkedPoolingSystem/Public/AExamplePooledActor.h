// Copyright JOSEUEM, 2024

#pragma once

#include "CoreMinimal.h"
#include "PoolInterface.h"
#include "GameFramework/Actor.h"
#include "AExamplePooledActor.generated.h"

UCLASS()
class NETWORKEDPOOLINGSYSTEM_API AAExamplePooledActor : public AActor, public IPoolInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAExamplePooledActor();

	virtual void OnPoolObjectActivate_Implementation() override;
	virtual void OnPoolObjectDeactivate_Implementation() override;
	virtual void OnPoolObjectContruct_Implementation() override;
	virtual TArray<FString> GetPropertyResetExcludeList_Implementation() override;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void K2_DestroyActor() override;
	virtual void LifeSpanExpired() override;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(EditAnywhere, Category="Pooling")
	TArray<FString> PropertiesToExclude;
};
