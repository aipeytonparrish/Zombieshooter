#include "Player/ZSPlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Input/ZSInputSet.h"
#include "Core/ZSGameUserSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Weapons/ZSWeaponComponent.h"
#include "Player/ZSPlayerCharacter.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Zombies/ZSZombieBase.h"
#include "Player/ZSCameraComponent.h"
#include "Movement/ZSMovementComponent.h"
#include "World/ZSWallBuy.h"
#include "World/ZSMysteryBox.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "Rounds/ZSGameMode_Survival.h"
#include "Rounds/ZSRoundManagerComponent.h"
#include "Profile/ZSProfileSubsystem.h"
#include "Rounds/ZSGameState_Survival.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Bunker/ZSBunkerMenus.h"
#include "Engine/DamageEvents.h"
#include "Rounds/ZSRunTaskComponent.h"
#include "Combat/ZSHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Zombieshooter.h"
#include "World/ZSBuyDoor.h"

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
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		const UZSInputSet* Set = GetInputSet();
		EIC->BindAction(Set->Pause, ETriggerEvent::Started, this, &ThisClass::Input_Pause);
		EIC->BindAction(Set->MenuConfirm, ETriggerEvent::Started, this, &ThisClass::Input_MenuConfirm);
		EIC->BindAction(Set->MenuBack, ETriggerEvent::Started, this, &ThisClass::Input_MenuBack);
		EIC->BindAction(Set->MenuUp, ETriggerEvent::Started, this, &ThisClass::Input_MenuUp);
		EIC->BindAction(Set->MenuDown, ETriggerEvent::Started, this, &ThisClass::Input_MenuDown);
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
	const bool bRange = FParse::Param(Cmd, TEXT("ZSAutoRange")) && !FParse::Param(Cmd, TEXT("ZSAutoNoRange"));
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
	int32 AutoZombies = 0, AutoZombieType = 2;
	FParse::Value(Cmd, TEXT("ZSAutoZombies="), AutoZombies);
	FParse::Value(Cmd, TEXT("ZSAutoZombieType="), AutoZombieType);
	const bool bGod = FParse::Param(Cmd, TEXT("ZSAutoGod"));
	const bool bBursts = FParse::Param(Cmd, TEXT("ZSAutoBursts"));
	if (FParse::Param(Cmd, TEXT("ZSAutoKillAll")))
	{
		FTimerHandle KillTimer;
		GetWorldTimerManager().SetTimer(KillTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { ZSKillZombies(); }), 2.f, true, 2.f);
	}
	// -ZSAutoExec=ZSRunCash+400|ZSUseDoors -ZSAutoExecAt=12: console commands at a given time ('+' for spaces).
	FString ExecStr;
	if (FParse::Value(Cmd, TEXT("ZSAutoExec="), ExecStr, false))
	{
		float ExecAt = 3.f;
		FParse::Value(Cmd, TEXT("ZSAutoExecAt="), ExecAt);
		FTimerHandle ExecTimer;
		GetWorldTimerManager().SetTimer(ExecTimer, FTimerDelegate::CreateWeakLambda(this, [this, ExecStr]()
		{
			TArray<FString> Lines;
			ExecStr.ParseIntoArray(Lines, TEXT("|"));
			for (const FString& L : Lines) { ConsoleCommand(L.Replace(TEXT("+"), TEXT(" ")).TrimStartAndEnd()); }   // '+' = space
		}), FMath::Max(0.1f, ExecAt), false);
	}
	FString TeleportStr;
	if (FParse::Value(Cmd, TEXT("ZSAutoTeleport="), TeleportStr, false))
	{
		TArray<FString> Parts;
		TeleportStr.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() == 3)
		{
			const FVector Where(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			FTimerHandle TeleportTimer;
			GetWorldTimerManager().SetTimer(TeleportTimer, FTimerDelegate::CreateWeakLambda(this, [this, Where]() { if (APawn* P = GetPawn()) { bool bOk = false; for (const float Up : { 0.f, 100.f, 250.f, 500.f }) { if ((bOk = P->TeleportTo(Where + FVector(0.f, 0.f, Up), P->GetActorRotation()))) { break; } } UE_LOG(LogZS, Warning, TEXT("Teleport %s -> %s"), bOk ? TEXT("ok") : TEXT("FAILED"), *P->GetActorLocation().ToCompactString()); } }), 0.7f, false);
		}
	}
	// -ZSAutoProbe="x,y,z;x,y,z": teleport to each point and log ZSProbe.
	FString ProbePts;
	if (FParse::Value(Cmd, TEXT("ZSAutoProbe="), ProbePts, false))
	{
		TArray<FString> Pts;
		ProbePts.ParseIntoArray(Pts, TEXT(";"));
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			TArray<FString> C;
			Pts[i].ParseIntoArray(C, TEXT(","));
			if (C.Num() != 3) { continue; }
			const FVector Loc(FCString::Atof(*C[0]), FCString::Atof(*C[1]), FCString::Atof(*C[2]));
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Loc]()
			{
				if (APawn* Me = GetPawn()) { Me->TeleportTo(Loc, Me->GetActorRotation()); }
				FTimerHandle H2;
				GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, [this]() { ZSProbe(); }), 0.3f, false);
			}), 1.5f + i * 0.6f, false);
		}
	}
	if (FParse::Param(Cmd, TEXT("ZSAutoThirdPerson")))
	{
		FTimerHandle TPTimer;
		GetWorldTimerManager().SetTimer(TPTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { ZSThirdPerson(); }), 1.0f, false);
	}
	// -ZSAutoHurt="bone,damage;bone,damage" one entry per second from 3 s (gore tests).
	FString AutoHurt;
	if (FParse::Value(Cmd, TEXT("ZSAutoHurt="), AutoHurt, false))
	{
		TArray<FString> Hits;
		AutoHurt.ParseIntoArray(Hits, TEXT(";"));
		for (int32 i = 0; i < Hits.Num(); ++i)
		{
			FString B, D;
			if (!Hits[i].Split(TEXT(","), &B, &D)) { continue; }
			const float Dmg = FCString::Atof(*D);
			FTimerHandle HurtTimer;
			GetWorldTimerManager().SetTimer(HurtTimer, FTimerDelegate::CreateWeakLambda(this, [this, B, Dmg]() { ZSHurtBone(B, Dmg); }), 3.f + i, false);
		}
	}
	// -ZSAutoFaceBoss=<seconds>: stand 10 m in front of the boss, looking at it (screenshots).
	float FaceBossAt = 0.f;
	if (FParse::Value(Cmd, TEXT("ZSAutoFaceBoss="), FaceBossAt))
	{
		FTimerHandle FaceTimer;
		GetWorldTimerManager().SetTimer(FaceTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
			{
				if (It->IsBoss() && GetPawn())
				{
					const FVector BossLoc = It->GetActorLocation();
					const FVector Spot = BossLoc + It->GetActorForwardVector() * 1000.f - FVector(0, 0, 100.f);
					GetPawn()->TeleportTo(Spot, GetPawn()->GetActorRotation());
					SetControlRotation((BossLoc + FVector(0, 0, 60.f) - (Spot + FVector(0, 0, 70.f))).Rotation());
					break;
				}
			}
		}), FaceBossAt, false);
	}
	if (FParse::Param(Cmd, TEXT("ZSAutoBoss")))
	{
		FTimerHandle BossTimer;
		GetWorldTimerManager().SetTimer(BossTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { ZSBoss(); }), 1.0f, false);
	}
	int32 AutoPowerUp = 0;
	if (FParse::Value(Cmd, TEXT("ZSAutoPowerUp="), AutoPowerUp))
	{
		FTimerHandle PUTimer;
		GetWorldTimerManager().SetTimer(PUTimer, FTimerDelegate::CreateWeakLambda(this, [this, AutoPowerUp]() { ZSPowerUp(AutoPowerUp); }), 2.5f, false);
	}
	int32 AutoTask = 0;
	if (FParse::Value(Cmd, TEXT("ZSAutoTask="), AutoTask))
	{
		FTimerHandle TaskTimer;
		GetWorldTimerManager().SetTimer(TaskTimer, FTimerDelegate::CreateWeakLambda(this, [this, AutoTask]() { ZSTask(AutoTask); }), 2.0f, false);
	}
	// Bunker menus: -ZSAutoMenu=Map|Armory|Market|Character|Missions [-ZSAutoMenuKeys=DDC] (D down, U up, C confirm, B back).
	int32 AutoCash = 0;
	if (FParse::Value(Cmd, TEXT("ZSAutoCash="), AutoCash)) { ZSGiveCash(AutoCash); }
	int32 AutoXP = 0;
	if (FParse::Value(Cmd, TEXT("ZSAutoXP="), AutoXP)) { ZSGiveXP(AutoXP); }
	FString AutoMenu;
	if (FParse::Value(Cmd, TEXT("ZSAutoMenu="), AutoMenu))
	{
		FString Keys;
		FParse::Value(Cmd, TEXT("ZSAutoMenuKeys="), Keys);
		FTimerHandle MenuTimer;
		GetWorldTimerManager().SetTimer(MenuTimer, FTimerDelegate::CreateWeakLambda(this, [this, AutoMenu, Keys]()
		{
			static const TMap<FString, EZSStationType> Names = { { TEXT("Map"), EZSStationType::MapTable }, { TEXT("Armory"), EZSStationType::Armory },
				{ TEXT("Market"), EZSStationType::Market }, { TEXT("Character"), EZSStationType::Character }, { TEXT("Missions"), EZSStationType::Missions } };
			if (const EZSStationType* T = Names.Find(AutoMenu)) { ZSBunkerMenus::Open(this, *T); }
			for (const TCHAR K : Keys)
			{
				const FInputActionValue V;
				switch (K)
				{
				case 'D': Input_MenuDown(V); break;
				case 'U': Input_MenuUp(V); break;
				case 'C': Input_MenuConfirm(V); break;
				case 'B': Input_MenuBack(V); break;
				default: break;
				}
			}
		}), 1.5f, false);
	}

	if (FParse::Param(Cmd, TEXT("ZSAutoWin")) || FParse::Param(Cmd, TEXT("ZSAutoAbandon")) || FParse::Param(Cmd, TEXT("ZSAutoPause")))
	{
		const bool bWin = FParse::Param(Cmd, TEXT("ZSAutoWin"));
		const bool bAbandon = FParse::Param(Cmd, TEXT("ZSAutoAbandon"));
		FTimerHandle EndTimer;
		GetWorldTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateWeakLambda(this, [this, bWin, bAbandon]()
		{
			ZSProfile();
			if (bWin) { ZSWinRun(); }
			else if (bAbandon) { if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>()) { GM->AbandonRun(); } }
			else { OpenPauseMenu(); if (MenuStack.Num() > 0) { MenuStack.Last().Index = 1; } }
			ZSProfile();
		}), 2.f, false);
	}
	int32 AutoRound = 0;
	FParse::Value(Cmd, TEXT("ZSAutoRound="), AutoRound);
	if (AutoRound > 0)
	{
		FTimerHandle RoundTimer;
		GetWorldTimerManager().SetTimer(RoundTimer, FTimerDelegate::CreateWeakLambda(this, [this, AutoRound]() { ZSRound(AutoRound); }), 0.6f, false);
	}

	FTimerHandle Setup;
	GetWorldTimerManager().SetTimer(Setup, FTimerDelegate::CreateWeakLambda(this, [this, bRange, bADS, bFire, WeaponSlot, Pitch, Yaw, AutoZombies, AutoZombieType, bGod]()
	{
		if (bRange)
		{
			ZSRange();
		}
		if (bGod)
		{
			ZSGod();
		}
		if (AutoZombies > 0)
		{
			ZSZombies(AutoZombies, AutoZombieType, 6.f);
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

	if (bBursts)
	{
		// Short controlled bursts: press 0.18 s, release 0.3 s, repeat.
		FTimerHandle BurstTimer;
		GetWorldTimerManager().SetTimer(BurstTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn());
			if (UZSWeaponComponent* Weapons = Char ? Char->GetWeaponComponent() : nullptr)
			{
				Weapons->SetTriggerHeld(true);
				FTimerHandle Release;
				GetWorldTimerManager().SetTimer(Release, FTimerDelegate::CreateWeakLambda(this, [Weapons]() { Weapons->SetTriggerHeld(false); }), 0.18f, false);
			}
		}), 0.48f, true, 1.f);
	}

	// -ZSAutoSlide[=hold]: run forward with sprint on, tap crouch at 1.5 s (released 0.1 s later), log the movement every
	// 0.1 s. "hold" uses hold-to-crouch (the release is what used to cancel slides).
	FString SlideMode;
	if (FParse::Value(Cmd, TEXT("ZSAutoSlide="), SlideMode) || FParse::Param(Cmd, TEXT("ZSAutoSlide")))
	{
		const bool bToggle = SlideMode != TEXT("hold");
		const double T0 = GetWorld()->GetTimeSeconds();
		FTimerHandle SlideTimer;
		GetWorldTimerManager().SetTimer(SlideTimer, FTimerDelegate::CreateWeakLambda(this, [this, T0, bToggle]()
		{
			AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn());
			UZSMovementComponent* Move = Char ? Char->GetZSMovement() : nullptr;
			if (!Move) { return; }
			const double T = GetWorld()->GetTimeSeconds() - T0;
			if (T > 4.0) { return; }
			Char->AddMovementInput(FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector(), 1.f);
			if (T < 1.5 && Move->IsMovingOnGround()) { Move->Velocity = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector() * 780.f; }   // stand-in for a sprint
			Move->SetSprintInput(T < 3.5);
			static int32 Step = 0;
			if (T >= 1.5 && Step == 0) { Step = 1; Move->HandleCrouchPressed(bToggle); UE_LOG(LogZS, Warning, TEXT("AutoSlide: crouch pressed (%s)"), bToggle ? TEXT("toggle") : TEXT("hold")); }
			if (T >= 1.6 && Step == 1) { Step = 2; Move->HandleCrouchReleased(bToggle); UE_LOG(LogZS, Warning, TEXT("AutoSlide: crouch released")); }
			static double NextLog = 0.0;
			if (T >= NextLog && T > 1.0)
			{
				NextLog = T + 0.1;
				UE_LOG(LogZS, Warning, TEXT("AutoSlide t=%.2f ign=%d acc=%.0f maxspd=%.0f mode=%d custom=%d sliding=%d crouched=%d speed=%.0f"), T, Char->IsMoveInputIgnored() ? 1 : 0, Move->GetCurrentAcceleration().Size(), Move->GetMaxSpeed(), (int32)Move->MovementMode.GetValue(),
					(int32)Move->CustomMovementMode, Move->IsSliding() ? 1 : 0, Char->bIsCrouched ? 1 : 0, Move->Velocity.Size2D());
			}
		}), 0.01f, true);
	}

	// -ZSAutoPlay: soak test. Stats every 10 s (round, alive, kills, cash, fps), ammo topped up, doors bought as cash
	// allows. Combine with -ZSAutoAim -ZSAutoBursts -ZSAutoGod and a long -ZSAutoShot.
	if (FParse::Param(Cmd, TEXT("ZSAutoPlay")))
	{
		FTimerHandle StatsTimer;
		GetWorldTimerManager().SetTimer(StatsTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
			const AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>();
			const UZSRoundManagerComponent* RM = GM ? GM->GetRoundManager() : nullptr;
			const float Fps = 1.f / FMath::Max(0.0001f, GetWorld()->GetDeltaSeconds());
			const APawn* P = GetPawn();
			UE_LOG(LogZS, Warning, TEXT("AutoPlay t=%.0f round=%d left=%d alive=%d kills=%d cash=%d fps=%.0f at %s"), GetWorld()->GetTimeSeconds(),
				GS ? GS->Round : -1, GS ? GS->ZombiesRemaining : -1, RM ? RM->GetAliveCount() : -1, GS ? GS->Kills : -1, GS ? GS->RunCash : -1, Fps,
				P ? *P->GetActorLocation().ToCompactString() : TEXT("-"));
		}), 10.f, true);
		FTimerHandle UpkeepTimer;
		GetWorldTimerManager().SetTimer(UpkeepTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ZSGiveAmmo();
			ZSUseDoors();
		}), 20.f, true);
	}

	// -ZSAutoAim: keep the view on the nearest live zombie's chest (repeatable damage tests).
	if (FParse::Param(Cmd, TEXT("ZSAutoAim")))
	{
		FString AimBoneStr = TEXT("spine_02");
		FParse::Value(Cmd, TEXT("ZSAutoAimBone="), AimBoneStr);
		const FName AimBone(*AimBoneStr);
		FTimerHandle AimTimer;
		GetWorldTimerManager().SetTimer(AimTimer, FTimerDelegate::CreateWeakLambda(this, [this, AimBone]()
		{
			const AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn());
			if (!Char) { return; }
			const FVector Eye = Char->GetFirstPersonCamera()->GetComponentLocation();
			const AZSZombieBase* Best = nullptr;
			float BestD = 1.e9f;
			for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
			{
				const float D = FVector::Dist(It->GetActorLocation(), Eye);
				if (It->IsDead() || D >= BestD) { continue; }
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(ZSAutoAim), false, Char);
				const FVector Chest = It->GetMesh()->GetBoneLocation(AimBone);
				if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chest, ECC_Visibility, Q) && Hit.GetActor() != *It) { continue; }   // behind a wall
				Best = *It; BestD = D;
			}
			if (Best)
			{
				SetControlRotation((Best->GetMesh()->GetBoneLocation(AimBone) - Eye).Rotation());
			}
		}), 0.02f, true);
	}

	// Core ticker (not world timers) so the shot and quit still happen while the game is paused.
	TWeakObjectPtr<AZSPlayerController> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (AZSPlayerController* PC = WeakThis.Get()) { PC->ZSZombieInfo(); PC->ConsoleCommand(TEXT("HighResShot 1")); }
		return false;
	}), FMath::Max(Delay, 1.f));
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (AZSPlayerController* PC = WeakThis.Get()) { PC->ConsoleCommand(TEXT("quit")); }
		return false;
	}), FMath::Max(Delay, 1.f) + 1.5f);
