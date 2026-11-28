#include "Zombies/ZSZombieBase.h"
#include "Zombies/ZSZombieAnimInstance.h"
#include "Zombies/ZSZombieAIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Combat/ZSHealthComponent.h"
#include "Combat/ZSAimAssistSubsystem.h"
#include "Combat/ZSBloodSubsystem.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Combat/ZSGoreSubsystem.h"
#include "World/ZSBarricade.h"
#include "Zombies/ZSSpitProjectile.h"
#include "Rounds/ZSGameState_Survival.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Zombieshooter.h"
#include "Rounds/ZSGameMode_Survival.h"
#include "Rounds/ZSRoundManagerComponent.h"

int32 AZSZombieBase::LiveCount = 0;

namespace
{
	/** The owner's two zombie characters. Each ships its own clips on its own skeleton. */
	TArray<FZSZombieAppearance> BuildAppearances()
	{
		TArray<FZSZombieAppearance> Out;

		FZSZombieAppearance City;
		City.MeshPath = TEXT("/Game/ZombieCity01/Mesh/SK_ZombieCity01.SK_ZombieCity01");
		City.AnimPrefix = TEXT("/Game/ZombieCity01/Animations/Anim_ZombieCity01_");
		City.Idles = { TEXT("Idle1"), TEXT("Idle2") };
		City.Walks = { TEXT("Walk") };
		City.Run = TEXT("Run");
		City.Rage = TEXT("Rage");
		City.Attacks = { TEXT("Attack1"), TEXT("Attack2"), TEXT("Attack4"), TEXT("Attack5") };   // Attack3 lunges 1.4 m
		City.AttackHitFractions = { 0.45f, 0.45f, 0.5f, 0.45f };
		City.HitReacts = { TEXT("GetHit2"), TEXT("GetHit3"), TEXT("GetHit4") };
		City.WalkAnimSpeed = 120.f;
		City.RunAnimSpeed = 380.f;
		Out.Add(City);

		FZSZombieAppearance Doctor;
		Doctor.MeshPath = TEXT("/Game/ZombieDoctor/Mesh/SK_ZombieDoctor.SK_ZombieDoctor");
		Doctor.AnimPrefix = TEXT("/Game/ZombieDoctor/Animations/Anim_ZombieDoctor_");
		Doctor.Idles = { TEXT("Idle1"), TEXT("Idle2") };
		Doctor.Walks = { TEXT("Walk2") };
		Doctor.Run = TEXT("Run");
		Doctor.Rage = TEXT("Rage");
		Doctor.Attacks = { TEXT("Attack1"), TEXT("Attack2"), TEXT("Attack4"), TEXT("Attack5") };
		Doctor.AttackHitFractions = { 0.45f, 0.45f, 0.5f, 0.45f };
		Doctor.HitReacts = { TEXT("GetHit2"), TEXT("GetHit3"), TEXT("GetHit4") };
		Doctor.WalkAnimSpeed = 80.f;
		Doctor.RunAnimSpeed = 320.f;
		Out.Add(Doctor);

		return Out;
	}
}

AZSZombieBase::AZSZombieBase()
{
	PrimaryActorTick.bCanEverTick = true;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(34.f, 90.f);
	// Bullets hit the physics-asset bodies (for head / limb zones), not the capsule.
	Capsule->SetCollisionResponseToChannel(ZS_TRACE_WEAPON, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	USkeletalMeshComponent* MeshComp = GetMesh();
	MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	MeshComp->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComp->SetCollisionObjectType(ECC_PhysicsBody);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(ZS_TRACE_WEAPON, ECR_Block);
	MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MeshComp->SetGenerateOverlapEvents(false);
	MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	MeshComp->bCastDynamicShadow = true;

	// Attached to the spine / head sockets once the mesh is chosen (ApplyAppearance).
	AimBody = CreateDefaultSubobject<USceneComponent>(TEXT("AimBody"));
	AimBody->SetupAttachment(GetCapsuleComponent());
	AimBody->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	AimHead = CreateDefaultSubobject<USceneComponent>(TEXT("AimHead"));
	AimHead->SetupAttachment(GetCapsuleComponent());
	AimHead->SetRelativeLocation(FVector(0.f, 0.f, 75.f));

	Health = CreateDefaultSubobject<UZSHealthComponent>(TEXT("Health"));

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 360.f, 0.f);
	Move->bUseRVOAvoidance = true;
	Move->AvoidanceConsiderationRadius = 220.f;
	Move->MaxAcceleration = 1200.f;
	Move->BrakingDecelerationWalking = 1400.f;
	Move->SetWalkableFloorAngle(50.f);

	AIControllerClass = AZSZombieAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

// =============================================================================================
// Setup
// =============================================================================================

void AZSZombieBase::BeginPlay()
{
	Super::BeginPlay();
	++LiveCount;

	ApplyAppearance();
	ApplyTypeDefaults();

	Health->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);

	if (UZSAimAssistSubsystem* Registry = GetWorld()->GetSubsystem<UZSAimAssistSubsystem>())
	{
		Registry->RegisterTarget(this, AimBody, AimHead, 40.f);
	}

	State = EZSZombieState::Spawning;
	StateTime = 0.f;
	StateDuration = FMath::FRandRange(0.3f, 0.8f);
}

void AZSZombieBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (State != EZSZombieState::Dead)
	{
		--LiveCount;
	}
	if (UWorld* World = GetWorld())
	{
		if (UZSAimAssistSubsystem* Registry = World->GetSubsystem<UZSAimAssistSubsystem>())
		{
			Registry->UnregisterTarget(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

UAnimSequence* AZSZombieBase::LoadAnim(const FString& Name) const
{
	if (Name.IsEmpty())
	{
		return nullptr;
	}
	const FString Package = Appearance.AnimPrefix + Name;
	const FString Path = Package + TEXT(".") + FPaths::GetBaseFilename(Package);
	return LoadObject<UAnimSequence>(nullptr, *Path);
}

UAnimSequence* AZSZombieBase::RandomAnim(const TArray<TObjectPtr<UAnimSequence>>& From) const
{
	return From.Num() > 0 ? From[FMath::RandRange(0, From.Num() - 1)].Get() : nullptr;
}

void AZSZombieBase::ApplyAppearance()
{
	static const TArray<FZSZombieAppearance> Appearances = BuildAppearances();

	// Pick a character that is actually present on this PC.
	TArray<int32> Order;
	for (int32 i = 0; i < Appearances.Num(); ++i)
	{
		Order.Insert(i, FMath::RandRange(0, Order.Num()));
	}

	USkeletalMesh* ChosenMesh = nullptr;
	for (const int32 i : Order)
	{
		ChosenMesh = LoadObject<USkeletalMesh>(nullptr, *Appearances[i].MeshPath);
		if (ChosenMesh)
		{
			Appearance = Appearances[i];
			break;
		}
	}
	if (!ChosenMesh)
	{
		UE_LOG(LogZS, Warning, TEXT("No zombie meshes found (ZombieCity01 / ZombieDoctor packs missing) - zombie will be invisible."));
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	MeshComp->SetSkeletalMeshAsset(ChosenMesh);
	MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	MeshComp->SetAnimInstanceClass(UZSZombieAnimInstance::StaticClass());
	AimBody->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("spine_03"));
	AimHead->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("head"));

	// Random skin (Skin1..3): every material ends in _Skin1, swap the number.
	const int32 Skin = FMath::RandRange(1, 3);
	if (Skin != 1)
	{
		for (int32 Slot = 0; Slot < MeshComp->GetNumMaterials(); ++Slot)
		{
			if (const UMaterialInterface* Current = MeshComp->GetMaterial(Slot))
			{
				const FString CurrentPath = Current->GetPathName();
				const FString NewPath = CurrentPath.Replace(TEXT("_Skin1"), *FString::Printf(TEXT("_Skin%d"), Skin));
				if (NewPath != CurrentPath)
				{
					if (UMaterialInterface* Variant = LoadObject<UMaterialInterface>(nullptr, *NewPath))
					{
						MeshComp->SetMaterial(Slot, Variant);
					}
				}
			}
		}
	}

	for (const FString& Name : Appearance.Attacks) { if (UAnimSequence* A = LoadAnim(Name)) { AttackAnims.Add(A); } }
	for (const FString& Name : Appearance.HitReacts) { if (UAnimSequence* A = LoadAnim(Name)) { HitAnims.Add(A); } }
	RageAnim = LoadAnim(Appearance.Rage);

	if (UZSZombieAnimInstance* Anim = GetZombieAnim())
	{
		const FString& IdleName = Appearance.Idles.Num() ? Appearance.Idles[FMath::RandRange(0, Appearance.Idles.Num() - 1)] : FString();
		const FString& WalkName = Appearance.Walks.Num() ? Appearance.Walks[FMath::RandRange(0, Appearance.Walks.Num() - 1)] : FString();
		Anim->SetLocomotion(LoadAnim(IdleName), LoadAnim(WalkName), LoadAnim(Appearance.Run), Appearance.WalkAnimSpeed, Appearance.RunAnimSpeed);
	}
}

void AZSZombieBase::ApplyTypeDefaults()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (ZombieType == EZSZombieType::Runner)
	{
		Health->MaxHealth = RunnerHealth * HealthMultiplier;
		MoveSpeed = FMath::FRandRange(RunnerSpeedRange.X, RunnerSpeedRange.Y) * SpeedMultiplier;
		Move->RotationRate = FRotator(0.f, 620.f, 0.f);
		Move->MaxAcceleration = 2400.f;
	}
	else
	{
		Health->MaxHealth = WalkerHealth * HealthMultiplier;
		MoveSpeed = FMath::FRandRange(WalkerSpeedRange.X, WalkerSpeedRange.Y) * SpeedMultiplier;
	}
	switch (ZombieType)
	{
	case EZSZombieType::Crawler:
		Health->MaxHealth = 70.f * HealthMultiplier;
		break;
	case EZSZombieType::Exploder:
		Health->MaxHealth = 90.f * HealthMultiplier;
		MoveSpeed *= 1.15f;
		break;
	case EZSZombieType::Spitter:
		Health->MaxHealth = 85.f * HealthMultiplier;
		SpecialCooldown = 2.f;
		break;
	case EZSZombieType::Brute:
		Health->MaxHealth = 900.f * HealthMultiplier;
		MoveSpeed = FMath::FRandRange(130.f, 160.f) * SpeedMultiplier;
		AttackDamage = 40.f;
		AttackRange = 175.f;
		FlinchChance = 0.f;
		DeathImpulse = 3000.f;
		SpecialCooldown = 4.f;
		break;
	case EZSZombieType::Screamer:
		Health->MaxHealth = 100.f * HealthMultiplier;
		SpecialCooldown = 3.f;
		break;
	case EZSZombieType::Boss:
		ApplyBossDefaults();
		break;
	default:
		break;
	}
	Health->Revive();   // apply the new max
	Move->MaxWalkSpeed = MoveSpeed;
	ApplySpecialLook();
}

UZSZombieAnimInstance* AZSZombieBase::GetZombieAnim() const
{
	return Cast<UZSZombieAnimInstance>(GetMesh()->GetAnimInstance());
}

APawn* AZSZombieBase::GetTarget() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

// =============================================================================================
// Behaviour
// =============================================================================================

void AZSZombieBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == EZSZombieState::Dead)
	{
		return;
	}

	StateTime += DeltaSeconds;
	FlinchTimer = FMath::Max(0.f, FlinchTimer - DeltaSeconds);
	UpdatePhysicalHit(DeltaSeconds);
	if (IsBoss())
	{
		UpdateBoss(DeltaSeconds);
		if (BossAction != EZSBossAction::None)
		{
			return;
		}
	}
	UpdateSpecial(DeltaSeconds);
	if (SpecialTime >= 0.f)
	{
		return;   // a special action (spit, scream, fuse) owns this frame
	}

	switch (State)
	{
	case EZSZombieState::Spawning:
		if (StateTime >= StateDuration)
		{
			if (ZombieType == EZSZombieType::Runner && RageAnim && FMath::FRand() < 0.7f)
			{
				State = EZSZombieState::Enraging;
				StateTime = 0.f;
				StateDuration = GetZombieAnim() ? GetZombieAnim()->PlayAction(RageAnim, 1.3f, 0.2f, 0.25f) * 0.85f : 0.f;
			}
			else
			{
				State = EZSZombieState::Chasing;
				StateTime = 0.f;
			}
		}
		break;

	case EZSZombieState::Enraging:
		FaceTarget(DeltaSeconds, 5.f);
		if (StateTime >= StateDuration)
		{
			State = EZSZombieState::Chasing;
			StateTime = 0.f;
		}
		break;

	case EZSZombieState::Chasing:
		UpdateChase(DeltaSeconds);
		break;

	case EZSZombieState::Attacking:
		UpdateAttack(DeltaSeconds);
		break;

	case EZSZombieState::Staggered:
		if (StateTime >= StateDuration)
		{
			State = EZSZombieState::Chasing;
			StateTime = 0.f;
			RepathTimer = 0.f;
		}
		break;

	default:
		break;
	}
}

