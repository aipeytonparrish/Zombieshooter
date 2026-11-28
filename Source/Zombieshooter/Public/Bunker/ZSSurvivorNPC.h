#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSSurvivorNPC.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UTextRenderComponent;

/**
 * A fellow survivor living at the safe house: one of the Survivors Mega Pack bodies, idling with a life-like loop from
 * the MC Idles pack (warming hands at the fire, smoking, leaning on a wall...), a name tag when you're close, and a
 * line or two when you walk up to them.
 *
 * The idle pack is on its own UE5 mannequin skeleton, so a hidden mannequin plays the animation and the survivor body
 * copies its pose bone by bone (leader pose). bOwnIdle uses the body's own plain idle instead.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSSurvivorNPC : public AActor
{
	GENERATED_BODY()

public:
	AZSSurvivorNPC();

	/** Survivors Mega Pack body 1-5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor", meta = (ClampMin = "1", ClampMax = "5"))
	int32 Body = 1;

	/** MC Idles pack loop, e.g. /Game/MC_Idles_Pack/Animations/Cold/am_Stand_Idle_Cold_Loop_01. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor")
	FString IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor")
	bool bOwnIdle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor")
	FText SurvivorName;

	/** Shown under the name ("QUARTERMASTER"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor")
	FText Job;

	/** One of these when the player walks up (at most every GreetCooldown seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor")
	TArray<FText> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor")
	float GreetRadius = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor")
	float GreetCooldown = 40.f;

	/** Playback rate jitter so a crowd doesn't move in lockstep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Survivor")
	float PlayRate = 1.f;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void ApplyLook();

	UPROPERTY(VisibleAnywhere, Category = "ZS|Survivor")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Survivor")
	TObjectPtr<USkeletalMeshComponent> Driver;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Survivor")
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Survivor")
	TObjectPtr<UTextRenderComponent> NameTag;

	double LastGreetTime = -1000.0;
	bool bPlayerWasNear = false;
	int32 NextLine = 0;
};