#endif
}

void AZSPlayerController::ZSZombies(int32 Count, int32 Type, float SpreadDegrees)
{
	APawn* P = GetPawn();
	UWorld* World = GetWorld();
	if (!P || !World)
	{
		return;
	}
	const FVector Origin = P->GetActorLocation();
	const float BaseYaw = GetControlRotation().Yaw;
	int32 Spawned = 0;
	for (int32 i = 0; i < FMath::Clamp(Count, 1, 60); ++i)
	{
		const float Yaw = BaseYaw + FMath::FRandRange(-SpreadDegrees, SpreadDegrees);
		const float Dist = FMath::FRandRange(1200.f, 2500.f);
		const FVector Flat = Origin + FRotator(0.f, Yaw, 0.f).Vector() * Dist;

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSZombieSpawn), false, P);
		if (!World->LineTraceSingleByChannel(Hit, Flat + FVector(0.f, 0.f, 600.f), Flat - FVector(0.f, 0.f, 1500.f), ECC_Visibility, Params))
		{
			continue;
		}
		const FVector Loc = Hit.ImpactPoint + FVector(0.f, 0.f, 95.f);
		const FTransform T(FRotator(0.f, Yaw + 180.f, 0.f), Loc);
		if (AZSZombieBase* Z = World->SpawnActorDeferred<AZSZombieBase>(AZSZombieBase::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			// 0 walker, 1 runner, 2 mix, 3 crawler, 4 bloater, 5 spitter, 6 brute, 7 screamer, 8 all specials mixed
			Z->ZombieType = Type == 0 ? EZSZombieType::Walker : (Type == 1 ? EZSZombieType::Runner : (FMath::FRand() < 0.35f ? EZSZombieType::Runner : EZSZombieType::Walker));
			if (Type >= 3 && Type <= 7) { Z->ZombieType = static_cast<EZSZombieType>(static_cast<int32>(EZSZombieType::Crawler) + Type - 3); }
			if (Type == 8) { Z->ZombieType = static_cast<EZSZombieType>(static_cast<int32>(EZSZombieType::Crawler) + i % 5); }
			Z->FinishSpawning(T);
			++Spawned;
		}
	}
	UE_LOG(LogZS, Log, TEXT("Spawned %d zombies (%d alive)."), Spawned, AZSZombieBase::GetLiveCount());
}