void AZSZombieBase::UpdateChase(float DeltaSeconds)
{
	APawn* Target = GetTarget();
	if (!Target)
	{
		return;
	}

	if (UpdateBarricade(DeltaSeconds))
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float Dist2D = ToTarget.Size2D();
	if (TrySpecialAction(Dist2D, Target))
	{
		return;
	}
	if (Dist2D <= AttackRange && FMath::Abs(ToTarget.Z) < (bCrawling ? 160.f : 130.f))
	{
		StartAttack();
		return;
	}

	// Stragglers: no real progress towards the player for a while (stuck on debris, lost behind a wall) -> the round
	// manager moves them to a fresh spawn point out of view.
	ProgressTimer += DeltaSeconds;
	if (ProgressTimer >= 8.f)
	{
		const bool bNoProgress = Dist2D > 700.f && Dist2D > ProgressStartDist - 150.f;
		ProgressTimer = 0.f;
		ProgressStartDist = Dist2D;
		if (bNoProgress && !IsBoss())
		{
			if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
			{
				if (GM->GetRoundManager() && GM->GetRoundManager()->RelocateStraggler(this))
				{
					ProgressStartDist = 1.e9f;
					RepathTimer = 0.f;
					return;
				}
			}
		}
	}

	RepathTimer -= DeltaSeconds;
	if (RepathTimer <= 0.f)
	{
		RepathTimer = ZombieType == EZSZombieType::Runner ? 0.25f : 0.45f;
		AZSZombieAIController* AI = Cast<AZSZombieAIController>(GetController());
		bDirectChase = !(AI && AI->ChaseActor(Target, AttackRange * 0.5f));
	}

	// No NavMesh in this level (or no path): steer straight at the player, sliding along walls when blocked.
	if (bDirectChase)
	{
		const FVector Want = ToTarget.GetSafeNormal2D();
		if (SidestepTime > 0.f)
		{
			SidestepTime -= DeltaSeconds;
			AddMovementInput((SidestepDir + Want * 0.25f).GetSafeNormal2D());
		}
		else
		{
			AddMovementInput(Want);
			const float Speed = GetVelocity().Size2D();
			StuckTime = Speed < MoveSpeed * 0.3f ? StuckTime + DeltaSeconds : 0.f;
			if (StuckTime > 0.5f)
			{
				// Pick the side with more free space.
				StuckTime = 0.f;
				const FVector Right = FVector::CrossProduct(FVector::UpVector, Want);
				FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSZombieSidestep), false, this);
				const FVector Eye = GetActorLocation();
				FHitResult HitR, HitL;
				const bool bR = GetWorld()->LineTraceSingleByChannel(HitR, Eye, Eye + Right * 400.f, ECC_Visibility, Params);
				const bool bL = GetWorld()->LineTraceSingleByChannel(HitL, Eye, Eye - Right * 400.f, ECC_Visibility, Params);
				const float FreeR = bR ? HitR.Distance : 400.f;
				const float FreeL = bL ? HitL.Distance : 400.f;
				SidestepDir = (FreeR >= FreeL ? Right : -Right);
				SidestepTime = FMath::FRandRange(0.8f, 1.4f);
			}
		}
	}
}

bool AZSZombieBase::UpdateBarricade(float DeltaSeconds)
{
	AZSBarricade* B = Barricade.Get();
	if (!B)
	{
		return false;
	}
	UCharacterMovementComponent* Move = GetCharacterMovement();
	AZSZombieAIController* AI = Cast<AZSZombieAIController>(GetController());

	// Climbing through: a scripted arc over the sill with collision off.
	if (VaultTime >= 0.f)
	{
		const float Duration = ZombieType == EZSZombieType::Runner ? 0.55f : 0.9f;
		VaultTime += DeltaSeconds;
		const float A = FMath::Clamp(VaultTime / Duration, 0.f, 1.f);
		const FVector Sill = B->GetSillPoint();
		const FVector End = B->GetInsidePoint();
		const FVector P = FMath::Lerp(FMath::Lerp(VaultStart, Sill, A), FMath::Lerp(Sill, End, A), A);   // quadratic bezier
		// Head-first dive: a standing body can't fit a 1.5 m window (it used to clip through the wall above it), so the
		// zombie pitches forward almost flat over the sill and rights itself as it lands.
		const float Dive = FMath::Sin(A * PI);
		const float Yaw = (B->GetInsidePoint() - B->GetOutsidePoint()).Rotation().Yaw;
		SetActorLocationAndRotation(P, FRotator(-72.f * Dive, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (A >= 1.f)
		{
			SetActorRotation(FRotator(0.f, Yaw, 0.f));
			VaultTime = -1.f;
			Barricade = nullptr;
			GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Move->SetMovementMode(MOVE_Walking);
			RepathTimer = 0.f;
			ProgressStartDist = 1.e9f;
			ProgressTimer = 0.f;
		}
		return true;
	}

	const FVector Out = B->GetOutsidePoint();
	const FVector ToOut = Out - GetActorLocation();
	if (ToOut.Size2D() > 60.f)
	{
		RepathTimer -= DeltaSeconds;
		if (RepathTimer <= 0.f)
		{
			RepathTimer = 0.5f;
			bDirectChase = !(AI && AI->MoveToLocation(Out, 25.f, false, true, false, true) != EPathFollowingRequestResult::Failed);
		}
		if (bDirectChase)
		{
			AddMovementInput(ToOut.GetSafeNormal2D());
		}
		return true;
	}

	// At the window: face in, rip the boards off one at a time, then climb through.
	if (AI) { AI->StopMovement(); }
	const FRotator Want(0.f, (B->GetInsidePoint() - GetActorLocation()).Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 8.f));
	if (B->HasBoards())
	{
		TearTimer -= DeltaSeconds;
		if (TearTimer <= 0.f)
		{
			TearTimer = ZombieType == EZSZombieType::Runner ? 1.1f : (ZombieType == EZSZombieType::Brute ? 0.8f : 1.7f);
			if (UZSZombieAnimInstance* Anim = GetZombieAnim())
			{
				if (UAnimSequence* Swipe = RandomAnim(AttackAnims))
				{
					Anim->PlayAction(Swipe, 1.4f, 0.08f, 0.15f);
				}
			}
			B->TearBoard(this);
		}
		return true;
	}
	UE_LOG(LogZS, Log, TEXT("Zombie %s climbing in through %s"), *GetName(), *B->GetName());
	VaultTime = 0.f;
	VaultStart = GetActorLocation();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_Flying);
	return true;
}

