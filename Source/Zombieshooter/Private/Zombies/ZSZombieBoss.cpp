// The round-20 boss, "The Abomination" (Milestone 12). Part of AZSZombieBase so it shares chase / melee / ragdoll.
//
// Phase 1 (100-66 %): melee swipes and a telegraphed ground slam (red ring: jump or get out). After a slam it is
//                     EXPOSED for 3 s - weak points (head, chest, back) take triple damage.
// Phase 2 (66-33 %):  roars in adds and hurls debris at a marked spot (orange ring), plus everything above.
// Phase 3 (< 33 %):   enraged - faster, slams more often and charges.
// Phase changes: armoured roar (90 % damage reduction for 2 s) that calls a wave of adds.

#include "Zombies/ZSZombieBase.h"
#include "Zombies/ZSZombieAIController.h"
#include "Zombies/ZSZombieAnimInstance.h"
#include "Zombies/ZSSpitProjectile.h"
#include "Combat/ZSHealthComponent.h"
#include "Combat/ZSGoreSubsystem.h"
#include "Combat/ZSBloodSubsystem.h"
#include "Rounds/ZSGameState_Survival.h"
#include "Rounds/ZSGameMode_Survival.h"
#include "Rounds/ZSRoundManagerComponent.h"
#include "World/ZSTaskMarker.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Zombieshooter.h"

namespace
{
	const FLinearColor SlamRed(1.f, 0.08f, 0.02f);
	const FLinearColor ThrowOrange(1.f, 0.5f, 0.05f);
	constexpr float SlamRadius = 520.f;
	constexpr float SlamWindup = 1.35f;
	constexpr float ThrowWindup = 0.9f;
	constexpr float ExposedTime = 3.f;
}

void AZSZombieBase::ApplyBossDefaults()
{
	SetActorScale3D(FVector(2.3f));
	Health->MaxHealth = BossMaxHealth;
	MoveSpeed = 150.f;
	AttackDamage = 35.f;
	AttackRange = 210.f;   // scaled capsule: ~2 m reach
	AttackReachBonus = 80.f;
	FlinchChance = 0.f;
	DeathImpulse = 1500.f;
	CorpseLifetime = 60.f;
	GetCharacterMovement()->MaxAcceleration = 900.f;
	GetCharacterMovement()->bUseRVOAvoidance = false;
	// Bigger body: strides are 2.3x longer, so the clips play slower for the same ground speed.
	if (UZSZombieAnimInstance* Anim = GetZombieAnim())
	{
		Anim->SetLocomotion(LoadAnim(Appearance.Idles.Num() ? Appearance.Idles[0] : FString()), LoadAnim(Appearance.Walks.Num() ? Appearance.Walks[0] : FString()),
			LoadAnim(Appearance.Run), Appearance.WalkAnimSpeed * 2.3f, Appearance.RunAnimSpeed * 2.3f);
	}
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Glow.M_ZS_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.1f, 0.05f));
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.25f);
		MID->SetScalarParameterValue(TEXT("Fresnel"), 3.f);
		GetMesh()->SetOverlayMaterial(MID);
	}
	if (AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>())
	{
		GS->bBossActive = true;
		GS->BossName = TypeName(EZSZombieType::Boss);
		GS->BossSay(NSLOCTEXT("ZS", "BossArrives", "THE ABOMINATION HAS ARRIVED"));
	}
	PublishBoss();
}

float AZSZombieBase::BossDamageScale(FName Bone) const
{
	const FString B = Bone.ToString().ToLower();
	const bool bWeak = B.Contains(TEXT("head")) || B.Contains(TEXT("neck")) || B.Contains(TEXT("spine_03")) || B.Contains(TEXT("spine_02"));
	if (BossAction == EZSBossAction::Exposed)
	{
		return bWeak ? 3.f : 1.5f;
	}
	return bWeak ? 1.25f : 1.f;
}

void AZSZombieBase::PublishBoss()
{
	if (AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>())
	{
		GS->BossHealthPct = Health->MaxHealth > 0.f ? Health->GetHealth() / Health->MaxHealth : 0.f;
		GS->BossPhase = BossPhase;
		GS->bBossExposed = BossAction == EZSBossAction::Exposed;
		if (IsDead())
		{
			GS->bBossActive = false;
			GS->BossHealthPct = 0.f;
		}
	}
}