void AZSPlayerController::ZSKillZombies()
{
	for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead())
		{
			UGameplayStatics::ApplyDamage(*It, 1.0e6f, this, GetPawn(), UDamageType::StaticClass());   // credited to the player (testing rewards)
		}
	}
}

void AZSPlayerController::ZSGod()
{
	if (APawn* P = GetPawn())
	{
		P->SetCanBeDamaged(!P->CanBeDamaged());
		UE_LOG(LogZS, Log, TEXT("God mode %s"), P->CanBeDamaged() ? TEXT("off") : TEXT("on"));
	}
}

void AZSPlayerController::ZSRound(int32 Round)
{
	if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
	{
		GM->GetRoundManager()->SkipToRound(Round);
	}
	else
	{
		UE_LOG(LogZS, Warning, TEXT("ZSRound only works in survival mode (?game=Survival or the L_ZSTest map)."));
	}
}

void AZSPlayerController::ZSZombieInfo()
{
	const APawn* P = GetPawn();
	int32 N = 0;
	for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
	{
		const AZSZombieBase* Z = *It;
		// Hit-box drift: where the spine's physics body is vs where the animated bone is drawn.
		float Drift = -1.f;
		if (const USkeletalMeshComponent* M = Z->GetMesh())
		{
			if (const FBodyInstance* BI = M->GetBodyInstance(TEXT("spine_02")))
			{
				Drift = FVector::Dist(BI->GetUnrealWorldTransform().GetLocation(), M->GetBoneLocation(TEXT("spine_02")));
			}
		}
		const UZSHealthComponent* HC = UZSHealthComponent::FindOn(Z);
		static TSet<FString> Dumped;
		if (Z->GetMesh() && Z->GetMesh()->GetPhysicsAsset() && !Dumped.Contains(GetNameSafe(Z->GetMesh()->GetSkeletalMeshAsset())) && (Dumped.Add(GetNameSafe(Z->GetMesh()->GetSkeletalMeshAsset())), true))
		{
			FString Bodies;
			for (const USkeletalBodySetup* BS : Z->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups)
			{
				if (BS) { Bodies += BS->BoneName.ToString() + TEXT(" "); }
			}
			UE_LOG(LogZS, Warning, TEXT("Zombie mesh %s bodies: %s"), *GetNameSafe(Z->GetMesh()->GetSkeletalMeshAsset()), *Bodies);
		}
		UE_LOG(LogZS, Warning, TEXT("Zombie %s: dist %.0f state %d speed %.0f %s hp %.0f drift %.0f at %s"), *Z->GetName(),
			P ? FVector::Dist(P->GetActorLocation(), Z->GetActorLocation()) : -1.f, (int32)Z->GetZombieState(),
			Z->GetVelocity().Size2D(), Z->IsDirectChasing() ? TEXT("DIRECT") : TEXT("NAV"), HC ? HC->GetHealth() : -1.f, Drift, *Z->GetActorLocation().ToCompactString());
		++N;
	}
	UE_LOG(LogZS, Warning, TEXT("%d zombies"), N);
}

