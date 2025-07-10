// Copyright JOSEUEM, 2024


#include "ActorPoolBase.h"
#include "Sound/SoundBase.h"
#include "PoolSubsystem.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"

const FDefaultComponentValues& FDefaultComponentsValuesContainer::FindComponent(UActorComponent* InComponent)
{
	FDefaultComponentValues* FoundItem = ComponentValues.FindByPredicate([InComponent](const FDefaultComponentValues& Item) { return Item.Component == InComponent; });

	if (FoundItem)
	{
		return *FoundItem;
	}
	
	static FDefaultComponentValues DefaultItem;
	return DefaultItem;
}

void FDefaultComponentsValuesContainer::Add(const FDefaultComponentValues& InComponentValues)
{
	ComponentValues.Add(InComponentValues);
}

AActorPoolBase::AActorPoolBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TargetClass = AActor::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AActorPoolBase::BeginPlay()
{
	Super::BeginPlay();

	TryRegisterWithPoolSubsystem();
}

UObject* AActorPoolBase::PreSpawnPoolObject(TSubclassOf<UObject> InClass, AActor* InOwner)
{
	if (AActor* PoolActor = FindInPool<AActor>(InClass))
	{
		UE_LOG(LogPoolSubsystem, Verbose, TEXT("Reusing pool actor %s"), *GetNameSafe(PoolActor));
		PoolActor->SetOwner(Owner);
		return PoolActor;
	}
	
	AActor* NewActor = GetWorld()->SpawnActorDeferred<AActor>(InClass, FTransform::Identity, InOwner);
	UE_LOG(LogPoolSubsystem, Verbose, TEXT("Spawning new pool object %s"), *GetNameSafe(NewActor));
	// Disable actor instantly, since we might be in the "deferred" spawning state
	DisableActor(NewActor);

	BP_OnPreSpawnPoolObject(NewActor);
	return NewActor;
}

void AActorPoolBase::ServerFinishSpawningActor(UObject* InTarget, const FTransform& InTransform)
{
	AActor* TargetActor = Cast<AActor>(InTarget);
	PoolObjects.Add(InTarget, false);
	// set the transform so is replicated to the clients, this is needed for actors that DO NOT replicate movement
	PoolObjects.SetItemTransform(TargetActor, InTransform);
	ResetComponentsTransform(TargetActor);
	ActiveActorComponents(TargetActor);
	TargetActor->SetActorTransform(InTransform, false, nullptr, ETeleportType::ResetPhysics);
	ActivateMovementComponent(TargetActor);
	SetActorEnabled(TargetActor, true);
	ForceNetUpdate();
}

void AActorPoolBase::ClientFinishSpawningActor(UObject* InTarget, const FTransform& InTransform)
{
	AActor* TargetActor = Cast<AActor>(InTarget);
	
	// Check if we are waiting replication and delay spawn until we get the properties replicated
	if (!IsActorReadyToSpawn(TargetActor))
	{
		FPendingActorData PendingActorData;
		PendingActorData.Actor = TargetActor;
		PendingActorData.Transform = InTransform;
		WaitingToSpawnActorQueue.Add(MoveTemp(PendingActorData));
		return;
	}

	PoolObjects.Add(InTarget, false);
	ForceNetUpdate();

	// If we are an actor replicated from the server, we won't have the default values stored
	TryStoreComponentsDefaultValues(TargetActor);

	if (!PoolObjects.IsFirstSpawn(TargetActor))
	{
		/* Order is important here, if you set actor transform before activating components it wont update the transform
		 * and it will create a visual mismatch between server and client*/
		ResetComponentsTransform(TargetActor);
		ActiveActorComponents(TargetActor);
		TargetActor->SetActorTransform(InTransform, false, nullptr, ETeleportType::ResetPhysics);
		ActivateMovementComponent(TargetActor);
	}

	SetActorEnabled(TargetActor, true);
}

void AActorPoolBase::FinishSpawningPoolObject(UObject* InTarget, const FTransform& InTransform)
{
	if (PoolObjects.Contains(InTarget))
	{
		if (HasAuthority())
		{
			ServerFinishSpawningActor(InTarget, InTransform);
		}
		else
		{
			ClientFinishSpawningActor(InTarget, InTransform);
		}
		
		Super::FinishSpawningPoolObject(InTarget, InTransform);
		return;
	}

	PoolObjects.Add(InTarget, false);
	ForceNetUpdate();
	
	AActor* TargetActor = Cast<AActor>(InTarget);
	PoolObjects.SetItemTransform(TargetActor, TargetActor->GetTransform());
	if (ensureMsgf(TargetActor, TEXT("Called FinishSpawningPoolObject in an object that is not of type AActor")))
	{
		SetActorEnabled(TargetActor, true);
		TargetActor->FinishSpawning(InTransform);
	}

	TryStoreComponentsDefaultValues(TargetActor);
	
	Super::FinishSpawningPoolObject(InTarget, InTransform);
}

