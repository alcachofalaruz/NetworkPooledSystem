// Copyright JOSEUEM, 2024

#include "PoolObjectsTypes.h"

#include "BasePool.h"
#include "PoolInterface.h"

DEFINE_LOG_CATEGORY(LogPoolSubsystem);

FPoolObjectItem& FPoolObjectsArray::Add(UObject* Target, bool bIsFree)
{
	if (FPoolObjectItem* ExistingItem = PoolObjects.FindByPredicate([Target](const FPoolObjectItem& Item) { return Item.Object == Target; }))
	{
		ExistingItem->bIsFree = bIsFree;
		MarkItemDirty(*ExistingItem);
		return *ExistingItem;
	}
	else
	{
		// If not found, add a new item
		FPoolObjectItem NewItem;
		NewItem.Object = Target;
		NewItem.bIsFree = bIsFree;

		PoolObjects.Add(MoveTemp(NewItem));
		MarkItemDirty(NewItem);
		return PoolObjects.Last();
	}
}

void FPoolObjectsArray::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
}

void FPoolObjectsArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	// Handle any logic after objects are added to the array during replication
	for (int32 Index : AddedIndices)
	{
		if (PoolObjects.IsValidIndex(Index))
		{
			FPoolObjectItem& Item = PoolObjects[Index];
			CheckItemPostReplication(Item);
		}
	}
}

void FPoolObjectsArray::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	// Handle any logic after objects are changed in the array during replication
	for (int32 Index : ChangedIndices)
	{
		if (PoolObjects.IsValidIndex(Index))
		{
			FPoolObjectItem& Item = PoolObjects[Index];
			CheckItemPostReplication(Item);
		}
	}
}

FPoolObjectItem& FPoolObjectsArray::Find(UObject* Target)
{
	FPoolObjectItem* FoundItem = PoolObjects.FindByPredicate([Target](const FPoolObjectItem& Item) { return Item.Object == Target; });

	if (FoundItem)
	{
		return *FoundItem;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Object not found in pool! Returning a default item."));
	static FPoolObjectItem DefaultItem;
	return DefaultItem;
}

FPoolObjectItem& FPoolObjectsArray::FindOrAdd(UObject* Target)
{
	if (FPoolObjectItem* FoundItem = PoolObjects.FindByPredicate([Target](const FPoolObjectItem& Item) { return Item.Object == Target; }))
	{
		return *FoundItem;
	}
	
	// If not found, add the object to the pool and return the new item
	return Add(Target, true); 
}

void FPoolObjectsArray::SetItemTransform(AActor* Target, const FTransform& InTransform)
{
	if (!Target->HasAuthority())
	{
		return;
	}
	
	FPoolObjectItem& Item = Find(Target);
	Item.SetTransform(InTransform);
	MarkItemDirty(Item);
}

bool FPoolObjectsArray::IsFirstSpawn(AActor* Target)
{
	FPoolObjectItem& Item = Find(Target);
	return Item.bIsFirstSpawn;
}

UObject* FPoolObjectsArray::GetFreeObject(TSubclassOf<UObject> Class)
{
	// Find the first free object in the pool
	FPoolObjectItem* FreeItem = PoolObjects.FindByPredicate([Class](const FPoolObjectItem& Item)
	{
		return Item.Object && Item.Object->GetClass() == Class && Item.bIsFree;
	});

	if (FreeItem)
	{
		// Mark the object as not free since it's being used
		FreeItem->bIsFree = false;
		MarkItemDirty(*FreeItem);

		return FreeItem->Object;
	}

	return nullptr;
}

void FPoolObjectsArray::SetOwningPool(ABasePool* InPool)
{
	OwningPool = InPool;
}

void FPoolObjectsArray::CheckItemPostReplication(FPoolObjectItem& Item)
{
	// Check if the object changed from free to not free and activate if needed
	if (Item.Object && OwningPool)
	{
		bool bImplementsInterface = Item.Object->GetClass()->ImplementsInterface(UPoolInterface::StaticClass());

		if (Item.bIsFree)
		{
			if (bImplementsInterface)
			{
				IPoolInterface::Execute_OnPoolObjectDeactivate(Item.Object);
			}
			OwningPool.Get()->ReturnToPool(Item.Object);
		}
		else
		{
			if (bImplementsInterface)
			{
				IPoolInterface::Execute_OnPoolObjectContruct(Item.Object);
			}

			FTransform ActorTransform = FTransform(Item.Rotation.Rotator(), Item.Location, Item.Scale);
			OwningPool.Get()->FinishSpawningPoolObject(Item.Object, ActorTransform);

			if (bImplementsInterface)
			{
				IPoolInterface::Execute_OnPoolObjectActivate(Item.Object);
			}

			Item.bIsFirstSpawn = false;
		}
	}
}