// ---- Pause menu / results input --------------------------------------------------------------

void AZSPlayerController::PushMenu(const FZSMenuPage& Page)
{
	FZSMenuPage& P = MenuStack.Add_GetRef(Page);
	if (P.Refresh)
	{
		P.Refresh(P);
	}
	P.Index = FMath::Clamp(P.Index, 0, FMath::Max(0, P.Items.Num() - 1));
	ApplyMenuInputMode();
}

void AZSPlayerController::PopMenu()
{
	if (MenuStack.Num() > 0)
	{
		const TFunction<void()> Closed = MenuStack.Last().OnClosed;
		MenuStack.Pop();
		if (Closed) { Closed(); }
		if (MenuStack.Num() > 0 && MenuStack.Last().Refresh)
		{
			MenuStack.Last().Refresh(MenuStack.Last());
		}
	}
	ApplyMenuInputMode();
}

void AZSPlayerController::CloseAllMenus()
{
	TArray<TFunction<void()>> Closed;
	for (int32 i = MenuStack.Num() - 1; i >= 0; --i)
	{
		if (MenuStack[i].OnClosed) { Closed.Add(MenuStack[i].OnClosed); }
	}
	MenuStack.Reset();
	for (const TFunction<void()>& F : Closed) { F(); }
	bConfirmAbandon = false;
	ApplyMenuInputMode();
}