void AZSZombieBase::StartAttack()
{
	if (AZSZombieAIController* AI = Cast<AZSZombieAIController>(GetController()))
	{
		AI->StopMovement();
	}
	GetCharacterMovement()->StopMovementImmediately();

	State = EZSZombieState::Attacking;
	StateTime = 0.f;
	bAttackLanded = false;

	AttackIndex = AttackAnims.Num() > 0 ? FMath::RandRange(0, AttackAnims.Num() - 1) : INDEX_NONE;
	AttackHitFraction = Appearance.AttackHitFractions.IsValidIndex(AttackIndex) ? Appearance.AttackHitFractions[AttackIndex] : 0.45f;
	const float Rate = ZombieType == EZSZombieType::Runner ? 1.3f : ((ZombieType == EZSZombieType::Brute || IsBoss()) ? 0.8f : 1.05f);
	const float Duration = (AttackIndex != INDEX_NONE && GetZombieAnim())
		? GetZombieAnim()->PlayAction(AttackAnims[AttackIndex], Rate, 0.12f, 0.2f)
		: 1.2f;
	StateDuration = FMath::Max(Duration, 0.5f);
}

void AZSZombieBase::UpdateAttack(float DeltaSeconds)
{
	const float Progress = StateTime / FMath::Max(StateDuration, 0.01f);
	if (Progress < AttackHitFraction)
	{
		FaceTarget(DeltaSeconds, 8.f);
	}

	if (!bAttackLanded && Progress >= AttackHitFraction)
	{
		bAttackLanded = true;
		if (APawn* Target = GetTarget())
		{
			const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(GetActorForwardVector(), ToTarget.GetSafeNormal2D())));
			if (ToTarget.Size2D() <= AttackRange + AttackReachBonus && FMath::Abs(ToTarget.Z) < 140.f && Angle < 70.f)
			{
				UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
				if (ZombieType == EZSZombieType::Brute || IsBoss())
				{
					if (ACharacter* Char = Cast<ACharacter>(Target))
					{
						Char->LaunchCharacter(ToTarget.GetSafeNormal2D() * 900.f + FVector(0.f, 0.f, 320.f), true, true);
					}
				}
			}
		}
	}

	// Leave a little early so attacks can chain into the chase smoothly.
	if (Progress >= 0.9f)
	{
		State = EZSZombieState::Chasing;
		StateTime = 0.f;
		RepathTimer = 0.f;
	}
}

void AZSZombieBase::FaceTarget(float DeltaSeconds, float Rate)
{
	if (const APawn* Target = GetTarget())
	{
		const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!To.IsNearlyZero())
		{
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), DeltaSeconds, Rate));
		}
	}
}

// =============================================================================================
// Damage / death
// =============================================================================================

float AZSZombieBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (State == EZSZombieState::Dead)
	{
		return 0.f;
	}
	// Player power-ups.
	if (EventInstigator && EventInstigator->IsPlayerController())
	{
		LastInstigator = EventInstigator;
		if (const AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>())
		{
			if (GS->IsPowerUpActive(EZSPowerUp::OneShot)) { DamageAmount = FMath::Max(DamageAmount, Health->GetHealth() + 1.f); }
			else if (GS->IsPowerUpActive(EZSPowerUp::Overkill)) { DamageAmount *= 2.f; }
		}
	}
	if (IsBoss())
	{
		// One Shot never one-shots the boss; weak points matter.
		DamageAmount = FMath::Min(DamageAmount, 400.f);
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			DamageAmount *= BossDamageScale(static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.BoneName);
		}
		if (BossAction == EZSBossAction::Roar && BossActionTime < 2.f) { DamageAmount *= 0.1f; }   // armoured while roaring
	}
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		LastHitLocation = Point.HitInfo.ImpactPoint;
		LastHitDirection = Point.ShotDirection.GetSafeNormal();
		LastHitBone = Point.HitInfo.BoneName;
		const FString Bone = LastHitBone.ToString().ToLower();
		bLastHitHead = Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("neck"));

		if (UZSBloodSubsystem* Blood = GetWorld()->GetSubsystem<UZSBloodSubsystem>())
		{
			Blood->SpawnHitBlood(LastHitLocation, LastHitDirection, bLastHitHead ? 1.6f : 1.f);
		}
		AddBloodiness(0.05f + DamageAmount / FMath::Max(1.f, Health->MaxHealth) * 0.3f);
	}
	LastHitDamage = DamageAmount;
	const bool bLethal = DamageAmount >= Health->GetHealth();

	// Dismemberment: limbs come off after enough damage, or from a heavy killing shot; heads pop on big headshot kills.
	if (!IsBoss() && UZSGoreSubsystem::DismemberEnabled() && LastHitBone != NAME_None && DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FName Cut = SeverBoneFor(LastHitBone);
		if (Cut != NAME_None && !Severed.Contains(Cut))
		{
			float& Taken = LimbDamage.FindOrAdd(Cut);
			Taken += DamageAmount;
			const bool bHead = Cut == TEXT("head");
			const bool bBigHit = DamageAmount >= 28.f;
			bool bCut = false;
			if (bHead)
			{
				bCut = bLethal && (DamageAmount >= 70.f || (bBigHit && FMath::FRand() < 0.55f));
			}
			else
			{
				bCut = Taken >= Health->MaxHealth * 0.5f || (bLethal && bBigHit && FMath::FRand() < 0.5f);
			}
			if (bCut)
			{
				Sever(Cut, LastHitDirection);
				const bool bLeg = Cut.ToString().StartsWith(TEXT("thigh")) || Cut.ToString().StartsWith(TEXT("calf"));
				if (bLeg && !bLethal && !bCrawling && ZombieType != EZSZombieType::Brute)
				{
					BecomeCrawler(false);
				}
			}
		}
	}

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Every bullet hits hard: a physical jolt on the body part, a stutter in the stride, a shove on heavy hits.
	if (State != EZSZombieState::Dead && !IsBoss() && DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		StartPhysicalHit(LastHitBone, LastHitDirection, DamageAmount * (bLastHitHead ? 1.4f : 1.f));
		if ((DamageAmount >= 30.f || bLastHitHead) && !bCrawling && ZombieType != EZSZombieType::Brute && GetCharacterMovement()->IsMovingOnGround())
		{
			const FVector Push = FVector(LastHitDirection.X, LastHitDirection.Y, 0.f).GetSafeNormal() * FMath::Clamp(DamageAmount * 2.5f, 60.f, 200.f);
			LaunchCharacter(Push, false, false);
		}
	}

	// Flinch (the health component already handled death if this was lethal).
	if (State != EZSZombieState::Dead && FlinchTimer <= 0.f && State != EZSZombieState::Attacking && SpecialTime < 0.f && !bCrawling &&
		ZombieType != EZSZombieType::Brute && !IsBoss() && (bLastHitHead || FMath::FRand() < FlinchChance))
	{
		UZSZombieAnimInstance* Anim = GetZombieAnim();
		UAnimSequence* Hit = RandomAnim(HitAnims);
		if (Anim && Hit)
		{
			if (AZSZombieAIController* AI = Cast<AZSZombieAIController>(GetController()))
			{
				AI->StopMovement();
			}
			const float Duration = Anim->PlayAction(Hit, 1.5f, 0.06f, 0.2f);
			State = EZSZombieState::Staggered;
			StateTime = 0.f;
			StateDuration = FMath::Min(Duration * 0.75f, ZombieType == EZSZombieType::Runner ? 0.45f : 0.8f);
			FlinchTimer = FlinchCooldown;
		}
	}
	return Applied;
}

