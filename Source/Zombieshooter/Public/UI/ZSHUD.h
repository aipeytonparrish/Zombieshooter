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

	/** Short message near the bottom of the screen ("AMMO REFILLED", "+ARMOR PLATE"). */
	static void Toast(const APawn* Player, const FText& Message);
	void AddToast(const FText& Message);

private:
	void DrawSurvival(const class AZSGameState_Survival* GS, const class AZSPlayerCharacter* Char, float Scale);
	void DrawInteractPrompt(const AZSPlayerCharacter* Char, float Scale);
	void DrawToasts(float Scale);
	void DrawRunEconomy(const AZSGameState_Survival* GS, float Scale);
	void DrawResults(const AZSGameState_Survival* GS, float Scale);
	void DrawPauseMenu(float Scale);
	void DrawCentered(const FString& Text, float Y, class UFont* Font, float TextScale, const FLinearColor& Color);

	struct FToast
	{
		FText Text;
		float Time = 0.f;
	};
	TArray<FToast> Toasts;

	void DrawCrosshair(class UZSWeaponComponent* Weapons, float Scale);
	void DrawHitMarkers(UZSWeaponComponent* Weapons, float Scale);
	void DrawDamageNumbers(UZSWeaponComponent* Weapons, float Scale);
	void DrawAmmo(UZSWeaponComponent* Weapons, float Scale);
	void DrawHealth(const class AZSPlayerCharacter* Char, float Scale);
};