void AZSPlayerController::ApplyMenuInputMode()
{
	const bool bOpen = MenuStack.Num() > 0;
	const bool bPause = bOpen && MenuStack[0].bPausesGame;
	SetPause(bPause);
	if (APawn* P = GetPawn())
	{
		if (bOpen) { P->DisableInput(this); }
		else
		{
			const AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
			const bool bRunOver = GS && GS->bShowResults;
			if (!bRunOver) { P->EnableInput(this); }
		}
	}
}

void AZSPlayerController::OpenPauseMenu()
{
	if (IsMenuOpen())
	{
		return;
	}
	FZSMenuPage Page;
	Page.Title = NSLOCTEXT("ZS", "Paused", "PAUSED");
	Page.bPausesGame = true;
	TWeakObjectPtr<AZSPlayerController> WeakThis(this);
	Page.Refresh = [WeakThis](FZSMenuPage& P)
	{
		AZSPlayerController* PC = WeakThis.Get();
		if (!PC) { return; }
		P.Items.Reset();
		P.Items.Emplace(NSLOCTEXT("ZS", "Pause_Resume", "RESUME"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->CloseAllMenus(); } });
		AZSGameMode_Survival* GM = PC->GetWorld()->GetAuthGameMode<AZSGameMode_Survival>();
		if (GM && !GM->IsRunOver())
		{
			P.Items.Emplace(PC->bConfirmAbandon ? NSLOCTEXT("ZS", "Pause_AbandonConfirm", "CONFIRM ABANDON") : NSLOCTEXT("ZS", "Pause_Abandon", "ABANDON RUN"),
				[WeakThis]()
				{
					AZSPlayerController* Me = WeakThis.Get();
					if (!Me) { return; }
					if (!Me->bConfirmAbandon) { Me->bConfirmAbandon = true; return; }   // ask twice
					Me->CloseAllMenus();
					if (AZSGameMode_Survival* M = Me->GetWorld()->GetAuthGameMode<AZSGameMode_Survival>()) { M->AbandonRun(); }
				},
				FText::GetEmpty(), NSLOCTEXT("ZS", "Pause_AbandonDetail", "Counts as a failed run: your deployed gear and everything earned this run are lost."));
		}
		P.Items.Emplace(NSLOCTEXT("ZS", "Pause_Quit", "QUIT GAME"), [WeakThis]()
		{
			AZSPlayerController* Me = WeakThis.Get();
			if (!Me) { return; }
			// Quitting mid-run is a fail (same as the force-quit rule), so settle it first.
			if (AZSGameMode_Survival* M = Me->GetWorld()->GetAuthGameMode<AZSGameMode_Survival>(); M && !M->IsRunOver()) { M->AbandonRun(); }
			Me->ConsoleCommand(TEXT("quit"));
		});
	};
	bConfirmAbandon = false;
	PushMenu(Page);
}

