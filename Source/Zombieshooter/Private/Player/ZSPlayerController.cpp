#include "Player/ZSPlayerController.h"
#include "Input/ZSInputSet.h"
#include "Core/ZSGameUserSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Weapons/ZSWeaponComponent.h"
#include "Player/ZSPlayerCharacter.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
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

void AZSPlayerController::ZSGiveAmmo()
{
	if (const AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn()))
	{
		if (UZSWeaponComponent* Weapons = Char->GetWeaponComponent())
		{
			Weapons->RefillAllAmmo();
		}
	}
}

void AZSPlayerController::ZSAimAssist(float Strength)
{
	UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	Settings->AimAssistStrength = FMath::Clamp(Strength, 0.f, 1.5f);
	Settings->bAimAssist = Strength > 0.f;
	SaveSettings();
}

void AZSPlayerController::ZSDamageNumbers(bool bEnabled)
{
	UZSGameUserSettings::Get()->bShowDamageNumbers = bEnabled;
	SaveSettings();
}

void AZSPlayerController::ZSRange()
{
	if (APawn* P = GetPawn())
	{
		P->TeleportTo(FVector(-250.f, 5650.f, 100.f), FRotator::ZeroRotator);
		SetControlRotation(FRotator::ZeroRotator);
	}
}

void AZSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	StartAutoShotIfRequested();
}

// Dev harness so visuals can be checked without a person at the keyboard:
//   -ZSAutoShot=3      take a screenshot 3 s after spawning, then quit
//   -ZSAutoRange       teleport to the shooting range first
//   -ZSAutoADS         hold aim-down-sights        -ZSAutoFire   hold the trigger
//   -ZSAutoWeapon=1    switch to weapon slot 1     -ZSAutoPitch=-10 / -ZSAutoYaw=20  look offset
void AZSPlayerController::StartAutoShotIfRequested()
{
#if !UE_BUILD_SHIPPING
	float Delay = 0.f;
	if (!IsLocalPlayerController() || !FParse::Value(FCommandLine::Get(), TEXT("ZSAutoShot="), Delay))
	{
		return;
	}
	const TCHAR* Cmd = FCommandLine::Get();
	const bool bRange = FParse::Param(Cmd, TEXT("ZSAutoRange"));
	const bool bADS = FParse::Param(Cmd, TEXT("ZSAutoADS"));
	const bool bFire = FParse::Param(Cmd, TEXT("ZSAutoFire"));
	if (FParse::Param(Cmd, TEXT("ZSAutoArmsDebug")))
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("zs.ArmsDebug")))
		{
			Var->Set(FParse::Param(FCommandLine::Get(), TEXT("ZSAutoArmsTest")) ? 2 : 1);
		}
	}
	int32 WeaponSlot = 0;
	FParse::Value(Cmd, TEXT("ZSAutoWeapon="), WeaponSlot);
	float Pitch = 0.f, Yaw = 0.f;
	FParse::Value(Cmd, TEXT("ZSAutoPitch="), Pitch);
	FParse::Value(Cmd, TEXT("ZSAutoYaw="), Yaw);

	FTimerHandle Setup;
	GetWorldTimerManager().SetTimer(Setup, FTimerDelegate::CreateWeakLambda(this, [this, bRange, bADS, bFire, WeaponSlot, Pitch, Yaw]()
	{
		if (bRange)
		{
			ZSRange();
		}
		SetControlRotation(FRotator(Pitch, Yaw, 0.f) + (bRange ? FRotator::ZeroRotator : GetControlRotation()));
		if (const AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn()))
		{
			if (UZSWeaponComponent* Weapons = Char->GetWeaponComponent())
			{
				for (int32 i = 0; i < WeaponSlot; ++i)
				{
					Weapons->RequestSwitchWeapon();
				}
				Weapons->SetADSHeld(bADS);
				Weapons->SetTriggerHeld(bFire);
			}
		}
	}), 0.5f, false);

	FTimerHandle Shot;
	GetWorldTimerManager().SetTimer(Shot, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ConsoleCommand(TEXT("HighResShot 1"));
	}), FMath::Max(Delay, 1.f), false);

	FTimerHandle Quit;
	GetWorldTimerManager().SetTimer(Quit, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ConsoleCommand(TEXT("quit"));
	}), FMath::Max(Delay, 1.f) + 1.5f, false);
#endif
}
