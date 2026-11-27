#include "Rounds/ZSRoundManagerComponent.h"
#include "Rounds/ZSGameState_Survival.h"
#include "Zombies/ZSZombieBase.h"
#include "World/ZSSpawnPoint.h"
#include "World/ZSPickup.h"
#include "Player/ZSPlayerCharacter.h"
#include "Player/ZSCameraComponent.h"
#include "Engine/CurveTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Rounds/ZSGameMode_Survival.h"
#include "Zombieshooter.h"

namespace
{
	TArray<TWeakObjectPtr<AZSPickup>> GPickups;
}

UZSRoundManagerComponent::UZSRoundManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	RoundCurves = TSoftObjectPtr<UCurveTable>(FSoftObjectPath(TEXT("/Game/ZS/Data/CT_ZSRounds.CT_ZSRounds")));
}

void UZSRoundManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	LoadedCurves = RoundCurves.LoadSynchronous();
	if (!LoadedCurves)
	{
		UE_LOG(LogZS, Warning, TEXT("Round CurveTable %s not found - using built-in round formulas."), *RoundCurves.ToString());
	}
	if (!RoundStartSound)
	{
		RoundStartSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/GunSoundsPackVol1/Bonus/cue/Explosion_Large_3-1_Cue.Explosion_Large_3-1_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!RoundEndSound)
	{
		RoundEndSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/GunSoundsPackVol1/Bonus/cue/Explosion_Medium_1-1_Cue.Explosion_Medium_1-1_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}


	GPickups.Reset();
	PhaseTimer = PreGameDuration;
	if (AZSGameState_Survival* GS = GetState())
	{
		GS->Phase = EZSRoundPhase::PreGame;
		GS->PhaseTimeLeft = PhaseTimer;
	}
}

AZSGameState_Survival* UZSRoundManagerComponent::GetState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AZSGameState_Survival>() : nullptr;
}

APawn* UZSRoundManagerComponent::GetPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

// ---- Tuning ---------------------------------------------------------------------------------

FZSRoundTuning UZSRoundManagerComponent::GetTuning(int32 Round) const
{
	const float R = static_cast<float>(FMath::Max(Round, 1));
	FZSRoundTuning T;
	// Built-in formulas (also what Data/ZSRounds.csv was generated from). ~1,000 zombies over 20 rounds.
	T.ZombieCount = FMath::RoundToInt((6.f + 2.6f * R + 0.12f * R * R) * 1.25f);   // x1.25: run length tuning (M12)
	T.HealthMultiplier = 1.f + 0.1f * (R - 1.f);
	T.SpeedMultiplier = FMath::Min(1.3f, 1.f + 0.015f * (R - 1.f));
	T.RunnerChance = FMath::Clamp((R - 2.f) * 0.035f, 0.f, 0.6f);
	T.SpawnInterval = FMath::Max(0.35f, 2.f - 0.08f * (R - 1.f));
	T.MaxAlive = FMath::Min(32, FMath::RoundToInt(8.f + 1.5f * R));

	if (LoadedCurves)
	{
		static const FString Ctx(TEXT("ZSRounds"));
		auto Eval = [this, R](const TCHAR* Row, float Fallback)
		{
			if (const FRealCurve* Curve = LoadedCurves->FindCurve(FName(Row), Ctx, false))
			{
				return Curve->Eval(R, Fallback);
			}
			return Fallback;
		};
		T.ZombieCount = FMath::RoundToInt(Eval(TEXT("ZombieCount"), T.ZombieCount));
		T.HealthMultiplier = Eval(TEXT("HealthMultiplier"), T.HealthMultiplier);
		T.SpeedMultiplier = Eval(TEXT("SpeedMultiplier"), T.SpeedMultiplier);
		T.RunnerChance = Eval(TEXT("RunnerChance"), T.RunnerChance);
		T.SpawnInterval = Eval(TEXT("SpawnInterval"), T.SpawnInterval);
		T.MaxAlive = FMath::RoundToInt(Eval(TEXT("MaxAlive"), T.MaxAlive));
	}
	T.MaxAlive = FMath::Clamp(T.MaxAlive, 1, MaxAliveCeiling);
	return T;
}

// ---- Flow -----------------------------------------------------------------------------------

void UZSRoundManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AZSGameState_Survival* GS = GetState();
	if (!GS || bRunOver)
	{
		return;
	}
	GS->RunTime += DeltaTime;

	switch (GS->Phase)
	{
	case EZSRoundPhase::PreGame:
	case EZSRoundPhase::Break:
		PhaseTimer -= DeltaTime;
		GS->PhaseTimeLeft = FMath::Max(0.f, PhaseTimer);
		if (PhaseTimer <= 0.f)
		{
			StartRound(GS->Round + 1);
		}
		break;

	case EZSRoundPhase::Active:
		SpawnTimer -= DeltaTime;
		if (ToSpawn > 0 && Alive < Tuning.MaxAlive && SpawnTimer <= 0.f)
		{
			SpawnOne();
			SpawnTimer = Tuning.SpawnInterval * FMath::FRandRange(0.7f, 1.3f);
		}
		GS->ZombiesRemaining = ToSpawn + Alive;
		if (ToSpawn <= 0 && Alive <= 0 && !bBossRound)
		{
			// Round cleared.
			GS->Phase = EZSRoundPhase::Break;
			PhaseTimer = BreakDuration;
			GS->PhaseTimeLeft = PhaseTimer;
			PlayStinger(RoundEndSound, 0.35f, 0.55f);
			UE_LOG(LogZS, Log, TEXT("Round %d cleared - break %.0f s"), GS->Round, BreakDuration);
			if (AZSGameMode_Survival* GM = Cast<AZSGameMode_Survival>(GetOwner()))
			{
				GM->OnRoundCleared(GS->Round);
			}
		}
		break;

	default:
		break;
	}
}

void UZSRoundManagerComponent::StartRound(int32 Round)
{
	SpawnPoints.Reset();
	for (TActorIterator<AZSSpawnPoint> It(GetWorld()); It; ++It)
	{
		SpawnPoints.Add(*It);
	}
	if (Round == 1)
	{
		UE_LOG(LogZS, Log, TEXT("Round manager: %d spawn points%s."), SpawnPoints.Num(), SpawnPoints.Num() == 0 ? TEXT(" (using a ring around the player)") : TEXT(""));
	}
	AZSGameState_Survival* GS = GetState();
	Tuning = GetTuning(Round);
	ToSpawn = Tuning.ZombieCount;
	bBossRound = Round == GS->WinRound;
	if (bBossRound)
	{
		// Boss round: the Abomination plus a small escort; its roars call in more.
		ToSpawn = 10;
		Tuning.MaxAlive = FMath::Min(Tuning.MaxAlive, 18);
	}
	SpawnTimer = 0.5f;
	GS->Round = Round;
	GS->Phase = EZSRoundPhase::Active;
	GS->RoundStartTime = GetWorld()->GetTimeSeconds();
	if (AZSGameMode_Survival* RoundGM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>()) { RoundGM->OnRoundStarted(Round); }
	GS->ZombiesRemaining = ToSpawn + Alive;
	PlayStinger(RoundStartSound, 0.45f, 0.6f);
	if (bBossRound)
	{
		SpawnBoss();
	}
	UE_LOG(LogZS, Log, TEXT("Round %d: %d zombies, health x%.2f, speed x%.2f, runners %.0f%%, interval %.2fs, max alive %d"),
		Round, Tuning.ZombieCount, Tuning.HealthMultiplier, Tuning.SpeedMultiplier, Tuning.RunnerChance * 100.f, Tuning.SpawnInterval, Tuning.MaxAlive);
}

void UZSRoundManagerComponent::EndRun()
{
	bRunOver = true;
	if (AZSGameState_Survival* GS = GetState())
	{
		GS->Phase = EZSRoundPhase::GameOver;
	}
}

void UZSRoundManagerComponent::SkipToRound(int32 Round)
{
	for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
	{
		It->OnKilled.RemoveAll(this);
		It->Destroy();
	}
	Alive = 0;
	if (AZSGameState_Survival* GS = GetState())
	{
		GS->Round = FMath::Max(0, Round - 1);
	}
	StartRound(FMath::Max(1, Round));
}

void UZSRoundManagerComponent::PlayStinger(USoundBase* Sound, float Volume, float Pitch) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Volume, Pitch);
	}
}

// ---- Spawning -------------------------------------------------------------------------------

bool UZSRoundManagerComponent::IsVisibleToPlayer(const FVector& Point) const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Fwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector Target = Point + FVector(0.f, 0.f, 80.f);
	const FVector To = (Target - Eye).GetSafeNormal();
	const float HalfFOV = FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle() * 0.5f + 15.f);
	if (FVector::DotProduct(Fwd, To) < FMath::Cos(HalfFOV))
	{
		return false;   // outside the view cone
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSSpawnLOS), false, PC->GetPawn());
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);
}

