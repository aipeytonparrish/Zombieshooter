#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InputActionValue.h"
#include "ZSInputSet.generated.h"

class UInputAction;
class UInputMappingContext;
struct FKey;

/**
 * Builds every Enhanced Input action and the default mapping contexts in C++, so the project needs
 * no input .uasset files. Gamepad bindings are the primary layout; keyboard + mouse mirror them.
 *
 * Default controller layout (Xbox naming):
 *   Left stick  move          Right stick  look
 *   L3          sprint        R3           melee
 *   A           jump / double jump / mantle
 *   B           crouch / slide (tap while sprinting)
 *   X           reload (tap) / interact
 *   Y           switch weapon
 *   LB          dash          RB           lethal (grenade)
 *   LT          aim (ADS)     RT           fire
 *   D-pad Left  tactical      D-pad Down   movement debug overlay (dev)
 *   Start       pause menu
 *
 * Full rebinding for both devices arrives with the settings menu (Milestone 14).
 */
UCLASS()
class ZOMBIESHOOTER_API UZSInputSet : public UObject
{
	GENERATED_BODY()

public:
	/** Creates all actions and contexts. Safe to call more than once (only builds the first time). */
	void Build();

	bool IsBuilt() const { return GameplayContext != nullptr; }

	/** Gameplay context: movement, combat, interaction. Used in the bunker and in matches. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input")
	TObjectPtr<UInputMappingContext> GameplayContext;

	// ---- Movement ----
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Move;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> LookMouse;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> LookGamepad;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Jump;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> CrouchSlide;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> SprintGamepad;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> SprintKeyboard;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Dash;

	// ---- Combat (handled from Milestone 3) ----
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Fire;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> AimDownSights;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Reload;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Interact;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> SwitchWeapon;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Melee;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Lethal;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Tactical;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> FireMode;

	// ---- Menus / debug ----
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> Pause;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZS|Input") TObjectPtr<UInputAction> ToggleMovementDebug;

private:
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType ValueType);
	void MapKey(UInputAction* Action, const FKey& Key);
	void MapWASD(UInputAction* Action);
};
