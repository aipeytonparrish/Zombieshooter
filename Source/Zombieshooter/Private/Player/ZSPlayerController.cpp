#include "Player/ZSPlayerController.h"
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
	FString TeleportStr;
	if (FParse::Value(Cmd, TEXT("ZSAutoTeleport="), TeleportStr, false))
	{
		TArray<FString> Parts;
		TeleportStr.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() == 3)
		{
			const FVector Where(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			FTimerHandle TeleportTimer;
			GetWorldTimerManager().SetTimer(TeleportTimer, FTimerDelegate::CreateWeakLambda(this, [this, Where]() { if (APawn* P = GetPawn()) { const bool bOk = P->TeleportTo(Where, P->GetActorRotation()); UE_LOG(LogZS, Warning, TEXT("Teleport %s -> %s"), bOk ? TEXT("ok") : TEXT("FAILED"), *P->GetActorLocation().ToCompactString()); } }), 0.7f, false);
		}
	}
	if (FParse::Param(Cmd, TEXT("ZSAutoThirdPerson")))
	{
		FTimerHandle TPTimer;
		GetWorldTimerManager().SetTimer(TPTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { ZSThirdPerson(); }), 1.0f, false);
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
			Z->ZombieType = Type == 0 ? EZSZombieType::Walker : (Type == 1 ? EZSZombieType::Runner : (FMath::FRand() < 0.35f ? EZSZombieType::Runner : EZSZombieType::Walker));
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
		UE_LOG(LogZS, Warning, TEXT("Zombie %s: dist %.0f state %d speed %.0f %s at %s"), *Z->GetName(),
			P ? FVector::Dist(P->GetActorLocation(), Z->GetActorLocation()) : -1.f, (int32)Z->GetZombieState(),
			Z->GetVelocity().Size2D(), Z->IsDirectChasing() ? TEXT("DIRECT") : TEXT("NAV"), *Z->GetActorLocation().ToCompactString());
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