void AZSZombieBase::HandleDeath(UZSHealthComponent* InHealth, AController* Killer, AActor* DamageCauser)
{
	if (State == EZSZombieState::Dead)
	{
		return;
	}
	State = EZSZombieState::Dead;
	--LiveCount;
	OnKilled.Broadcast(this, Killer, bLastHitHead);
	if (IsBoss())
	{
		if (Telegraph) { Telegraph->Destroy(); Telegraph = nullptr; }
		PublishBoss();
	}
	if (ZombieType == EZSZombieType::Exploder && !bExploded)
	{
		FTimerHandle Boom;
		GetWorldTimerManager().SetTimer(Boom, FTimerDelegate::CreateWeakLambda(this, [this]() { Explode(); }), 0.12f, false);
	}

	if (UZSAimAssistSubsystem* Registry = GetWorld()->GetSubsystem<UZSAimAssistSubsystem>())
	{
		Registry->UnregisterTarget(this);
	}

	EnterRagdoll();

	if (AController* C = GetController())
	{
		C->UnPossess();
		C->Destroy();
	}

	// Blood pool once the body has settled.
	FTimerHandle PoolTimer;
	GetWorldTimerManager().SetTimer(PoolTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UZSBloodSubsystem* Blood = GetWorld()->GetSubsystem<UZSBloodSubsystem>())
		{
			Blood->SpawnPool(GetMesh()->GetSocketLocation(TEXT("pelvis")));
		}
	}), 1.4f, false);

	SetLifeSpan(CorpseLifetime);
}

void AZSZombieBase::AddBloodiness(float Amount)
{
	if (!UZSGoreSubsystem::BloodEnabled())
	{
		return;
	}
	if (!BloodMID)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_BloodOverlay.M_ZS_BloodOverlay"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Base)
		{
			return;
		}
		BloodMID = UMaterialInstanceDynamic::Create(Base, this);
		BloodMID->SetScalarParameterValue(TEXT("Seed"), FMath::FRandRange(0.f, 100.f));
		GetMesh()->SetOverlayMaterial(BloodMID);
	}
	Bloodiness = FMath::Min(0.42f, Bloodiness + Amount);
	BloodMID->SetScalarParameterValue(TEXT("Amount"), Bloodiness);
}

void AZSZombieBase::StartPhysicalHit(FName Bone, const FVector& Direction, float Damage)
{
	// Visual-only jolt: the mesh rocks back / twists around the hips and springs back. (This used to hand the upper
	// body to physics for a moment, which could pull the hit boxes off the drawn body so quick follow-up body shots
	// missed.) The physics bodies stay kinematic and always match what you see.
	if (bCrawling)
	{
		return;
	}
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Direction.GetSafeNormal());   // X forward, Y right
	const FString B = Bone.ToString().ToLower();
	const bool bHead = B.Contains(TEXT("head")) || B.Contains(TEXT("neck"));
	const bool bLeg = B.StartsWith(TEXT("thigh")) || B.StartsWith(TEXT("calf")) || B.StartsWith(TEXT("foot"));
	const float K = FMath::Clamp(Damage / 30.f, 0.6f, 2.2f) * (bHead ? 1.5f : 1.f);
	// Shot from the front (Local.X < 0) pitches the body back; from the side, rolls it away; always a little twist.
	JoltVel.Pitch += -Local.X * 140.f * K * (bLeg ? 0.4f : 1.f);
	JoltVel.Roll += Local.Y * 110.f * K;
	JoltVel.Yaw += FMath::FRandRange(-1.f, 1.f) * 90.f * K;
	HitReactAlpha = 1.f;
}

