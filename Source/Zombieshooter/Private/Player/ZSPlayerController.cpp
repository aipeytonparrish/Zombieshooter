#include "Player/ZSPlayerController.h"
#include "Input/ZSInputSet.h"
#include "Core/ZSGameUserSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Zombieshooter.h"

AZSPlayerController::AZSPlayerController()
{
}

UZSInputSet* AZSPlayerController::GetInputSet()
{
	if (!InputSet)
	{
		InputSet = NewObject<UZSInputSet>(this, TEXT("ZSInputSet"));
		InputSet->Build();
	}
	return InputSet;
}

void AZSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(GetInputSet()->GameplayContext, 0);
	}
	else
	{
		UE_LOG(LogZS, Warning, TEXT("Enhanced Input subsystem missing - is the EnhancedInput plugin enabled?"));
	}
}

void AZSPlayerController::SaveSettings()
{
	UZSGameUserSettings::Get()->SaveSettings();
}

void AZSPlayerController::ZSFov(float NewFOV)
{
	UZSGameUserSettings::Get()->SetFieldOfView(NewFOV);
	SaveSettings();
}

void AZSPlayerController::ZSSens(float GamepadSensitivity, float MouseSensitivity)
{
	UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	Settings->SetGamepadLookSensitivity(GamepadSensitivity);
	Settings->SetMouseSensitivity(MouseSensitivity);
	SaveSettings();
}

void AZSPlayerController::ZSDeadZone(float LookDeadZone, float MoveDeadZone, float OuterDeadZone)
{
	UZSGameUserSettings::Get()->SetGamepadDeadZones(LookDeadZone, MoveDeadZone, OuterDeadZone);
	SaveSettings();
}

void AZSPlayerController::ZSCurve(float Exponent)
{
	UZSGameUserSettings::Get()->SetGamepadResponseExponent(Exponent);
	SaveSettings();
}

void AZSPlayerController::ZSHeadBob(bool bEnabled)
{
	UZSGameUserSettings::Get()->bHeadBob = bEnabled;
	SaveSettings();
}

void AZSPlayerController::ZSInvertY(bool bInvert)
{
	UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	Settings->bInvertGamepadY = bInvert;
	Settings->bInvertMouseY = bInvert;
	SaveSettings();
}

void AZSPlayerController::ZSToggleSprint(bool bToggle)
{
	UZSGameUserSettings::Get()->bToggleSprintGamepad = bToggle;
	SaveSettings();
}

void AZSPlayerController::ZSToggleCrouch(bool bToggle)
{
	UZSGameUserSettings::Get()->bToggleCrouch = bToggle;
	SaveSettings();
}

void AZSPlayerController::ZSMoveDebug()
{
	ToggleMovementDebug();
}
