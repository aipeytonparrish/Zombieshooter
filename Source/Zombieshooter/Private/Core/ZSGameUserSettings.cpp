#include "Core/ZSGameUserSettings.h"
#include "Engine/Engine.h"

UZSGameUserSettings::UZSGameUserSettings()
{
	FieldOfView = 80.f;
	bHeadBob = true;
	CameraEffectsScale = 1.f;

	GamepadLookSensitivity = 1.f;
	GamepadADSSensitivity = 0.8f;
	GamepadLookDeadZone = 0.10f;
	GamepadMoveDeadZone = 0.12f;
	GamepadOuterDeadZone = 0.96f;
	GamepadResponseExponent = 2.f;
	bGamepadLookAcceleration = true;
	bInvertGamepadY = false;
	bToggleSprintGamepad = true;

	MouseSensitivity = 1.f;
	MouseADSSensitivity = 0.85f;
	bInvertMouseY = false;
	bToggleSprintKeyboard = false;

	bToggleCrouch = true;
}

UZSGameUserSettings* UZSGameUserSettings::Get()
{
	if (GEngine)
	{
		if (UZSGameUserSettings* Settings = Cast<UZSGameUserSettings>(GEngine->GetGameUserSettings()))
		{
			return Settings;
		}
	}
	return GetMutableDefault<UZSGameUserSettings>();
}

void UZSGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	const UZSGameUserSettings* Defaults = GetDefault<UZSGameUserSettings>();
	FieldOfView = Defaults->FieldOfView;
	bHeadBob = Defaults->bHeadBob;
	CameraEffectsScale = Defaults->CameraEffectsScale;
	GamepadLookSensitivity = Defaults->GamepadLookSensitivity;
	GamepadADSSensitivity = Defaults->GamepadADSSensitivity;
	GamepadLookDeadZone = Defaults->GamepadLookDeadZone;
	GamepadMoveDeadZone = Defaults->GamepadMoveDeadZone;
	GamepadOuterDeadZone = Defaults->GamepadOuterDeadZone;
	GamepadResponseExponent = Defaults->GamepadResponseExponent;
	bGamepadLookAcceleration = Defaults->bGamepadLookAcceleration;
	bInvertGamepadY = Defaults->bInvertGamepadY;
	bToggleSprintGamepad = Defaults->bToggleSprintGamepad;
	MouseSensitivity = Defaults->MouseSensitivity;
	MouseADSSensitivity = Defaults->MouseADSSensitivity;
	bInvertMouseY = Defaults->bInvertMouseY;
	bToggleSprintKeyboard = Defaults->bToggleSprintKeyboard;
	bToggleCrouch = Defaults->bToggleCrouch;
}

void UZSGameUserSettings::SetFieldOfView(float InFOV)
{
	FieldOfView = FMath::Clamp(InFOV, MinFieldOfView, MaxFieldOfView);
}

void UZSGameUserSettings::SetGamepadLookSensitivity(float InValue)
{
	GamepadLookSensitivity = FMath::Clamp(InValue, 0.1f, 5.f);
}

void UZSGameUserSettings::SetMouseSensitivity(float InValue)
{
	MouseSensitivity = FMath::Clamp(InValue, 0.05f, 10.f);
}

void UZSGameUserSettings::SetGamepadDeadZones(float InLookDeadZone, float InMoveDeadZone, float InOuterDeadZone)
{
	GamepadLookDeadZone = FMath::Clamp(InLookDeadZone, 0.f, 0.5f);
	GamepadMoveDeadZone = FMath::Clamp(InMoveDeadZone, 0.f, 0.5f);
	GamepadOuterDeadZone = FMath::Clamp(InOuterDeadZone, 0.7f, 1.f);
}

void UZSGameUserSettings::SetGamepadResponseExponent(float InExponent)
{
	GamepadResponseExponent = FMath::Clamp(InExponent, 1.f, 4.f);
}
