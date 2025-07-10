// Copyright JOSEUEM, 2024

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Net/UnrealNetwork.h"
#include "Engine/NetSerialization.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "PoolObjectsTypes.generated.h"

class ABasePool;


DECLARE_LOG_CATEGORY_EXTERN(LogPoolSubsystem, Log, All);
// FPoolObjectItem:
/* this array struct is used to pool objects manipulation, it helps with replication of pool object data, in this case
 * we use it for transform replication for objects that do not replicate movement and for setting free/used state on the client
 */

USTRUCT(BlueprintType)
struct FPoolObjectItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	UObject* Object = nullptr;

	UPROPERTY()
	bool bIsFree = false;

	UPROPERTY()
	FVector_NetQuantize Location;

	UPROPERTY()
	FQuat Rotation;

	UPROPERTY()
	FVector_NetQuantize Scale;

	UPROPERTY(NotReplicated)
	bool bIsFirstSpawn = true;
	
	void SetTransform(const FTransform& InTransform)
	{
		Location = InTransform.GetLocation();
		Scale = InTransform.GetScale3D();
		Rotation = InTransform.GetRotation().GetNormalized();
	}
	
	bool operator==(const FPoolObjectItem& Other) const
	{
		return Object == Other.Object;
	}
};

USTRUCT(BlueprintType)
struct FPoolObjectsArray : public FFastArraySerializer
{
	GENERATED_BODY()
	FPoolObjectsArray()
		: OwningPool(nullptr)
	{
	}
	
	bool IsEmpty()
	{
		return PoolObjects.IsEmpty();
	}
	
	FPoolObjectItem& Add(UObject* Target, bool bIsFree);

	// Contains function to check if the target object exists in the pool
	bool Contains(UObject* Target) const
	{
		return PoolObjects.ContainsByPredicate([Target](const FPoolObjectItem& Item)
		{
			return Item.Object == Target;
		});
	}
	
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract
	
	FPoolObjectItem& Find(UObject* Target);

	FPoolObjectItem& FindOrAdd(UObject* Target);
	
	void SetItemTransform(AActor* Target, const FTransform& InTransform);
	bool IsFirstSpawn(AActor* Target);

	// Get the first free object from the pool
	UObject* GetFreeObject(TSubclassOf<UObject> Class);
	
	// Serialization function
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FastArrayDeltaSerialize<FPoolObjectItem, FPoolObjectsArray>(PoolObjects, DeltaParms, *this);
	}
	
	void SetOwningPool(ABasePool* InPool);
	
private:
	void CheckItemPostReplication(FPoolObjectItem& Item);

private:
	UPROPERTY()
	TArray<FPoolObjectItem> PoolObjects;
	
	UPROPERTY(NotReplicated)
	TObjectPtr<ABasePool> OwningPool;
};

template<>
struct TStructOpsTypeTraits<FPoolObjectsArray> : public TStructOpsTypeTraitsBase2<FPoolObjectsArray>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
