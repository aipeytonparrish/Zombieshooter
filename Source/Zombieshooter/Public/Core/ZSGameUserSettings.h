#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ZSGameUserSettings.generated.h"

/**
 * Player-facing settings, saved to GameUserSettings.ini.
 * Registered in Config/DefaultEngine.ini via GameUserSettingsClassName.
 * The settings menu (Milestone 14) edits these; gameplay code reads them live every frame,
 * so changes apply immediately without restarting.
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class ZOMBIESHOOTER_API UZSGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UZSGameUserSettings();

	/** Returns the active settings object (never null; falls back to the class default). */
	static UZSGameUserSettings* Get();

	virtual void SetToDefaults() override;

	// ---- Camera ----------------------------------------------------------------------------

	static constexpr float MinFieldOfView = 70.f;
	static constexpr float MaxFieldOfView = 110.f;

	/** Horizontal field of view in degrees (70-110). */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Camera")
	float FieldOfView;

	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Camera")
	bool bHeadBob;

	/** Scales all camera tilt / dip / FOV-kick effects (0 = off, 1 = full). */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Camera")
	float CameraEffectsScale;

	// ---- Controller ------------------------------------------------------------------------

	/** Multiplier on the base look rate of the right stick. */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	float GamepadLookSensitivity;

	/** Multiplier while aiming down sights (used from Milestone 3). */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	float GamepadADSSensitivity;

	/** Radial inner dead zone of the right stick (0-0.5). */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	float GamepadLookDeadZone;

	/** Radial inner dead zone of the left stick (0-0.5). */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	float GamepadMoveDeadZone;

	/** Stick magnitude treated as full deflection (0.7-1). */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	float GamepadOuterDeadZone;

	/** Look response curve exponent. 1 = linear, 2 = classic, 3 = very precise near centre. */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	float GamepadResponseExponent;

	/** Extra turn speed after holding the stick at full deflection (console-style look acceleration). */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	bool bGamepadLookAcceleration;

	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	bool bInvertGamepadY;

	/** Tap L3 to sprint until you stop (true) or hold L3 (false). */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	bool bToggleSprintGamepad;

	// ---- Mouse & keyboard ------------------------------------------------------------------

	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Mouse")
	float MouseSensitivity;

	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Mouse")
	float MouseADSSensitivity;

	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Mouse")
	bool bInvertMouseY;

	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Mouse")
	bool bToggleSprintKeyboard;

	// ---- Shared ----------------------------------------------------------------------------

	/** Crouch button toggles crouch (true) or must be held (false). Slide is always a tap while sprinting. */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Input")
	bool bToggleCrouch;

	/** Controller aim assist (slowdown near targets, ADS snap, light tracking). Never applies to the mouse. */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	bool bAimAssist;

	/** 0 = off .. 1 = default .. 1.5 = strong. */
	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|Controller")
	float AimAssistStrength;

	UPROPERTY(config, BlueprintReadOnly, Category = "ZS|HUD")
	bool bShowDamageNumbers;

	// ---- Setters (clamped) -----------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ZS|Settings")
	void SetFieldOfView(float InFOV);

	UFUNCTION(BlueprintCallable, Category = "ZS|Settings")
	void SetGamepadLookSensitivity(float InValue);

	UFUNCTION(BlueprintCallable, Category = "ZS|Settings")
	void SetMouseSensitivity(float InValue);

	UFUNCTION(BlueprintCallable, Category = "ZS|Settings")
	void SetGamepadDeadZones(float InLookDeadZone, float InMoveDeadZone, float InOuterDeadZone);

	UFUNCTION(BlueprintCallable, Category = "ZS|Settings")
	void SetGamepadResponseExponent(float InExponent);
};