void AActorPoolBase::ReturnToPool(UObject* InObject)
{
	Super::ReturnToPool(InObject);
	AActor* PoolActor = Cast<AActor>(InObject);
	check(PoolActor);

	DisableActor(PoolActor);
	SetReplicationEnabled(PoolActor, false);
}

void AActorPoolBase::Tick(float DeltaSeconds)
{
	if (WaitingToSpawnActorQueue.IsEmpty())
	{
		return;
	}

	for (int i = WaitingToSpawnActorQueue.Num() - 1; i >= 0; --i)
	{
		FPendingActorData& PendingActorData = WaitingToSpawnActorQueue[i];
		AActor* Actor = PendingActorData.Actor.Get();
		if (Actor && IsActorReadyToSpawn(PendingActorData.Actor.Get()))
		{
			FinishSpawningPoolObject(Actor, PendingActorData.Transform);
			WaitingToSpawnActorQueue.RemoveAt(i);
		}
	}
}

void AActorPoolBase::RegisterWithPoolSubsystem(UPoolSubsystem* Subsystem)
{
	Subsystem->RegisterPool(this);
}

void AActorPoolBase::TryRegisterWithPoolSubsystem()
{
	if (!HasAuthority())
	{
		UPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UPoolSubsystem>();
		if (ensure(PoolSubsystem))
		{
			RegisterWithPoolSubsystem(PoolSubsystem);
		}
	}
}

void AActorPoolBase::TryStoreComponentsDefaultValues(AActor* InTarget)
{
	if (ActorDefaultComponentValuesMap.Contains(InTarget))
	{
		return;
	}
	
	TInlineComponentArray<UActorComponent*> Components;
	InTarget->GetComponents<UActorComponent>(Components);

	FDefaultComponentsValuesContainer ComponentsValuesContainer;
	for (UActorComponent* Component : Components)
	{
		FDefaultComponentValues DefaultValues;
		DefaultValues.Component = Component;
		DefaultValues.bAutoActivate = Component->bAutoActivate;
		
		const USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
		if (SceneComponent)
		{
			DefaultValues.Component = Component;
			bool bShouldCheckAutoActivate = Component->IsA(UFXSystemComponent::StaticClass()) || Component->IsA(UAudioComponent::StaticClass());
			DefaultValues.bAutoActivate = !bShouldCheckAutoActivate || Component->bAutoActivate;
			DefaultValues.bVisible = SceneComponent->IsVisible();
			DefaultValues.bHiddenInGame = SceneComponent->bHiddenInGame;
			DefaultValues.RelativeTransform = SceneComponent->GetRelativeTransform();
			if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
			{
				DefaultValues.bGravityEnabled = PrimitiveComponent->IsGravityEnabled();
				DefaultValues.bSimulatesPhysics = PrimitiveComponent->IsSimulatingPhysics();
				DefaultValues.CollisionEnabled = PrimitiveComponent->GetCollisionEnabled();
			}
		}
		
		ComponentsValuesContainer.Add(DefaultValues);
	}
	ActorDefaultComponentValuesMap.Add(InTarget, ComponentsValuesContainer);
}

void AActorPoolBase::ActiveActorComponents(AActor* InTarget)
{
	FDefaultComponentsValuesContainer* DefaultValues = ActorDefaultComponentValuesMap.Find(InTarget);
	for (const FDefaultComponentValues& ComponentValues : DefaultValues->ComponentValues)
	{
		UActorComponent* Component = ComponentValues.Component.Get();
		check(Component);

		if (UParticleSystemComponent* FXComp = Cast<UParticleSystemComponent>(Component))
		{
			FXComp->TickComponent(0.0f, LEVELTICK_All, nullptr);
			FXComp->ActivateSystem();
			continue;
		}

		const FDefaultComponentValues& ComponentDefaultValues = DefaultValues->FindComponent(Component);
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
		{
			SceneComp->SetRelativeTransform(ComponentDefaultValues.RelativeTransform, false, nullptr, ETeleportType::ResetPhysics);
			if (UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(Component))
			{
				PrimitiveComp->SetEnableGravity(ComponentDefaultValues.bGravityEnabled);
				PrimitiveComp->SetCollisionEnabled(ComponentDefaultValues.CollisionEnabled);
				PrimitiveComp->SetSimulatePhysics(ComponentDefaultValues.bSimulatesPhysics);
			}
			
			SceneComp->SetHiddenInGame(ComponentDefaultValues.bHiddenInGame);
			SceneComp->SetVisibility(ComponentDefaultValues.bVisible);
		}
		
		if (ComponentDefaultValues.bAutoActivate)
		{
			Component->Activate();
		}
	}
}