void AZSPlayerController::Input_Pause(const FInputActionValue& Value)
{
	const AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
	if (GS && GS->bShowResults)
	{
		return;
	}
	if (IsMenuOpen()) { CloseAllMenus(); } else { OpenPauseMenu(); }
}

void AZSPlayerController::Input_MenuConfirm(const FInputActionValue& Value)
{
	// Results screen: continue.
	if (const AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>(); GS && GS->bShowResults && !IsMenuOpen())
	{
		if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
		{
			GM->RestartRun();
		}
		return;
	}
	if (MenuStack.Num() == 0)
	{
		return;
	}
	const int32 Depth = MenuStack.Num();
	FZSMenuPage& Top = MenuStack.Last();
	if (!Top.Items.IsValidIndex(Top.Index) || !Top.Items[Top.Index].bEnabled)
	{
		return;
	}
	const TFunction<void()> Action = Top.Items[Top.Index].OnSelect;   // copy: the callback may change the stack
	if (Action)
	{
		Action();
	}
	// Refresh the page that is now on top (if the callback didn't close / replace it).
	if (MenuStack.Num() > 0 && MenuStack.Num() <= Depth && MenuStack.Last().Refresh)
	{
		FZSMenuPage& Now = MenuStack.Last();
		Now.Refresh(Now);
		Now.Index = FMath::Clamp(Now.Index, 0, FMath::Max(0, Now.Items.Num() - 1));
	}
}

