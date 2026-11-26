#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
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

protected:
	virtual void SetupInputComponent() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UZSInputSet> InputSet;

	bool bShowMovementDebug = !UE_BUILD_SHIPPING;

	void SaveSettings();
};
