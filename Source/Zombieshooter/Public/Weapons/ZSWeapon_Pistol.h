#pragma once

#include "CoreMinimal.h"
#include "Weapons/ZSWeaponBase.h"
#include "ZSWeapon_Pistol.generated.h"

/**
 * Sidearm. No pistol mesh has been supplied yet (see Docs/AssetRequests.md), so it is built from
 * engine shapes. Swap Stats.Parts for imported meshes when one arrives.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSWeapon_Pistol : public AZSWeaponBase
{
	GENERATED_BODY()

public:
	AZSWeapon_Pistol();
};
