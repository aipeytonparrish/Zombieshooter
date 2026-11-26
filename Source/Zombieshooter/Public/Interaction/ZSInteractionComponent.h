#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZSInteractionComponent.generated.h"

/** Finds the interactable the player is looking at (short trace on the Interact channel) and uses it. */
UCLASS(ClassGroup = (ZS), meta = (BlueprintSpawnableComponent))
class ZOMBIESHOOTER_API UZSInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZSInteractionComponent();

	UPROPERTY(EditAnywhere, Category = "ZS|Interaction", meta = (ForceUnits = "cm"))
	float Reach = 280.f;

	/** The actor currently in focus (implements IZSInteractable), or nullptr. */
	AActor* GetFocused() const { return Focused.Get(); }
	FText GetPrompt() const;

	/** Use the focused interactable. Returns true if something was used. */
	bool TryInteract();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<AActor> Focused;
};