bool UZSRoundManagerComponent::PickSpawnLocation(FVector& OutLocation, FRotator& OutRotation, bool& bOutAllowRunners) const
{
	const APawn* Player = GetPlayerPawn();
	if (!Player)
	{
		return false;
	}
	const FVector P = Player->GetActorLocation();
	bOutAllowRunners = true;

	if (SpawnPoints.Num() > 0)
	{
		// Valid = enabled, in the distance band, out of view. Pick randomly among the closest few so the horde
		// arrives from varied directions without spawning on the far side of the map.
		TArray<TPair<float, const AZSSpawnPoint*>> Valid;
		const AZSSpawnPoint* Fallback = nullptr;
		float FallbackDist = 0.f;
		for (const AZSSpawnPoint* SP : SpawnPoints)
		{
			if (!SP || !SP->bEnabled)
			{
				continue;
			}
			const float D = FVector::Dist(SP->GetActorLocation(), P);
			if (D < MinSpawnDistance)
			{
				continue;
			}
			if (IsVisibleToPlayer(SP->GetActorLocation()))
			{
				continue;
			}
			if (D <= MaxSpawnDistance)
			{
				Valid.Emplace(D, SP);
			}
			else if (!Fallback || D < FallbackDist)
			{
				Fallback = SP;
				FallbackDist = D;
			}
		}
		Valid.Sort([](const TPair<float, const AZSSpawnPoint*>& A, const TPair<float, const AZSSpawnPoint*>& B) { return A.Key < B.Key; });
		const AZSSpawnPoint* Chosen = Valid.Num() > 0 ? Valid[FMath::RandRange(0, FMath::Min(Valid.Num(), 4) - 1)].Value : Fallback;
		if (Chosen)
		{
			// Small jitter so a group from one point doesn't stack.
			OutLocation = Chosen->GetActorLocation() + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 0.f);
			OutRotation = FRotator(0.f, (P - OutLocation).Rotation().Yaw, 0.f);
			bOutAllowRunners = Chosen->bAllowRunners;
			return true;
		}
	}

	// No usable spawn points: a ring around the player, behind or out of sight.
	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		const float Yaw = FMath::FRandRange(0.f, 360.f);
		const float Dist = FMath::FRandRange(MinSpawnDistance * 1.2f, MinSpawnDistance * 2.5f);
		const FVector Flat = P + FRotator(0.f, Yaw, 0.f).Vector() * Dist;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSSpawnGround), false, Player);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Flat + FVector(0.f, 0.f, 500.f), Flat - FVector(0.f, 0.f, 1500.f), ECC_Visibility, Params))
		{
			continue;
		}
		const FVector Candidate = Hit.ImpactPoint + FVector(0.f, 0.f, 95.f);
		if (IsVisibleToPlayer(Candidate) && Attempt < 12)
		{
			continue;
		}
		OutLocation = Candidate;
		OutRotation = FRotator(0.f, (P - Candidate).Rotation().Yaw, 0.f);
		return true;
	}
	return false;
}

void UZSRoundManagerComponent::SpawnOne()
{
	FVector Loc;
	FRotator Rot;
	bool bAllowRunners = true;
	if (!PickSpawnLocation(Loc, Rot, bAllowRunners))
	{
		UE_LOG(LogZS, Warning, TEXT("SpawnOne: no spawn location (points=%d)"), SpawnPoints.Num());
		return;
	}

	const FTransform T(Rot, Loc);
	AZSZombieBase* Z = GetWorld()->SpawnActorDeferred<AZSZombieBase>(AZSZombieBase::StaticClass(), T, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Z)
	{
		UE_LOG(LogZS, Warning, TEXT("SpawnOne: spawn blocked at %s"), *Loc.ToCompactString());
		return;
	}
	Z->ZombieType = (bAllowRunners && FMath::FRand() < Tuning.RunnerChance) ? EZSZombieType::Runner : EZSZombieType::Walker;
	// Specials join from round 4 on (Brutes from round 10, at most 2 alive).
	if (const AZSGameState_Survival* RoundGS = GetState())
	{
		const int32 Round = RoundGS->Round;
		struct FSpecial { EZSZombieType Type; int32 FromRound; float Chance; };
		static const FSpecial Specials[] = {
			{ EZSZombieType::Crawler, 4, 0.06f }, { EZSZombieType::Exploder, 5, 0.05f }, { EZSZombieType::Spitter, 7, 0.05f },
			{ EZSZombieType::Screamer, 9, 0.03f }, { EZSZombieType::Brute, 10, 0.025f } };
		float Roll = FMath::FRand();
		for (const FSpecial& S : Specials)
		{
			if (Round < S.FromRound) { continue; }
			const float Chance = S.Chance * FMath::Min(2.f, 1.f + (Round - S.FromRound) * 0.06f);
			if (Roll < Chance)
			{
				int32 Brutes = 0;
				for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It) { Brutes += (!It->IsDead() && It->ZombieType == EZSZombieType::Brute); }
				if (S.Type != EZSZombieType::Brute || Brutes < 2) { Z->ZombieType = S.Type; }
				break;
			}
			Roll -= Chance;
		}
	}
	Z->HealthMultiplier = Tuning.HealthMultiplier;
	Z->SpeedMultiplier = Tuning.SpeedMultiplier;
	Z->FinishSpawning(T);
	if (!IsValid(Z))
	{
		UE_LOG(LogZS, Warning, TEXT("SpawnOne: zombie invalid after FinishSpawning at %s"), *Loc.ToCompactString());
		return;
	}
	Z->OnKilled.AddUObject(this, &ThisClass::HandleZombieKilled);
	--ToSpawn;
	++Alive;
}