void AZSZombieBase::UpdatePhysicalHit(float DeltaSeconds)
{
	if (State == EZSZombieState::Dead)
	{
		return;
	}
	// Jolt spring (stiff, quick: each hit is a sharp rock that's gone in ~0.3 s).
	const float Kp = 160.f, Kd = 16.f;
	auto Spring = [DeltaSeconds, Kp, Kd](double& X, double& V) { V += (-Kp * X - Kd * V) * DeltaSeconds; X += V * DeltaSeconds; };
	Spring(Jolt.Pitch, JoltVel.Pitch);
	Spring(Jolt.Roll, JoltVel.Roll);
	Spring(Jolt.Yaw, JoltVel.Yaw);
	Jolt.Pitch = FMath::Clamp(Jolt.Pitch, -25.0, 25.0);
	Jolt.Roll = FMath::Clamp(Jolt.Roll, -20.0, 20.0);
	Jolt.Yaw = FMath::Clamp(Jolt.Yaw, -20.0, 20.0);
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// Rotate the mesh about the capsule centre (roughly the hips), not its feet.
		const FQuat J = Jolt.Quaternion();
		MeshComp->SetRelativeLocationAndRotation(J.RotateVector(FVector(0.f, 0.f, -90.f)), J * FRotator(0.f, -90.f, 0.f).Quaternion());
	}
	if (HitReactAlpha > 0.f)
	{
		HitReactAlpha = FMath::Max(0.f, HitReactAlpha - DeltaSeconds / 0.35f);
		// Stopping power: the stride stutters while the hit plays out.
		if (UCharacterMovementComponent* Move = GetCharacterMovement(); Move && !bCrawling)
		{
			Move->MaxWalkSpeed = MoveSpeed * BuffMultiplier * (1.f - 0.6f * HitReactAlpha);
		}
	}
}

void AZSZombieBase::StopPhysicalHit()
{
	HitReactAlpha = 0.f;
	Jolt = JoltVel = FRotator::ZeroRotator;
}

void AZSZombieBase::EnterRagdoll()
{
	StopPhysicalHit();
	if (UZSZombieAnimInstance* Anim = GetZombieAnim())
	{
		Anim->Freeze();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();
	Move->SetComponentTickEnabled(false);

	USkeletalMeshComponent* MeshComp = GetMesh();
	MeshComp->SetCollisionProfileName(FName(TEXT("Ragdoll")));
	MeshComp->SetCollisionResponseToChannel(ZS_TRACE_WEAPON, ECR_Block);
	MeshComp->SetEnableGravity(true);
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetAllBodiesPhysicsBlendWeight(1.f);
	MeshComp->WakeAllRigidBodies();
	MeshComp->bBlendPhysics = true;

	// Shove from the killing shot: the hit body gets most of it, the pelvis a little so the whole body reacts.
	const float Scale = FMath::Clamp(LastHitDamage / 35.f, 0.7f, 1.8f) * (bLastHitHead ? 1.2f : 1.f);
	const FVector Push = LastHitDirection * DeathImpulse * 0.04f * Scale;   // DeathImpulse 9000 -> ~360 cm/s
	if (LastHitBone != NAME_None && MeshComp->GetBodyInstance(LastHitBone))
	{
		MeshComp->AddImpulse(Push, LastHitBone, true);
	}
	MeshComp->AddImpulse(Push * 0.35f + FVector(0.f, 0.f, 60.f), TEXT("pelvis"), true);
}

// =============================================================================================
// Specials (Milestone 11)
// =============================================================================================

namespace
{
	const TCHAR* MannequinSkeleton = TEXT("/Game/Zombie_deaths_crawls/demo/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
	const TCHAR* CrawlPath = TEXT("/Game/Zombie_deaths_crawls/Animations/");

	UAnimSequence* LoadCrawl(const TCHAR* Name)
	{
		return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s%s.%s"), CrawlPath, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

FText AZSZombieBase::TypeName(EZSZombieType Type)
{
	switch (Type)
	{
	case EZSZombieType::Runner:   return NSLOCTEXT("ZS", "ZRunner", "RUNNER");
	case EZSZombieType::Crawler:  return NSLOCTEXT("ZS", "ZCrawler", "CRAWLER");
	case EZSZombieType::Exploder: return NSLOCTEXT("ZS", "ZExploder", "BLOATER");
	case EZSZombieType::Spitter:  return NSLOCTEXT("ZS", "ZSpitter", "SPITTER");
	case EZSZombieType::Brute:    return NSLOCTEXT("ZS", "ZBrute", "BRUTE");
	case EZSZombieType::Screamer: return NSLOCTEXT("ZS", "ZScreamer", "SCREAMER");
	case EZSZombieType::Boss:     return NSLOCTEXT("ZS", "ZBoss", "THE ABOMINATION");
	default:                      return NSLOCTEXT("ZS", "ZWalker", "WALKER");
	}
}

void AZSZombieBase::ApplySpecialLook()
{
	FLinearColor Glow(0.f, 0.f, 0.f, 0.f);
	float Scale = 1.f;
	switch (ZombieType)
	{
	case EZSZombieType::Exploder: Glow = FLinearColor(1.f, 0.25f, 0.02f, 0.35f); Scale = 1.12f; break;
	case EZSZombieType::Spitter:  Glow = FLinearColor(0.3f, 1.f, 0.05f, 0.3f); break;
	case EZSZombieType::Brute:    Glow = FLinearColor(0.5f, 0.02f, 0.02f, 0.18f); Scale = 1.4f; break;
	case EZSZombieType::Screamer: Glow = FLinearColor(0.7f, 0.5f, 1.f, 0.3f); Scale = 0.95f; break;
	default: break;
	}
	if (Scale != 1.f)
	{
		SetActorScale3D(FVector(Scale));
	}
	if (Glow.A > 0.f)
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Glow.M_ZS_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(Glow.R, Glow.G, Glow.B));
			MID->SetScalarParameterValue(TEXT("Opacity"), Glow.A);
			MID->SetScalarParameterValue(TEXT("Fresnel"), 2.5f);
			GetMesh()->SetOverlayMaterial(MID);
		}
	}
	if (ZombieType == EZSZombieType::Crawler)
	{
		BecomeCrawler(true);
	}
}

void AZSZombieBase::BecomeCrawler(bool bHideLegs)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	USkeleton* Mine = MeshComp->GetSkeletalMeshAsset() ? MeshComp->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	USkeleton* Manny = LoadObject<USkeleton>(nullptr, MannequinSkeleton, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UAnimSequence* Crawl = LoadCrawl(TEXT("A_zombie_crawl_front"));
	UAnimSequence* Idle = LoadCrawl(TEXT("A_Zombie_Crawls_idle_1"));
	if (!Mine || !Manny || !Crawl)
	{
		// No crawl clips on this PC: losing the legs is fatal instead.
		if (!bHideLegs && Health) { Health->Kill(nullptr, nullptr); }
		return;
	}
	if (!Mine->GetCompatibleSkeletons().Contains(TSoftObjectPtr<USkeleton>(Manny)))
	{
		Mine->AddCompatibleSkeleton(Manny);   // same bone names: the mannequin crawl clips play on the zombie skeleton
	}
	bCrawling = true;
	if (bHideLegs)
	{
		for (const TCHAR* Leg : { TEXT("calf_l"), TEXT("calf_r") })
		{
			MeshComp->HideBoneByName(Leg, PBO_None);
			Severed.Add(Leg);
		}
	}
	GetCapsuleComponent()->SetCapsuleHalfHeight(40.f);
	MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	MoveSpeed = FMath::FRandRange(85.f, 115.f) * SpeedMultiplier;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	AttackRange = 110.f;
	AttackAnims.Reset();
	for (const TCHAR* A : { TEXT("A_Zombie_crawl_biting_1"), TEXT("A_Zombie_crawl_biting_2") })
	{
		if (UAnimSequence* Anim = LoadCrawl(A)) { AttackAnims.Add(Anim); }
	}
	Appearance.AttackHitFractions = { 0.5f, 0.5f };
	HitAnims.Reset();
	if (UZSZombieAnimInstance* Anim = GetZombieAnim())
	{
		Anim->StopAction(0.1f);
		Anim->SetLocomotion(Idle ? Idle : Crawl, Crawl, Crawl, 75.f, 75.f);
	}
	if (State == EZSZombieState::Attacking || State == EZSZombieState::Staggered)
	{
		State = EZSZombieState::Chasing;
		StateTime = 0.f;
	}
}

void AZSZombieBase::ApplySpeedBuff(float Multiplier, float Duration)
{
	if (State == EZSZombieState::Dead || ZombieType == EZSZombieType::Brute)
	{
		return;
	}
	BuffMultiplier = FMath::Max(BuffMultiplier, Multiplier);
	BuffTime = FMath::Max(BuffTime, Duration);
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed * BuffMultiplier;
}

bool AZSZombieBase::HasLineOfSight(const APawn* Target) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSZombieLOS), false, this);
	Params.AddIgnoredActor(Target);
	return !GetWorld()->LineTraceSingleByChannel(Hit, GetMesh()->GetSocketLocation(TEXT("head")), Target->GetActorLocation() + FVector(0, 0, 40.f), ECC_Visibility, Params);
}

