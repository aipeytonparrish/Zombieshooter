#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ZSInteractable.h"
#include "ZSMysteryBox.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USoundBase;

/**
 * The mystery box: pay, the lid flies open, guns spin over it, and it settles on a random one - sometimes already
 * kitted with attachments. Take it before the box closes (about 8 s). A light beam marks where it is.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSMysteryBox : public AActor, public IZSInteractable
{
	GENERATED_BODY()

public:
	AZSMysteryBox();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Box")
	int32 Cost = 950;

	virtual FText GetInteractPrompt() const override;
	virtual bool CanInteract(const AZSPlayerCharacter* Player) const override;
	virtual void Interact(AZSPlayerCharacter* Player) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	enum class EState : uint8 { Idle, Rolling, Offering };
	void ShowGun(FName ItemId);
	void Close();

	UPROPERTY(VisibleAnywhere, Category = "ZS|Box")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Box")
	TObjectPtr<UStaticMeshComponent> Crate;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Box")
	TObjectPtr<UStaticMeshComponent> Lid;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Box")
	TObjectPtr<UStaticMeshComponent> Beam;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Box")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Box")
	TObjectPtr<USceneComponent> GunPivot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> GunParts;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> OpenSound;

	EState State = EState::Idle;
	float StateTime = 0.f;
	float NextSwap = 0.f;
	FName OfferedId;
	TArray<FName> OfferedAttachments;
};
