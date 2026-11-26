#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZSSpawnPoint.generated.h"

/**
 * Where zombies may enter. Place many around a level (behind corners, in alleys, at windows). The round manager
 * only uses points that are out of the player's view and within its distance band.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AZSSpawnPoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Spawn")
	bool bEnabled = true;

	/** Allow runners from this point (e.g. disable for windows they would sprint out of awkwardly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Spawn")
	bool bAllowRunners = true;
};