void AZSZombieBase::UpdateSpecial(float DeltaSeconds)
{
	SpecialCooldown = FMath::Max(0.f, SpecialCooldown - DeltaSeconds);
	if (BuffTime > 0.f)
	{
		BuffTime -= DeltaSeconds;
		if (BuffTime <= 0.f)
		{
			BuffMultiplier = 1.f;
			GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
		}
	}
	if (ChargeTime > 0.f)
	{
		ChargeTime -= DeltaSeconds;
		if (const APawn* T = GetTarget())
		{
			AddMovementInput((T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D());
		}
		if (ChargeTime <= 0.f)
		{
			GetCharacterMovement()->MaxWalkSpeed = MoveSpeed * BuffMultiplier;
			if (UZSZombieAnimInstance* Anim = GetZombieAnim())
			{
				Anim->SetLocomotion(LoadAnim(Appearance.Idles.Num() ? Appearance.Idles[0] : FString()), LoadAnim(Appearance.Walks.Num() ? Appearance.Walks[0] : FString()), LoadAnim(Appearance.Run), Appearance.WalkAnimSpeed, Appearance.RunAnimSpeed);
			}
		}
	}

	if (SpecialTime < 0.f)
	{
		return;
	}
	// A running special action: spit, scream or exploder fuse.
	SpecialTime += DeltaSeconds;
	FaceTarget(DeltaSeconds, 6.f);
	switch (ZombieType)
	{
	case EZSZombieType::Spitter:
		if (!bSpecialFired && SpecialTime >= 0.55f) { bSpecialFired = true; FireSpit(); }
		if (SpecialTime >= 1.1f) { SpecialTime = -1.f; State = EZSZombieState::Chasing; StateTime = 0.f; RepathTimer = 0.f; }
		break;
	case EZSZombieType::Screamer:
		if (!bSpecialFired && SpecialTime >= 0.6f) { bSpecialFired = true; Scream(); }
		if (SpecialTime >= 1.8f) { SpecialTime = -1.f; State = EZSZombieState::Chasing; StateTime = 0.f; RepathTimer = 0.f; }
		break;
	case EZSZombieType::Exploder:
		// Fuse: swell and flash, then burst.
		SetActorScale3D(FVector(1.12f + SpecialTime * 0.25f + 0.04f * FMath::Sin(SpecialTime * 40.f)));
		if (SpecialTime >= 1.0f) { SpecialTime = -1.f; Health->Kill(nullptr, this); }
		break;
	default:
		SpecialTime = -1.f;
		break;
	}
}

bool AZSZombieBase::TrySpecialAction(float Dist2D, APawn* Target)
{
	auto Begin = [this](UAnimSequence* Anim, float Rate)
	{
		if (AZSZombieAIController* AI = Cast<AZSZombieAIController>(GetController())) { AI->StopMovement(); }
		GetCharacterMovement()->StopMovementImmediately();
		SpecialTime = 0.f;
		bSpecialFired = false;
		if (Anim && GetZombieAnim()) { GetZombieAnim()->PlayAction(Anim, Rate, 0.12f, 0.2f); }
	};
	switch (ZombieType)
	{
	case EZSZombieType::Exploder:
		if (Dist2D < 230.f)
		{
			Begin(RageAnim, 1.6f);
			return true;
		}
		break;
	case EZSZombieType::Spitter:
		if (SpecialCooldown <= 0.f && Dist2D > 450.f && Dist2D < 1800.f && HasLineOfSight(Target))
		{
			Begin(AttackAnims.Num() ? AttackAnims[0].Get() : nullptr, 1.2f);
			SpecialCooldown = FMath::FRandRange(3.f, 4.5f);
			return true;
		}
		break;
	case EZSZombieType::Screamer:
		if (SpecialCooldown <= 0.f && Dist2D < 2500.f && HasLineOfSight(Target))
		{
			Begin(RageAnim, 1.f);
			SpecialCooldown = FMath::FRandRange(9.f, 12.f);
			return true;
		}
		break;
	case EZSZombieType::Brute:
		if (SpecialCooldown <= 0.f && ChargeTime <= 0.f && Dist2D > 500.f && Dist2D < 1400.f && HasLineOfSight(Target))
		{
			// Charge: sprint straight at the player for a moment.
			ChargeTime = 1.7f;
			SpecialCooldown = FMath::FRandRange(7.f, 10.f);
			GetCharacterMovement()->MaxWalkSpeed = 560.f;
			if (UZSZombieAnimInstance* Anim = GetZombieAnim())
			{
				UAnimSequence* Run = LoadAnim(Appearance.Run);
				Anim->SetLocomotion(Run, Run, Run, Appearance.RunAnimSpeed, Appearance.RunAnimSpeed);
			}
		}
		break;
	default:
		break;
	}
	return false;
}

void AZSZombieBase::FireSpit()
{
	const APawn* Target = GetTarget();
	if (!Target)
	{
		return;
	}
	const FVector From = GetMesh()->GetSocketLocation(TEXT("head")) + GetActorForwardVector() * 40.f;
	// Lead the target a little.
	const FVector Aim = Target->GetActorLocation() + Target->GetVelocity() * 0.5f - FVector(0, 0, 60.f);
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AZSSpitProjectile* Spit = GetWorld()->SpawnActor<AZSSpitProjectile>(AZSSpitProjectile::StaticClass(), FTransform(From), Params))
	{
		Spit->Launch(AZSSpitProjectile::AimVelocity(From, Aim, 1400.f));
	}
}

