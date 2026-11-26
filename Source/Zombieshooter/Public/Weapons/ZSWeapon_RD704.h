#pragma once

#include "CoreMinimal.h"
#include "Weapons/ZSWeaponBase.h"
#include "ZSWeapon_RD704.generated.h"

/**
 * Assault rifle: the owner's modular RD-704 AK (imported by Tools/Import/import_rd704.py into
 * /Game/ZS_Imported/Weapons/RD704). Falls back to grey placeholder boxes if the import hasn't been run.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSWeapon_RD704 : public AZSWeaponBase
{
	GENERATED_BODY()

public:
	AZSWeapon_RD704();
};
