// Copyright JOSEUEM, 2024

#pragma once

#include "GameFramework/Info.h"
#include "CoreMinimal.h"
#include "PoolObjectsTypes.h"
#include "PoolSystemSettings.h"
#include "UObject/NoExportTypes.h"
#include "BasePool.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class NETWORKEDPOOLINGSYSTEM_API ABasePool : public AInfo
{
	GENERATED_BODY()
	
public:
	ABasePool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UObject* PreSpawnPoolObject(TSubclassOf<UObject> InClass, AActor* InOwner);
	void PreAllocateObjects(TArray<TSoftClassPtr<UObject>> PreAllocastionClasses, int32 PreAllocationNumber);
	
	virtual void FinishSpawningPoolObject(UObject* Target, const FTransform& Transform);
	void ResetToDefaultValues(UObject* Object);
	virtual void ReturnToPool(UObject* Object);

	const TSubclassOf<UObject>& GetTargetclass() const { return TargetClass; }
	bool ShouldIncludeChildrenClasses() const { return bIncludeChildrenClasses; }

	bool DoesObjectBelongsToPool(UObject* InObject) const { return PoolObjects.Contains(InObject); }
	bool IsObjectFree(UObject* InObject);
protected:
	
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnFinishSpawningPoolObject(UObject* Target, const FTransform& Transform = FTransform::Identity);
	
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnPreSpawnPoolObject(UObject* Object, AActor* InOwner = nullptr);
	
	bool IsClassFromProject(UClass* Class);
	
	template<typename T>
	T* FindInPool(TSubclassOf<UObject> Class)
	{
		// Ensure the type is derived from Object
		static_assert(TIsDerivedFrom<T, UObject>::IsDerived, "T must be a subclass of UObject");

		// Check if the pool has any actors available
		if (PoolObjects.IsEmpty())
		{
			return nullptr;
		}
		
		// Ensure the actor is of the correct type
		T* TypedActor = Cast<T>(PoolObjects.GetFreeObject(Class));
		if (TypedActor)
		{
			return TypedActor;
		}
		
		return nullptr;
	}

private:
	bool CanResetProperty(FProperty* Property) const;
	
protected:
	TSubclassOf<UObject> TargetClass;
	bool bIncludeChildrenClasses = true;
	
	UPROPERTY(Replicated)
	FPoolObjectsArray PoolObjects;
};
