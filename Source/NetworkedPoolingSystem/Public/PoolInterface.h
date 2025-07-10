// Copyright JOSEUEM, 2024

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PoolInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPoolInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class NETWORKEDPOOLINGSYSTEM_API IPoolInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// Blueprint event that is called before the object is spawned or activated.
	UFUNCTION(BlueprintNativeEvent, Category="Object Pooling")
	void OnPoolObjectContruct();
	virtual void OnPoolObjectContruct_Implementation() = 0;

	// Blueprint event that is called when the object is activated from the pool.
	UFUNCTION(BlueprintNativeEvent, Category="Object Pooling")
	void OnPoolObjectActivate();
	virtual void OnPoolObjectActivate_Implementation() = 0;

	// Blueprint event that is called when the object is deactivated and returned to the pool.
	UFUNCTION(BlueprintNativeEvent, Category="Object Pooling")
	void OnPoolObjectDeactivate();
	virtual void OnPoolObjectDeactivate_Implementation() = 0;

	UFUNCTION(BlueprintNativeEvent, Category="Object Pooling")
	TArray<FString> GetPropertyResetExcludeList();
	virtual TArray<FString> GetPropertyResetExcludeList_Implementation() = 0;
};
