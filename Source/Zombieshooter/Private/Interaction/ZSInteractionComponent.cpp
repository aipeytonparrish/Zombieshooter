#include "Interaction/ZSInteractionComponent.h"
#include "Interaction/ZSInteractable.h"
#include "Player/ZSPlayerCharacter.h"
#include "Player/ZSCameraComponent.h"
#include "Engine/World.h"

UZSInteractionComponent::UZSInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}

void UZSInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Focused = nullptr;
	const AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetOwner());
	if (!Char || !Char->GetFirstPersonCamera())
	{
		return;
	}

	const FVector Start = Char->GetFirstPersonCamera()->GetComponentLocation();
	const FVector End = Start + Char->GetControlRotation().Vector() * Reach;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSInteract), false, Char);
	FHitResult Hit;
	// A small sphere is forgiving on a controller.
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ZS_TRACE_INTERACT, FCollisionShape::MakeSphere(12.f), Params))
	{
		AActor* Actor = Hit.GetActor();
		if (const IZSInteractable* I = Cast<IZSInteractable>(Actor); I && I->CanInteract(Char))
		{
			Focused = Actor;
		}
	}
}

FText UZSInteractionComponent::GetPrompt() const
{
	if (const IZSInteractable* I = Cast<IZSInteractable>(Focused.Get()))
	{
		return I->GetInteractPrompt();
	}
	return FText::GetEmpty();
}

bool UZSInteractionComponent::TryInteract()
{
	AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetOwner());
	if (IZSInteractable* I = Cast<IZSInteractable>(Focused.Get()); I && Char && I->CanInteract(Char))
	{
		I->Interact(Char);
		return true;
	}
	return false;
}
