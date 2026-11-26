#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ZSZombieAIController.generated.h"

/**
 * Thin AI controller: path following toward the player. Decision-making lives on AZSZombieBase for v1; it moves
 * into StateTree tasks when the special zombies (Milestone 11) need richer behaviour.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSZombieAIController : public AAIController
{
	GENERATED_BODY()

public:
	AZSZombieAIController();

	/** Path toward Goal. Returns false if there is no navigation path (caller then steers directly). */
	bool ChaseActor(AActor* Goal, float AcceptanceRadius);
};
