#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/ZSMenu.h"
#include "ZSPlayerController.generated.h"

class UZSInputSet;

/**
 * Player controller: owns the C++-built input set and registers the gameplay mapping context.
 * Also hosts developer console commands for tuning (open the console with ` / ~ on keyboard).
 */
UCLASS()
class ZOMBIESHOOTER_API AZSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AZSPlayerController();

	/** Lazily builds and returns all input actions / contexts. */
	UZSInputSet* GetInputSet();

	bool IsMovementDebugVisible() const { return bShowMovementDebug; }

	// ---- Menus (drawn by AZSHUD) ----
	/** Push a menu page (stations, pause). While any page is open the pawn ignores gameplay input. */
	void PushMenu(const FZSMenuPage& Page);
	void PopMenu();
	void CloseAllMenus();
	bool IsMenuOpen() const { return MenuStack.Num() > 0; }
	const FZSMenuPage* GetTopMenu() const { return MenuStack.Num() > 0 ? &MenuStack.Last() : nullptr; }
	void OpenPauseMenu();
	/** Kept for the HUD / harness. */
	bool IsPauseMenuOpen() const { return MenuStack.Num() > 0 && MenuStack[0].bPausesGame; }
	void SetPauseMenuOpen(bool bOpen) { if (bOpen) { OpenPauseMenu(); } else { CloseAllMenus(); } }
	void ToggleMovementDebug() { bShowMovementDebug = !bShowMovementDebug; }

	// ---- Console commands (type in the ~ console) ----

	/** ZSFov 95  -> sets field of view (70-110) and saves it. */
	UFUNCTION(Exec)
	void ZSFov(float NewFOV);

	/** ZSSens 1.2 0.8  -> gamepad look sensitivity, mouse sensitivity. */
	UFUNCTION(Exec)
	void ZSSens(float GamepadSensitivity, float MouseSensitivity);

	/** ZSDeadZone 0.1 0.12 0.96 -> look inner, move inner, outer. */
	UFUNCTION(Exec)
	void ZSDeadZone(float LookDeadZone, float MoveDeadZone, float OuterDeadZone);

	/** ZSCurve 2 -> look response exponent (1 linear .. 4 precise). */
	UFUNCTION(Exec)
	void ZSCurve(float Exponent);

	/** ZSHeadBob 0/1 */
	UFUNCTION(Exec)
	void ZSHeadBob(bool bEnabled);

	/** ZSInvertY 0/1 -> inverts both gamepad and mouse Y. */
	UFUNCTION(Exec)
	void ZSInvertY(bool bInvert);

	/** ZSToggleSprint 0/1 -> controller L3 toggles sprint (1) or must be held (0). */
	UFUNCTION(Exec)
	void ZSToggleSprint(bool bToggle);

	/** ZSToggleCrouch 0/1 */
	UFUNCTION(Exec)
	void ZSToggleCrouch(bool bToggle);

	/** ZSMoveDebug -> toggles the on-screen movement debug readout. */
	UFUNCTION(Exec)
	void ZSMoveDebug();

	/** ZSGiveAmmo -> refills every carried weapon. */
	UFUNCTION(Exec)
	void ZSGiveAmmo();

	/** ZSAimAssist 1 -> controller aim-assist strength (0 = off, 1 = default, 1.5 = strong). */
	UFUNCTION(Exec)
	void ZSAimAssist(float Strength);

	/** ZSDamageNumbers 0/1 */
	UFUNCTION(Exec)
	void ZSDamageNumbers(bool bEnabled);

	/** ZSRange -> teleports to the shooting range firing line (movement course). */
	UFUNCTION(Exec)
	void ZSRange();

	/** ZSZombies 10 2 55 -> spawn N zombies 12-25 m ahead. Type: 0 = walkers, 1 = runners, 2 = mix. Spread = +/- degrees around your aim. */
	UFUNCTION(Exec)
	void ZSZombies(int32 Count = 8, int32 Type = 2, float SpreadDegrees = 55.f);

	/** ZSKillZombies -> removes every zombie. */
	UFUNCTION(Exec)
	void ZSKillZombies();

	/** ZSRound 10 -> survival mode: jump to round N. */
	UFUNCTION(Exec)
	void ZSRound(int32 Round);

	/** ZSZombieInfo -> logs every zombie: distance, state, speed, nav path or direct steering. */
	UFUNCTION(Exec)
	void ZSZombieInfo();

	/** ZSProfile -> logs level, XP, cash, weapons and lifetime stats. */
	UFUNCTION(Exec)
	void ZSProfile();

	/** ZSResetProfile -> wipes the save (level 1, no cash, starter weapons). */
	UFUNCTION(Exec)
	void ZSResetProfile();

	/** ZSGiveCash 5000 / ZSGiveXP 10000 */
	UFUNCTION(Exec)
	void ZSGiveCash(int32 Amount);

	UFUNCTION(Exec)
	void ZSGiveXP(int32 Amount);

	/** ZSWinRun -> survival: end the run as a win now (testing). */
	UFUNCTION(Exec)
	void ZSWinRun();

	/** Logs distances to walls in 16 directions and the floor height around the player (level layout work). */
	UFUNCTION(Exec)
	void ZSProbe();

	/** Over-the-shoulder camera experiment (Milestone 9). */
	UFUNCTION(Exec)
	void ZSThirdPerson();

	/** ZSGod -> toggles player invulnerability (testing). */
	UFUNCTION(Exec)
	void ZSGod();

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UZSInputSet> InputSet;

	bool bShowMovementDebug = !UE_BUILD_SHIPPING;

	void SaveSettings();

	/** Dev-only automated screenshot run, driven by -ZSAutoShot=<seconds> on the command line (see .cpp). */
	void StartAutoShotIfRequested();

	void Input_Pause(const struct FInputActionValue& Value);
	void Input_MenuConfirm(const FInputActionValue& Value);
	void Input_MenuBack(const FInputActionValue& Value);
	void Input_MenuUp(const FInputActionValue& Value);
	void Input_MenuDown(const FInputActionValue& Value);
	void ApplyMenuInputMode();

	TArray<FZSMenuPage> MenuStack;
	bool bConfirmAbandon = false;
};
