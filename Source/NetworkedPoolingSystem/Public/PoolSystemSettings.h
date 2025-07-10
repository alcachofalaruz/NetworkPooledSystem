// Copyright JOSEUEM, 2024

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PoolSystemSettings.generated.h"

class ABasePool;

USTRUCT(Blueprintable)
struct FPoolsToSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Object Pooling")
	TSoftClassPtr<ABasePool> Class;

	UPROPERTY(EditAnywhere, Category="Object Pooling")
	bool bAuthorityOnly = true;
	
	UPROPERTY(EditAnywhere, Category="Object Pooling")
	TArray<TSoftClassPtr<UObject>> PreAllocastionClasses;

	// how many objects should we create upfront on this pool
	UPROPERTY(EditAnywhere, Category="Object Pooling")
	int32 PreAllocationNumber = 0;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Object Pooling Settings"))
class NETWORKEDPOOLINGSYSTEM_API UPoolSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPoolSystemSettings(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(config, EditAnywhere, Category = "Object Pooling")
	bool bUseAutomaticPropertyReset = true;

	UPROPERTY(config, EditAnywhere, Category = "Object Pooling")
	TArray<FPoolsToSpawn> Pools;
};