void UZSRoundManagerComponent::HandleZombieKilled(AZSZombieBase* Zombie, AController* Killer, bool bHeadshot)
{
	Alive = FMath::Max(0, Alive - 1);
	if (AZSGameState_Survival* GS = GetState())
	{
		const bool bByPlayer = Killer && Killer->IsPlayerController();
		if (bByPlayer)
		{
			++GS->Kills;
			GS->Headshots += bHeadshot ? 1 : 0;
			if (AZSGameMode_Survival* GM = Cast<AZSGameMode_Survival>(GetOwner()))
			{
				GM->RewardKill(Zombie, bHeadshot);
			}
		}
		GS->ZombiesRemaining = ToSpawn + Alive;
	}
	if (Zombie && !bRunOver)
	{
		MaybeDropPickup(Zombie->GetActorLocation());
	}
}

void UZSRoundManagerComponent::MaybeDropPickup(const FVector& Location)
{
	GPickups.RemoveAll([](const TWeakObjectPtr<AZSPickup>& P) { return !P.IsValid(); });
	// Power-ups: rarer, one on the ground at a time.
	EZSPickupType PowerUp = EZSPickupType::Health;
	bool bPowerUp = false;
	if (FMath::FRand() < PowerUpDropChance && !GPickups.ContainsByPredicate([](const TWeakObjectPtr<AZSPickup>& P) { return P->Type >= EZSPickupType::Overkill; }))
	{
		const float R = FMath::FRand();
		PowerUp = R < 0.32f ? EZSPickupType::Overkill : (R < 0.6f ? EZSPickupType::Payday : (R < 0.85f ? EZSPickupType::OneShot : EZSPickupType::ScorchedEarth));
		bPowerUp = true;
	}
	if (!bPowerUp && (GPickups.Num() >= MaxPickupsAlive || FMath::FRand() >= PickupDropChance))
	{
		return;
	}
	FHitResult Hit;
	FVector Ground = Location;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSPickupGround), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location, Location - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
	{
		Ground = Hit.ImpactPoint;
	}
	const FTransform T(FRotator::ZeroRotator, Ground + FVector(0.f, 0.f, 10.f));
	if (AZSPickup* Pickup = GetWorld()->SpawnActorDeferred<AZSPickup>(AZSPickup::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Pickup->Type = bPowerUp ? PowerUp : (FMath::FRand() < ArmorDropShare ? EZSPickupType::Armor : EZSPickupType::Health);
		Pickup->FinishSpawning(T);
		GPickups.Add(Pickup);
		UE_LOG(LogZS, Log, TEXT("Dropped %s pickup"), Pickup->Type == EZSPickupType::Armor ? TEXT("armor") : TEXT("health"));
	}
}

void UZSRoundManagerComponent::SpawnExtra(int32 Count)
{
	ToSpawn += FMath::Max(0, Count);
	SpawnTimer = 0.f;
}

void UZSRoundManagerComponent::SpawnBoss()
{
	// Farthest spawn point from the player (or a ring around them).
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	FVector Loc = Player ? Player->GetActorLocation() + Player->GetActorForwardVector() * 2500.f : FVector::ZeroVector;
	float Best = -1.f;
	for (const AZSSpawnPoint* SP : SpawnPoints)
	{
		if (!SP || !Player) { continue; }
		const float D = FVector::Dist(SP->GetActorLocation(), Player->GetActorLocation());
		if (D > Best && D < 6000.f) { Best = D; Loc = SP->GetActorLocation(); }
	}
	Loc.Z += 120.f;   // room for the scaled capsule
	const FTransform T(Player ? (Player->GetActorLocation() - Loc).GetSafeNormal2D().Rotation() : FRotator::ZeroRotator, Loc);
	AZSZombieBase* Boss = GetWorld()->SpawnActorDeferred<AZSZombieBase>(AZSZombieBase::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Boss)
	{
		UE_LOG(LogZS, Error, TEXT("Boss failed to spawn at %s"), *Loc.ToCompactString());
		return;
	}
	Boss->ZombieType = EZSZombieType::Boss;
	Boss->FinishSpawning(T);
	Boss->OnKilled.AddUObject(this, &ThisClass::HandleZombieKilled);
	Boss->OnKilled.AddLambda([this](AZSZombieBase*, AController*, bool)
	{
		if (AZSGameMode_Survival* GM = Cast<AZSGameMode_Survival>(GetOwner())) { GM->OnBossKilled(); }
	});
	++Alive;
	UE_LOG(LogZS, Log, TEXT("Boss spawned at %s"), *Loc.ToCompactString());
}
