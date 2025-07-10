// Copyright JOSEUEM, 2024


#include "AExamplePooledActor.h"
#include "Engine/World.h"
#include "PoolSubsystem.h"

// Sets default values
AAExamplePooledActor::AAExamplePooledActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AAExamplePooledActor::OnPoolObjectActivate_Implementation()
{
	// Use this instead of begin play
}

void AAExamplePooledActor::OnPoolObjectDeactivate_Implementation()
{
	// Use this instead of destruct
}

void AAExamplePooledActor::OnPoolObjectContruct_Implementation()
{
	// Use this instead of contruct
}

TArray<FString> AAExamplePooledActor::GetPropertyResetExcludeList_Implementation()
{
	return PropertiesToExclude;
}

// Called when the game starts or when spawned
void AAExamplePooledActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AAExamplePooledActor::K2_DestroyActor()
{
	if (UPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UPoolSubsystem>())
	{
		PoolSubsystem->ReturnToPool(this);
	}
}

void AAExamplePooledActor::LifeSpanExpired()
{
	if (UPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UPoolSubsystem>())
	{
		PoolSubsystem->ReturnToPool(this);
	}
}

// Called every frame
void AAExamplePooledActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