void AZSZombieBase::StartBossAction(EZSBossAction Action)
{
	if (AZSZombieAIController* AI = Cast<AZSZombieAIController>(GetController())) { AI->StopMovement(); }
	GetCharacterMovement()->StopMovementImmediately();
	BossAction = Action;
	BossActionTime = 0.f;
	bBossActionFired = false;
	if (Telegraph) { Telegraph->Destroy(); Telegraph = nullptr; }
	UZSZombieAnimInstance* Anim = GetZombieAnim();
	AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();

	auto Ring = [this](const FVector& At, float Radius, const FLinearColor& Color)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FHitResult Hit;
		FVector Ground = At;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ZSBossRing), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 100.f), At - FVector(0, 0, 800.f), ECC_Visibility, Q)) { Ground = Hit.ImpactPoint; }
		if (AZSTaskMarker* M = GetWorld()->SpawnActor<AZSTaskMarker>(AZSTaskMarker::StaticClass(), FTransform(Ground), P))
		{
			M->Setup(Radius, Color, false);
			Telegraph = M;
		}
	};

	switch (Action)
	{
	case EZSBossAction::Slam:
		Ring(GetActorLocation(), SlamRadius, SlamRed);
		if (Anim && AttackAnims.Num()) { Anim->PlayAction(AttackAnims[0], 0.55f, 0.15f, 0.2f); }
		if (GS) { GS->BossSay(NSLOCTEXT("ZS", "BossSlam", "GROUND SLAM - JUMP OR GET CLEAR")); }
		break;
	case EZSBossAction::Throw:
	{
		const APawn* T = GetTarget();
		ThrowTarget = T ? T->GetActorLocation() + T->GetVelocity() * 0.6f : GetActorLocation();
		Ring(ThrowTarget, 240.f, ThrowOrange);
		if (Anim && AttackAnims.Num() > 1) { Anim->PlayAction(AttackAnims[1], 0.8f, 0.15f, 0.2f); }
		break;
	}
	case EZSBossAction::Roar:
		if (Anim && RageAnim) { Anim->PlayAction(RageAnim, 0.8f, 0.2f, 0.3f); }
		break;
	case EZSBossAction::Exposed:
		if (Anim && HitAnims.Num()) { Anim->PlayAction(HitAnims[0], 0.45f, 0.1f, 0.3f); }
		if (GS) { GS->BossSay(NSLOCTEXT("ZS", "BossExposed", "IT'S STUNNED - SHOOT THE HEAD AND CHEST")); }
		break;
	case EZSBossAction::Charge:
		GetCharacterMovement()->MaxWalkSpeed = 620.f;
		if (UZSZombieAnimInstance* A = GetZombieAnim())
		{
			UAnimSequence* Run = LoadAnim(Appearance.Run);
			A->SetLocomotion(Run, Run, Run, Appearance.RunAnimSpeed * 2.3f, Appearance.RunAnimSpeed * 2.3f);
		}
		if (GS) { GS->BossSay(NSLOCTEXT("ZS", "BossCharge", "IT'S CHARGING")); }
		break;
	default:
		break;
	}
	PublishBoss();
}

void AZSZombieBase::BossSlamImpact()
{
	const FVector Center = GetActorLocation();
	if (APawn* P = GetTarget())
	{
		const FVector D = P->GetActorLocation() - Center;
		const bool bAirborne = P->GetMovementComponent() && P->GetMovementComponent()->IsFalling();
		if (D.Size2D() < SlamRadius && !bAirborne)
		{
			UGameplayStatics::ApplyDamage(P, 45.f, GetController(), this, UDamageType::StaticClass());
			if (ACharacter* C = Cast<ACharacter>(P))
			{
				C->LaunchCharacter(D.GetSafeNormal2D() * 1000.f + FVector(0, 0, 450.f), true, true);
			}
		}
	}
	if (UZSGoreSubsystem* Gore = GetWorld()->GetSubsystem<UZSGoreSubsystem>())
	{
		Gore->SplatterNearCamera(Center, SlamRadius, 0.35f);
	}
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/GunSoundsPackVol1/Bonus/cue/Explosion_Large_3-1_Cue.Explosion_Large_3-1_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Center, 0.7f, 0.6f);
	}
}

void AZSZombieBase::BossThrow()
{
	const FVector From = GetMesh()->GetSocketLocation(TEXT("hand_r")) + FVector(0, 0, 60.f);
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AZSSpitProjectile* Rock = GetWorld()->SpawnActorDeferred<AZSSpitProjectile>(AZSSpitProjectile::StaticClass(), FTransform(From), this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Rock->bLeavePool = false;
		Rock->ImpactDamage = 30.f;
		Rock->SplashRadius = 240.f;
		Rock->SplashDamage = 30.f;
		Rock->Color = ThrowOrange;
		Rock->Size = 2.6f;
		Rock->FinishSpawning(FTransform(From));
		Rock->Launch(AZSSpitProjectile::AimVelocity(From, ThrowTarget, 1600.f));
	}
}

void AZSZombieBase::BossRoar(int32 Adds)
{
	if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
	{
		GM->GetRoundManager()->SpawnExtra(Adds);
	}
	for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead()) { It->ApplySpeedBuff(1.3f, 6.f); }
	}
}

