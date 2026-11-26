#pragma once

#include "CoreMinimal.h"
#include "Weapons/ZSWeaponBase.h"
#include "ZSWeapon_TacticalAR.generated.h"

/**
 * Assault rifle with full first-person animation: the owner's InfimaGames "Tactical FPS Animations" pack
 * (arms clips, modular AR mesh, AR audio, muzzle flash). Falls back to placeholder boxes if the pack is missing.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSWeapon_TacticalAR : public AZSWeaponBase
{
	GENERATED_BODY()

public:
	AZSWeapon_TacticalAR();
};