void AZSZombieBase::Scream()
{
	int32 Buffed = 0;
	for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead() && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 1800.f)
		{
			It->ApplySpeedBuff(1.45f, 7.f);
			++Buffed;
		}
	}
	UE_LOG(LogZS, Log, TEXT("Screamer enraged %d zombies."), Buffed);
}

void AZSZombieBase::Explode()
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	const FVector Center = GetMesh()->GetSocketLocation(TEXT("spine_03"));
	const float Radius = 420.f;

	// Hurts the player (falloff) and other zombies; kills credit whoever killed the exploder.
	if (APawn* P = GetTarget())
	{
		const float D = FVector::Dist(P->GetActorLocation(), Center);
		if (D < Radius)
		{
			UGameplayStatics::ApplyDamage(P, 15.f + 50.f * (1.f - D / Radius), nullptr, this, UDamageType::StaticClass());
		}
	}
	for (TActorIterator<AZSZombieBase> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead())
		{
			const float D = FVector::Dist(It->GetActorLocation(), Center);
			if (D < Radius)
			{
				FPointDamageEvent Ev;
				Ev.Damage = 120.f * (1.f - D / Radius) + 20.f;
				Ev.ShotDirection = (It->GetActorLocation() - Center).GetSafeNormal();
				Ev.HitInfo.ImpactPoint = It->GetActorLocation();
				It->TakeDamage(Ev.Damage, Ev, LastInstigator.Get(), this);
			}
		}
	}

	if (UZSGoreSubsystem* Gore = GetWorld()->GetSubsystem<UZSGoreSubsystem>())
	{
		Gore->SpawnGibs(Center, FVector::UpVector, 14, 1.4f);
		Gore->SplatterNearCamera(Center, 700.f, 1.f);
	}
	if (UZSBloodSubsystem* Blood = GetWorld()->GetSubsystem<UZSBloodSubsystem>())
	{
		for (int32 i = 0; i < 6; ++i)
		{
			Blood->SpawnHitBlood(Center, (FMath::VRand() + FVector(0, 0, 0.3f)).GetSafeNormal(), 2.f);
		}
		Blood->SpawnPool(Center - FVector(0, 0, 80.f));
	}
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/GunSoundsPackVol1/Bonus/cue/Explosion_Medium_1-1_Cue.Explosion_Medium_1-1_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Center);
	}
	// Nothing left of it.
	if (UZSGoreSubsystem::DismemberEnabled())
	{
		GetMesh()->SetVisibility(false, true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetLifeSpan(1.f);
	}
}

FName AZSZombieBase::SeverBoneFor(FName HitBone)
{
	const FString B = HitBone.ToString().ToLower();
	const TCHAR* Side = B.EndsWith(TEXT("_l")) ? TEXT("_l") : (B.EndsWith(TEXT("_r")) ? TEXT("_r") : nullptr);
	if (B.Contains(TEXT("head")) || B.Contains(TEXT("neck"))) { return TEXT("head"); }
	if (!Side) { return NAME_None; }
	if (B.Contains(TEXT("hand")) || B.Contains(TEXT("lowerarm"))) { return FName(*(FString(TEXT("lowerarm")) + Side)); }
	if (B.Contains(TEXT("upperarm"))) { return FName(*(FString(TEXT("upperarm")) + Side)); }
	if (B.Contains(TEXT("calf")) || B.Contains(TEXT("foot"))) { return FName(*(FString(TEXT("calf")) + Side)); }
	if (B.Contains(TEXT("thigh"))) { return FName(*(FString(TEXT("thigh")) + Side)); }
	return NAME_None;
}

void AZSZombieBase::Sever(FName Bone, const FVector& Direction)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return;
	}
	Severed.Add(Bone);
	UE_LOG(LogZS, Log, TEXT("Severed %s on %s"), *Bone.ToString(), *GetName());
	const FVector Where = MeshComp->GetSocketLocation(Bone);
	if (UZSGoreSubsystem* Gore = GetWorld()->GetSubsystem<UZSGoreSubsystem>())
	{
		Gore->SpawnSeveredPart(MeshComp, Bone, Direction);   // before hiding it: the copy takes the current pose
	}
	MeshComp->HideBoneByName(Bone, PBO_Term);
	if (UZSGoreSubsystem* Gore = GetWorld()->GetSubsystem<UZSGoreSubsystem>())
	{
		const bool bHead = Bone == TEXT("head");
		Gore->SpawnGibs(Where, Direction, bHead ? 3 : 1, bHead ? 0.6f : 0.5f);   // a few small bits, the real part flies off
		Gore->AddStump(MeshComp, Bone, bHead ? 3.f : 2.2f);
		Gore->SplatterNearCamera(Where, 380.f, bHead ? 0.7f : 0.45f);
	}
}