void AZSPlayerController::Input_MenuBack(const FInputActionValue& Value)
{
	if (bConfirmAbandon)
	{
		bConfirmAbandon = false;
		if (MenuStack.Num() > 0 && MenuStack.Last().Refresh) { MenuStack.Last().Refresh(MenuStack.Last()); }
		return;
	}
	if (MenuStack.Num() > 0)
	{
		PopMenu();
	}
}

void AZSPlayerController::Input_MenuUp(const FInputActionValue& Value)
{
	if (MenuStack.Num() > 0 && MenuStack.Last().Items.Num() > 0)
	{
		FZSMenuPage& P = MenuStack.Last();
		P.Index = (P.Index + P.Items.Num() - 1) % P.Items.Num();
		if (bConfirmAbandon) { bConfirmAbandon = false; if (P.Refresh) { P.Refresh(P); } }
	}
}

void AZSPlayerController::Input_MenuDown(const FInputActionValue& Value)
{
	if (MenuStack.Num() > 0 && MenuStack.Last().Items.Num() > 0)
	{
		FZSMenuPage& P = MenuStack.Last();
		P.Index = (P.Index + 1) % P.Items.Num();
		if (bConfirmAbandon) { bConfirmAbandon = false; if (P.Refresh) { P.Refresh(P); } }
	}
}

// ---- Profile debug --------------------------------------------------------------------------

void AZSPlayerController::ZSProfile()
{
	if (const UZSProfileSubsystem* P = UZSProfileSubsystem::Get(this))
	{
		FString Weapons;
		for (const FZSInventoryWeapon& W : P->GetWeapons())
		{
			Weapons += W.ItemId.ToString() + TEXT(" ");
		}
		const FZSLifetimeStats& S = P->GetStats();
		UE_LOG(LogZS, Warning, TEXT("Profile: level %d (%d / %d XP), $%lld, weapons [%s], runs %d wins %d kills %d best round %d, run active %d"),
			P->GetLevel(), P->GetLevelXP(), P->GetXPToNextLevel(), P->GetCash(), *Weapons, S.Runs, S.Wins, S.Kills, S.HighestRound, P->IsRunActive());
	}
}

void AZSPlayerController::ZSResetProfile()
{
	if (UZSProfileSubsystem* P = UZSProfileSubsystem::Get(this)) { P->ResetProfile(); ZSProfile(); }
}

void AZSPlayerController::ZSGiveCash(int32 Amount)
{
	if (UZSProfileSubsystem* P = UZSProfileSubsystem::Get(this)) { P->GiveCash(Amount); ZSProfile(); }
}

void AZSPlayerController::ZSRunCash(int32 Amount)
{
	if (AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>())
	{
		GS->RunCash += Amount;
		UE_LOG(LogZS, Warning, TEXT("Run cash: %d"), GS->RunCash);
	}
}

void AZSPlayerController::ZSBuyWall(FName ItemId)
{
	AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn());
	for (TActorIterator<AZSWallBuy> It(GetWorld()); It; ++It)
	{
		if (It->ItemId == ItemId)
		{
			UE_LOG(LogZS, Warning, TEXT("WallBuy %s: %s"), *It->GetName(), *It->GetInteractPrompt().ToString());
			It->Interact(Char);
			return;
		}
	}
	UE_LOG(LogZS, Warning, TEXT("No wall buy for %s"), *ItemId.ToString());
}

void AZSPlayerController::ZSUseBox()
{
	AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn());
	for (TActorIterator<AZSMysteryBox> It(GetWorld()); It; ++It)
	{
		AZSMysteryBox* Box = *It;
		Box->Interact(Char);
		UE_LOG(LogZS, Warning, TEXT("Box: rolling"));
		FTimerHandle H;
		TWeakObjectPtr<AZSMysteryBox> WeakBox = Box;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, WeakBox]()
		{
			if (WeakBox.IsValid())
			{
				UE_LOG(LogZS, Warning, TEXT("Box: %s"), *WeakBox->GetInteractPrompt().ToString());
				WeakBox->Interact(Cast<AZSPlayerCharacter>(GetPawn()));
			}
		}), 4.6f, false);
		return;
	}
}

void AZSPlayerController::ZSUseDoors()
{
	AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn());
	for (TActorIterator<AZSBuyDoor> It(GetWorld()); It; ++It)
	{
		if (!It->IsOpen())
		{
			UE_LOG(LogZS, Warning, TEXT("Door %s: %s"), *It->GetName(), *It->GetInteractPrompt().ToString());
			It->Interact(Char);
			UE_LOG(LogZS, Warning, TEXT("Door %s open: %d"), *It->GetName(), It->IsOpen() ? 1 : 0);
		}
	}
}

