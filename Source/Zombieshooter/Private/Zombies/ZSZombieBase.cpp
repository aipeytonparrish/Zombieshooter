#include "Zombies/ZSZombieBase.h"
#include "Zombies/ZSZombieAnimInstance.h"
#include "Zombies/ZSZombieAIController.h"
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
#include "Zombieshooter.h"

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
	Health->Revive();   // apply the new max
	Move->MaxWalkSpeed = MoveSpeed;
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

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float Dist2D = ToTarget.Size2D();
	if (Dist2D <= AttackRange && FMath::Abs(ToTarget.Z) < 130.f)
	{
		StartAttack();
		return;
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
	const float Rate = ZombieType == EZSZombieType::Runner ? 1.3f : 1.05f;
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
	}
	LastHitDamage = DamageAmount;

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Flinch (the health component already handled death if this was lethal).
	if (State != EZSZombieState::Dead && FlinchTimer <= 0.f && State != EZSZombieState::Attacking &&
		(bLastHitHead || FMath::FRand() < FlinchChance))
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

void AZSZombieBase::EnterRagdoll()
{
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
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->SetSimulatePhysics(true);
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
