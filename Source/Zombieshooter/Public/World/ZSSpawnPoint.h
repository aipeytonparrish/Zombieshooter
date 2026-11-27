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

	/** Area this point belongs to. None = always open; otherwise it is used only once that zone has been opened (a
	 *  bought door, see AZSBuyDoor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Spawn")
	FName Zone = NAME_None;

	/** Closest the player may be for this point to be used (cm); < 0 = the round manager's MinSpawnDistance. Small for
	 *  breach points inside a closed start room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZS|Spawn")
	float MinPlayerDistance = -1.f;
};