void AActorPoolBase::ActivateMovementComponent(AActor* InTarget)
{
	// Get the default component from the default object
	UProjectileMovementComponent* ProjMoveComp = InTarget->FindComponentByClass<UProjectileMovementComponent>();
	if (ProjMoveComp)
	{
		ProjMoveComp->SetUpdatedComponent(InTarget->GetRootComponent());
		if (ProjMoveComp->Velocity.SizeSquared() > 0.f)
		{
			// InitialSpeed > 0 overrides initial velocity magnitude.
			if (ProjMoveComp->InitialSpeed > 0.f)
			{
				ProjMoveComp->Velocity = ProjMoveComp->Velocity.GetSafeNormal() * ProjMoveComp->InitialSpeed;
			}

			if (ProjMoveComp->bInitialVelocityInLocalSpace)
			{
				ProjMoveComp->SetVelocityInLocalSpace(ProjMoveComp->Velocity);
			}

			if (ProjMoveComp->bRotationFollowsVelocity)
			{
				if (ProjMoveComp->UpdatedComponent)
				{
					FRotator DesiredRotation = ProjMoveComp->Velocity.Rotation();
					if (ProjMoveComp->bRotationRemainsVertical)
					{
						DesiredRotation.Pitch = 0.0f;
						DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
						DesiredRotation.Roll = 0.0f;
					}

					ProjMoveComp->UpdatedComponent->SetWorldRotation(DesiredRotation);
				}
			}

			ProjMoveComp->UpdateComponentVelocity();
		
			if (ProjMoveComp->UpdatedPrimitive && ProjMoveComp->UpdatedPrimitive->IsSimulatingPhysics())
			{
				ProjMoveComp->UpdatedPrimitive->SetPhysicsLinearVelocity(ProjMoveComp->Velocity);
			}
		}
	}
}

void AActorPoolBase::ResetComponentsTransform(AActor* InTarget)
{
	FDefaultComponentsValuesContainer* DefaultValues = ActorDefaultComponentValuesMap.Find(InTarget);
	for (const FDefaultComponentValues& ComponentValues : DefaultValues->ComponentValues)
	{
		USceneComponent* Component = Cast<USceneComponent>(ComponentValues.Component.Get());
		if (!Component)
		{
			continue;
		}
		check(Component);
		Component->SetRelativeTransform(ComponentValues.RelativeTransform, false, nullptr, ETeleportType::ResetPhysics);
	}
}

void AActorPoolBase::DeactivateComponents(AActor* InTarget)
{
	/* reset and deactivate components, cast to specific types
	 * if it needs special handling
	 */
	TInlineComponentArray<UActorComponent*> Components;
	InTarget->GetComponents<UActorComponent>(Components);
	
	for (UActorComponent* Component : Components)
	{
		check(Component);
		if (UParticleSystemComponent* PfxComp = Cast<UParticleSystemComponent>(Component))
		{
			PfxComp->TickComponent(0.0f, LEVELTICK_All, nullptr);
			PfxComp->DeactivateSystem();
			continue;
		}

		UAudioComponent* AudioComp = Cast<UAudioComponent>(Component);
		if (AudioComp)
		{
			if ((AudioComp->Sound != nullptr) && (AudioComp->Sound->GetDuration() >= INDEFINITELY_LOOPING_DURATION))
			{
				static const float AudioDestroyFadouttime = 0.2f;
				AudioComp->FadeOut(AudioDestroyFadouttime, 0.0f);
			}

			continue;
		}

		if (UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(Component))
		{
			const TArray<FOverlapInfo> OverlapArray(PrimitiveComp->GetOverlapInfos());

			for (const FOverlapInfo& OverlapInfo : OverlapArray)
			{
				PrimitiveComp->EndComponentOverlap(OverlapInfo);
			}
			
			PrimitiveComp->SetSimulatePhysics(false);
			PrimitiveComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComp->SetEnableGravity(false);
		}

		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			SceneComponent->SetHiddenInGame(true);
			SceneComponent->SetVisibility(false);	
		}
		
		Component->Deactivate();
	}
}

void AActorPoolBase::DisableActor(AActor* InTarget)
{
	InTarget->SetOwner(nullptr);
	InTarget->SetInstigator(nullptr);
	InTarget->SetActorTransform(FTransform::Identity);
	
	SetActorEnabled(InTarget, false);
	DeactivateComponents(InTarget);
}

void AActorPoolBase::SetActorEnabled(AActor* InTarget, bool bEnabled)
{
	InTarget->SetActorHiddenInGame(!bEnabled);
	InTarget->SetActorTickEnabled(bEnabled && InTarget->PrimaryActorTick.bStartWithTickEnabled);
	InTarget->SetActorEnableCollision(bEnabled);
	SetReplicationEnabled(InTarget, bEnabled);
	InTarget->ForceNetUpdate();
}

bool AActorPoolBase::HasReplicatedProperties(AActor* TargetActor)
{
	return TargetActor->NetDormancy == DORM_Awake;
}

bool AActorPoolBase::IsActorReadyToSpawn(AActor* TargetActor)
{
	return TargetActor->IsActorInitialized() && HasReplicatedProperties(TargetActor);
}

void AActorPoolBase::SetReplicationEnabled(AActor* TargetActor, bool bEnabled)
{
	TargetActor->SetNetDormancy(bEnabled ? DORM_Awake : DORM_DormantAll);
}