void AZSPlayerController::ZSHeightMap(int32 X0, int32 X1, int32 Y0, int32 Y1, int32 Step)
{
	Step = FMath::Max(Step, 50);
	FString Out = FString::Printf(TEXT("x %d..%d y %d..%d step %d  (floor z in dm; '.' = no hit; '~' = water/below -300)\n"), X0, X1, Y0, Y1, Step);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSHeightMap), true, GetPawn());
	for (int32 Y = Y0; Y <= Y1; Y += Step)
	{
		Out += FString::Printf(TEXT("%6d "), Y);
		for (int32 X = X0; X <= X1; X += Step)
		{
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(X, Y, 20000.f), FVector(X, Y, -5000.f), ECC_Visibility, Params))
			{
				Out += Hit.ImpactPoint.Z < -300.f ? TEXT("    ~") : FString::Printf(TEXT("%5d"), FMath::RoundToInt(Hit.ImpactPoint.Z / 10.f));
			}
			else
			{
				Out += TEXT("    .");
			}
		}
		Out += TEXT("\n");
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("ZSHeightMap.txt");
	FFileHelper::SaveStringToFile(Out, *Path);
	UE_LOG(LogZS, Warning, TEXT("Height map written to %s"), *Path);
}

void AZSPlayerController::ZSPrestige(int32 Rank)
{
	UZSProfileSubsystem* P = UZSProfileSubsystem::Get(this);
	if (!P) { return; }
	if (Rank < 0)
	{
		FText Msg;
		P->DoPrestige(Msg);
		UE_LOG(LogZS, Warning, TEXT("%s"), *Msg.ToString());
	}
	else
	{
		P->SetPrestigeDebug(Rank);
	}
	ZSProfile();
}

void AZSPlayerController::ZSGiveXP(int32 Amount)
{
	if (UZSProfileSubsystem* P = UZSProfileSubsystem::Get(this)) { P->GiveXP(Amount); ZSProfile(); }
}

void AZSPlayerController::ZSWinRun()
{
	if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
	{
		GM->EndRun(EZSRunOutcome::Won);
	}
}

void AZSPlayerController::ZSThirdPerson()
{
	if (AZSPlayerCharacter* Char = Cast<AZSPlayerCharacter>(GetPawn()))
	{
		Char->SetThirdPerson(!Char->IsThirdPerson());
	}
}

void AZSPlayerController::ZSProbe()
{
	const APawn* P = GetPawn();
	if (!P)
	{
		return;
	}
	FCollisionQueryParams Params(TEXT("ZSProbe"), false, P);
	const FVector Eye = P->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	FHitResult Hit;
	const float FloorZ = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye - FVector(0, 0, 1000), ECC_Visibility, Params) ? Hit.ImpactPoint.Z : -1.f;
	const float CeilZ = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + FVector(0, 0, 2000), ECC_Visibility, Params) ? Hit.ImpactPoint.Z : -1.f;
	FString Dirs;
	for (int32 i = 0; i < 16; ++i)
	{
		const FVector Dir = FRotator(0.f, i * 22.5f, 0.f).Vector();
		const float D = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Dir * 5000.f, ECC_Visibility, Params) ? Hit.Distance : 5000.f;
		Dirs += FString::Printf(TEXT(" %.0f:%.0f"), i * 22.5f, D);
	}
	UE_LOG(LogZS, Warning, TEXT("ZSProbe at %s floor %.0f ceiling %.0f |%s"), *P->GetActorLocation().ToString(), FloorZ, CeilZ, *Dirs);
}

void AZSPlayerController::ZSTask(int32 Type)
{
	if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
	{
		GM->GetTasks()->StartTask(static_cast<EZSRunTaskType>(FMath::Clamp(Type, 1, 3)));
	}
}

void AZSPlayerController::ZSGore(int32 Level)
{
	if (UZSGameUserSettings* S = UZSGameUserSettings::Get())
	{
		S->GoreLevel = FMath::Clamp(Level, 0, 2);
		S->SaveSettings();
		UE_LOG(LogZS, Warning, TEXT("Gore level %d (0 off, 1 reduced, 2 full)."), S->GoreLevel);
	}
}

void AZSPlayerController::ZSPowerUp(int32 Type)
{
	if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
	{
		GM->ActivatePowerUp(static_cast<EZSPowerUp>(FMath::Clamp(Type, 1, 4)));
	}
}

void AZSPlayerController::ZSHurtBone(const FString& Bone, float Damage)
{
	for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
	{
		if (It->IsDead())
		{
			continue;
		}
		FPointDamageEvent Ev;
		Ev.Damage = Damage;
		Ev.HitInfo.BoneName = FName(*Bone);
		Ev.HitInfo.ImpactPoint = It->GetMesh()->GetSocketLocation(Ev.HitInfo.BoneName);
		Ev.ShotDirection = (Ev.HitInfo.ImpactPoint - PlayerCameraManager->GetCameraLocation()).GetSafeNormal();
		It->TakeDamage(Damage, Ev, this, GetPawn());
	}
}

void AZSPlayerController::ZSBoss()
{
	if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
	{
		if (const AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>())
		{
			GM->GetRoundManager()->SkipToRound(GS->WinRound);
		}
	}
}
