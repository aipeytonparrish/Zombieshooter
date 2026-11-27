#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ZSInteractable.h"
#include "ZSStation.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;
class UPointLightComponent;
class USkeletalMeshComponent;
class UCameraComponent;

UENUM(BlueprintType)
enum class EZSStationType : uint8
{
	MapTable,     // level select + deploy
	Armory,       // inventory, loadouts, attachments
	Market,       // daily rotating shop
	Character,    // character creator
	Missions      // dailies, weeklies, story
};

/** A bunker station: walk up, press Interact, its menu opens (UZSBunkerMenus). */
UCLASS()
class ZOMBIESHOOTER_API AZSStation : public AActor, public IZSInteractable
{
	GENERATED_BODY()

public:
	AZSStation();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Station")
	EZSStationType Type = EZSStationType::MapTable;

	virtual FText GetInteractPrompt() const override;
	virtual void Interact(AZSPlayerCharacter* Player) override;

	static FText StationName(EZSStationType Type);

	/** Locker: the camera the character menu looks through. */
	UCameraComponent* GetPreviewCamera() const { return PreviewCamera; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void ApplyVisuals();
	/** Locker: show the profile's survivor body. */
	void RefreshPreview();
	FDelegateHandle LookChangedHandle;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Station") TObjectPtr<UBoxComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category = "ZS|Station") TObjectPtr<UStaticMeshComponent> Table;
	UPROPERTY(VisibleAnywhere, Category = "ZS|Station") TObjectPtr<UStaticMeshComponent> PropA;
	UPROPERTY(VisibleAnywhere, Category = "ZS|Station") TObjectPtr<UStaticMeshComponent> PropB;
	UPROPERTY(VisibleAnywhere, Category = "ZS|Station") TObjectPtr<UTextRenderComponent> Sign;
	UPROPERTY(VisibleAnywhere, Category = "ZS|Station") TObjectPtr<UPointLightComponent> Lamp;
	UPROPERTY(VisibleAnywhere, Category = "ZS|Station") TObjectPtr<USkeletalMeshComponent> Preview;
	UPROPERTY(VisibleAnywhere, Category = "ZS|Station") TObjectPtr<UCameraComponent> PreviewCamera;
};