void AZSZombieBase::UpdateBoss(float DeltaSeconds)
{
	if (IsDead())
	{
		return;
	}
	SlamCooldown -= DeltaSeconds;
	ThrowCooldown -= DeltaSeconds;
	RoarCooldown -= DeltaSeconds;
	BossChargeCooldown -= DeltaSeconds;
	PublishBoss();
	AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();

	// Phase changes.
	const float Pct = Health->GetHealth() / FMath::Max(1.f, Health->MaxHealth);
	const int32 WantPhase = Pct > 0.66f ? 1 : (Pct > 0.33f ? 2 : 3);
	if (WantPhase > BossPhase && BossAction != EZSBossAction::Slam)
	{
		BossPhase = WantPhase;
		if (GS) { GS->BossSay(BossPhase == 2 ? NSLOCTEXT("ZS", "BossP2", "IT'S CALLING THE HORDE") : NSLOCTEXT("ZS", "BossP3", "IT'S ENRAGED")); }
		if (BossPhase == 3)
		{
			MoveSpeed = 210.f;
			GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
		}
		StartBossAction(EZSBossAction::Roar);
		BossRoar(BossPhase == 2 ? 6 : 8);
		RoarCooldown = 30.f;
		return;
	}

	if (BossAction == EZSBossAction::None)
	{
		// Pick an action when the chase allows it.
		const APawn* T = GetTarget();
		if (!T || State == EZSZombieState::Attacking || State == EZSZombieState::Spawning)
		{
			return;
		}
		const float Dist = FVector::Dist2D(T->GetActorLocation(), GetActorLocation());
		if (SlamCooldown <= 0.f && Dist < SlamRadius * 0.9f)
		{
			StartBossAction(EZSBossAction::Slam);
			SlamCooldown = BossPhase == 3 ? 5.f : 8.f;
		}
		else if (BossPhase >= 2 && ThrowCooldown <= 0.f && Dist > 700.f && Dist < 3500.f && HasLineOfSight(T))
		{
			StartBossAction(EZSBossAction::Throw);
			ThrowCooldown = BossPhase == 3 ? 4.5f : 6.5f;
		}
		else if (BossPhase >= 2 && RoarCooldown <= 0.f)
		{
			StartBossAction(EZSBossAction::Roar);
			BossRoar(4);
			RoarCooldown = 28.f;
		}
		else if (BossPhase == 3 && BossChargeCooldown <= 0.f && Dist > 600.f && Dist < 1800.f)
		{
			StartBossAction(EZSBossAction::Charge);
			BossChargeCooldown = 9.f;
		}
		return;
	}

	BossActionTime += DeltaSeconds;
	switch (BossAction)
	{
	case EZSBossAction::Slam:
		FaceTarget(DeltaSeconds, 2.f);
		if (!bBossActionFired && BossActionTime >= SlamWindup)
		{
			bBossActionFired = true;
			if (Telegraph) { Telegraph->Destroy(); Telegraph = nullptr; }
			BossSlamImpact();
		}
		if (BossActionTime >= SlamWindup + 0.4f)
		{
			StartBossAction(EZSBossAction::Exposed);
		}
		break;
	case EZSBossAction::Throw:
		FaceTarget(DeltaSeconds, 5.f);
		if (!bBossActionFired && BossActionTime >= ThrowWindup)
		{
			bBossActionFired = true;
			BossThrow();
		}
		if (BossActionTime >= ThrowWindup + 1.6f)
		{
			if (Telegraph) { Telegraph->Destroy(); Telegraph = nullptr; }
			BossAction = EZSBossAction::None;
		}
		break;
	case EZSBossAction::Roar:
		if (BossActionTime >= 2.2f) { BossAction = EZSBossAction::None; }
		break;
	case EZSBossAction::Exposed:
		if (BossActionTime >= ExposedTime) { BossAction = EZSBossAction::None; }
		break;
	case EZSBossAction::Charge:
		if (const APawn* T = GetTarget())
		{
			const FVector To = T->GetActorLocation() - GetActorLocation();
			AddMovementInput(To.GetSafeNormal2D());
			FaceTarget(DeltaSeconds, 3.f);
			if (To.Size2D() < AttackRange + 40.f && !bBossActionFired)
			{
				bBossActionFired = true;
				UGameplayStatics::ApplyDamage(const_cast<APawn*>(T), 40.f, GetController(), this, UDamageType::StaticClass());
				if (ACharacter* C = Cast<ACharacter>(const_cast<APawn*>(T)))
				{
					C->LaunchCharacter(To.GetSafeNormal2D() * 1300.f + FVector(0, 0, 400.f), true, true);
				}
			}
		}
		if (BossActionTime >= 2.f || bBossActionFired)
		{
			GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
			if (UZSZombieAnimInstance* A = GetZombieAnim())
			{
				A->SetLocomotion(LoadAnim(Appearance.Idles.Num() ? Appearance.Idles[0] : FString()), LoadAnim(Appearance.Walks.Num() ? Appearance.Walks[0] : FString()),
					LoadAnim(Appearance.Run), Appearance.WalkAnimSpeed * 2.3f, Appearance.RunAnimSpeed * 2.3f);
			}
			BossAction = EZSBossAction::None;
		}
		break;
	default:
		BossAction = EZSBossAction::None;
		break;
	}
}
