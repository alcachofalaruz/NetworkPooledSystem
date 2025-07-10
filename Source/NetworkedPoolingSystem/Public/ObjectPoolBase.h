// Copyright JOSEUEM, 2024

#pragma once

#include "CoreMinimal.h"
#include "BasePool.h"
#include "ObjectPoolBase.generated.h"

/**
 * 
 */
UCLASS()
class NETWORKEDPOOLINGSYSTEM_API AObjectPoolBase : public ABasePool
{
	GENERATED_BODY()

public:
	AObjectPoolBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UObject* PreSpawnPoolObject(TSubclassOf<UObject> InClass, AActor* InOwner = nullptr) override;
};
