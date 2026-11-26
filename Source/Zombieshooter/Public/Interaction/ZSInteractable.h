#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ZSInteractable.generated.h"

class AZSPlayerCharacter;

/** Collision channel for interaction traces (DefaultEngine.ini: GameTraceChannel2 = "Interact"). Interactables must block it. */
#define ZS_TRACE_INTERACT ECC_GameTraceChannel2

UINTERFACE(MinimalAPI, BlueprintType)
class UZSInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Anything the player can use with Interact (X / E): ammo crates now, bunker stations and doors later. */
class ZOMBIESHOOTER_API IZSInteractable
{
	GENERATED_BODY()

public:
	/** Short verb shown in the prompt, e.g. "REFILL AMMO". */
	virtual FText GetInteractPrompt() const = 0;
	virtual bool CanInteract(const AZSPlayerCharacter* Player) const { return true; }
	virtual void Interact(AZSPlayerCharacter* Player) = 0;
};
