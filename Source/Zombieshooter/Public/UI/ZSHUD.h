#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ZSHUD.generated.h"

/**
 * Milestone 3 gameplay HUD drawn with the canvas: dynamic crosshair (shows spread), hit markers
 * (white = hit, red = kill, gold tick = headshot), optional damage numbers, ammo / fire mode / weapon name,
 * and a reload bar. Sized for a TV at couch distance. The CommonUI HUD replaces it in a later milestone.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCrosshair(class UZSWeaponComponent* Weapons, float Scale);
	void DrawHitMarkers(UZSWeaponComponent* Weapons, float Scale);
	void DrawDamageNumbers(UZSWeaponComponent* Weapons, float Scale);
	void DrawAmmo(UZSWeaponComponent* Weapons, float Scale);
};
