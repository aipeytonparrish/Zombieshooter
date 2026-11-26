#include "Weapons/ZSWeaponComponent.h"
#include "Weapons/ZSWeaponBase.h"
#include "Weapons/ZSWeapon_RD704.h"
#include "Weapons/ZSWeapon_TacticalAR.h"
#include "Weapons/ZSFPAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Weapons/ZSWeapon_Pistol.h"
#include "Player/ZSPlayerCharacter.h"
#include "Player/ZSCameraComponent.h"
#include "Player/ZSFirstPersonArmsComponent.h"
#include "Movement/ZSMovementComponent.h"
#include "Combat/ZSHealthComponent.h"
#include "Combat/ZSAimAssistSubsystem.h"
#include "Core/ZSGameUserSettings.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Zombieshooter.h"

static TAutoConsoleVariable<int32> CVarZSShowImpacts(TEXT("zs.ShowImpacts"), UE_BUILD_SHIPPING ? 0 : 1,
	TEXT("Draw bullet impact points (1) and tracers (2)."));

namespace
{
	float EaseInOut(float A) { return A * A * (3.f - 2.f * A); }

	float SpringStep(float& Value, float& Velocity, float Stiffness, float Damping, float DeltaTime)
	{
		const float Accel = -Stiffness * Value - Damping * Velocity;
		Velocity += Accel * DeltaTime;
		Value += Velocity * DeltaTime;
		return Value;
	}

	/** Bell curve 0 -> 1 -> 0 over [Start, End] of a 0..1 timeline. */
	float Window(float T, float Start, float End, float Blend = 0.12f)
	{
		const float In = FMath::Clamp((T - Start) / Blend, 0.f, 1.f);
		const float Out = FMath::Clamp((End - T) / Blend, 0.f, 1.f);
		return EaseInOut(FMath::Min(In, Out));
	}
}

UZSWeaponComponent::UZSWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the character's own tick (movement input, camera feel) so the view model uses this frame's camera.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	DefaultWeapons.Add(AZSWeapon_TacticalAR::StaticClass());
	DefaultWeapons.Add(AZSWeapon_Pistol::StaticClass());
	DefaultWeapons.Add(AZSWeapon_RD704::StaticClass());
}

AZSPlayerCharacter* UZSWeaponComponent::GetCharacter() const
{
	return Cast<AZSPlayerCharacter>(GetOwner());
}

APlayerController* UZSWeaponComponent::GetPC() const
{
	const AZSPlayerCharacter* Char = GetCharacter();
	return Char ? Cast<APlayerController>(Char->GetController()) : nullptr;
}

AZSWeaponBase* UZSWeaponComponent::GetCurrentWeapon() const
{
	return Weapons.IsValidIndex(CurrentIndex) ? Weapons[CurrentIndex].Get() : nullptr;
}

// =============================================================================================
// Setup
// =============================================================================================

void UZSWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AZSPlayerCharacter* Char = GetCharacter())
	{
		ViewModelRoot = Char->GetViewModelRoot();
		Arms = Char->GetFirstPersonArms();
		AnimatedArms = Char->GetAnimatedArms();
	}
	SpawnDefaultWeapons();
}

void UZSWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (AZSWeaponBase* W : Weapons)
	{
		if (W)
		{
			W->Destroy();
		}
	}
	Weapons.Reset();
	Super::EndPlay(EndPlayReason);
}

void UZSWeaponComponent::SpawnDefaultWeapons()
{
	UWorld* World = GetWorld();
	AZSPlayerCharacter* Char = GetCharacter();
	if (!World || !Char || !ViewModelRoot)
	{
		return;
	}

	for (const TSubclassOf<AZSWeaponBase>& Class : DefaultWeapons)
	{
		if (!Class)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = Char;
		Params.Instigator = Char;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AZSWeaponBase* Weapon = World->SpawnActor<AZSWeaponBase>(Class, FTransform::Identity, Params))
		{
			Weapon->AttachToComponent(ViewModelRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			Weapon->SetWeaponVisible(false);
			Weapons.Add(Weapon);
		}
	}

	if (Weapons.Num() > 0)
	{
		EquipIndex(0, false);
	}
}

void UZSWeaponComponent::EquipIndex(int32 Index, bool bInstant)
{
	if (!Weapons.IsValidIndex(Index))
	{
		return;
	}
	if (AZSWeaponBase* Old = GetCurrentWeapon())
	{
		Old->SetWeaponVisible(false);
	}
	CurrentIndex = Index;
	PendingIndex = INDEX_NONE;
	AZSWeaponBase* W = Weapons[Index];
	W->SetWeaponVisible(true);

	SetupArmsFor(W);

	State = EZSWeaponState::Equipping;
	StateTime = 0.f;
	StateDuration = bInstant ? 0.01f : W->Stats.EquipTime;
	if (W->IsAnimated() && !bInstant)
	{
		const FZSLoadedAnimSet& L = W->GetLoadedAnims();
		if (UZSFPAnimInstance* ArmsAnim = W->GetArmsAnim(); ArmsAnim && L.Equip)
		{
			ArmsAnim->PlayAction(L.Equip, L.Equip->GetPlayLength() / FMath::Max(StateDuration, 0.1f), 0.01f, 0.2f);
		}
		if (L.EquipSound)
		{
			UGameplayStatics::PlaySound2D(this, L.EquipSound);
		}
	}
	EquipAlpha = 0.f;
	Bloom = 0.f;
	ShotsInSequence = 0;
	BurstRemaining = 0;
	OnWeaponChanged.Broadcast(W);
}

// =============================================================================================
// Input
// =============================================================================================

void UZSWeaponComponent::SetTriggerHeld(bool bHeld)
{
	if (bHeld && !bTriggerHeld)
	{
		bFireQueued = true;
		FireQueuedTime = 0.f;
		bDryFired = false;

		// Pulling the trigger while sprinting ends the sprint (fires after SprintToFireTime).
		if (AZSPlayerCharacter* Char = GetCharacter())
		{
			if (Char->GetZSMovement() && Char->GetZSMovement()->IsSprinting())
			{
				Char->CancelSprint();
			}
		}
		// Firing interrupts a reload that hasn't committed yet only if the mag still has rounds.
		if (State == EZSWeaponState::Reloading && !bReloadCommitted)
		{
			if (const AZSWeaponBase* W = GetCurrentWeapon(); W && W->GetAmmoInMag() > 0)
			{
				CancelReload();
			}
		}
	}
	bTriggerHeld = bHeld;
}

void UZSWeaponComponent::SetADSHeld(bool bHeld)
{
	if (bHeld && !bADSHeld)
	{
		if (AZSPlayerCharacter* Char = GetCharacter())
		{
			if (Char->GetZSMovement() && Char->GetZSMovement()->IsSprinting())
			{
				Char->CancelSprint();
			}
		}
	}
	bADSHeld = bHeld;
}

void UZSWeaponComponent::RequestReload()
{
	BeginReload();
}

void UZSWeaponComponent::RequestSwitchWeapon()
{
	if (Weapons.Num() < 2 || State == EZSWeaponState::Unequipping)
	{
		return;
	}
	if (State == EZSWeaponState::Reloading)
	{
		CancelReload();
	}
	PendingIndex = (CurrentIndex + 1) % Weapons.Num();
	State = EZSWeaponState::Unequipping;
	StateTime = 0.f;
	const AZSWeaponBase* W = GetCurrentWeapon();
	StateDuration = W ? W->Stats.UnequipTime : 0.2f;
	if (W && W->IsAnimated())
	{
		const FZSLoadedAnimSet& L = W->GetLoadedAnims();
		if (UZSFPAnimInstance* ArmsAnim = W->GetArmsAnim(); ArmsAnim && L.Holster)
		{
			ArmsAnim->PlayAction(L.Holster, L.Holster->GetPlayLength() / FMath::Max(StateDuration, 0.1f), 0.05f, 0.05f);
		}
	}
}

void UZSWeaponComponent::RequestCycleFireMode()
{
	if (AZSWeaponBase* W = GetCurrentWeapon())
	{
		W->CycleFireMode();
		BurstRemaining = 0;
	}
}

void UZSWeaponComponent::RefillAllAmmo()
{
	for (AZSWeaponBase* W : Weapons)
	{
		if (W)
		{
			W->RefillAmmo();
		}
	}
}

float UZSWeaponComponent::GetLookInputScale(bool bGamepad) const
{
	const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	const float ADSSens = bGamepad ? Settings->GamepadADSSensitivity : Settings->MouseADSSensitivity;
	float Scale = FMath::Lerp(1.f, ADSSens, ADSAlpha);
	if (bGamepad)
	{
		Scale *= FrictionScale;
	}
	return Scale;
}

float UZSWeaponComponent::GetReloadProgress() const
{
	return State == EZSWeaponState::Reloading && StateDuration > 0.f ? FMath::Clamp(StateTime / StateDuration, 0.f, 1.f) : 0.f;
}

bool UZSWeaponComponent::IsWeaponLowered() const
{
	return SprintAlpha > 0.5f || LowerAlpha > 0.5f;
}

// =============================================================================================
// Tick
// =============================================================================================

void UZSWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AZSPlayerCharacter* Char = GetCharacter();
	APlayerController* PC = GetPC();
	if (!Char || !PC || !GetCurrentWeapon() || DeltaTime <= 0.f)
	{
		return;
	}

	// Player look input this frame = control rotation change since we last touched it.
	const FRotator Control = PC->GetControlRotation();
	FRotator LookDelta = FRotator::ZeroRotator;
	if (bHasLastControlRotation)
	{
		LookDelta.Pitch = FMath::FindDeltaAngleDegrees(LastControlRotation.Pitch, Control.Pitch);
		LookDelta.Yaw = FMath::FindDeltaAngleDegrees(LastControlRotation.Yaw, Control.Yaw);
	}
	LastControlRotation = Control;
	bHasLastControlRotation = true;

	const UZSMovementComponent* Move = Char->GetZSMovement();
	const bool bSprinting = Move && Move->IsSprinting();
	TimeSinceSprint = bSprinting ? 0.f : TimeSinceSprint + DeltaTime;
	SprintAlpha = FMath::FInterpTo(SprintAlpha, bSprinting && State != EZSWeaponState::Reloading ? 1.f : 0.f, DeltaTime, 10.f);
	LowerAlpha = FMath::FInterpTo(LowerAlpha, (Move && Move->IsMantling()) ? 1.f : 0.f, DeltaTime, 12.f);

	UpdateStateMachine(DeltaTime);
	UpdateADS(DeltaTime);
	UpdateFiring(DeltaTime);
	UpdateSpread(DeltaTime);
	UpdateRecoil(DeltaTime, LookDelta);
	UpdateAimAssist(DeltaTime, LookDelta);
	UpdateViewModel(DeltaTime, LookDelta);

	// Our own control changes this frame are not "player look" next frame.
	LastControlRotation = PC->GetControlRotation();

	// Prune hit markers.
	const float Now = GetWorld()->GetTimeSeconds();
	RecentHits.RemoveAll([Now](const FZSHitInfo& H) { return Now - H.Time > 1.2f; });
}

void UZSWeaponComponent::ApplyControlDelta(const FRotator& Delta)
{
	if (APlayerController* PC = GetPC())
	{
		FRotator R = PC->GetControlRotation();
		R.Pitch = FRotator::ClampAxis(R.Pitch + Delta.Pitch);
		R.Yaw += Delta.Yaw;
		// Respect the camera manager's pitch limits.
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->LimitViewPitch(R, PC->PlayerCameraManager->ViewPitchMin, PC->PlayerCameraManager->ViewPitchMax);
		}
		PC->SetControlRotation(R);
	}
}

// ---- State machine --------------------------------------------------------------------------

void UZSWeaponComponent::UpdateStateMachine(float DeltaTime)
{
	AZSWeaponBase* W = GetCurrentWeapon();
	StateTime += DeltaTime;

	switch (State)
	{
	case EZSWeaponState::Equipping:
		EquipAlpha = FMath::Clamp(StateTime / FMath::Max(StateDuration, 0.01f), 0.f, 1.f);
		if (StateTime >= StateDuration)
		{
			State = EZSWeaponState::Idle;
			EquipAlpha = 1.f;
		}
		break;

	case EZSWeaponState::Unequipping:
		EquipAlpha = 1.f - FMath::Clamp(StateTime / FMath::Max(StateDuration, 0.01f), 0.f, 1.f);
		if (StateTime >= StateDuration)
		{
			EquipIndex(PendingIndex != INDEX_NONE ? PendingIndex : CurrentIndex, false);
		}
		break;

	case EZSWeaponState::Reloading:
		if (!bReloadCommitted && StateTime >= StateDuration * W->Stats.ReloadCommitFraction)
		{
			W->CommitReload(bReloadWasEmpty);
			bReloadCommitted = true;
		}
		if (StateTime >= StateDuration)
		{
			State = EZSWeaponState::Idle;
			W->SetMagazineOffset(FVector::ZeroVector, FRotator::ZeroRotator);
			if (UZSFPAnimInstance* G = W->GetWeaponAnim()) { G->StopAction(0.1f); }
		}
		break;

	default:
		break;
	}

	// Auto-reload shortly after running dry.
	if (State == EZSWeaponState::Idle && W->GetAmmoInMag() == 0 && W->CanReload())
	{
		EmptyTime += DeltaTime;
		if (EmptyTime > 0.25f)
		{
			BeginReload();
		}
	}
	else
	{
		EmptyTime = 0.f;
	}
}

void UZSWeaponComponent::BeginReload()
{
	AZSWeaponBase* W = GetCurrentWeapon();
	if (!W || State != EZSWeaponState::Idle || !W->CanReload())
	{
		return;
	}
	if (AZSPlayerCharacter* Char = GetCharacter(); Char && Char->GetZSMovement() && Char->GetZSMovement()->IsSprinting())
	{
		// Reloading is allowed while sprinting; the sprint pose blends out for the reload motion.
	}
	bReloadWasEmpty = W->GetAmmoInMag() == 0;
	bReloadCommitted = false;
	State = EZSWeaponState::Reloading;
	StateTime = 0.f;
	StateDuration = bReloadWasEmpty ? W->Stats.EmptyReloadTime : W->Stats.TacticalReloadTime;
	BurstRemaining = 0;
	if (W->IsAnimated())
	{
		const FZSLoadedAnimSet& L = W->GetLoadedAnims();
		UAnimSequence* ArmsClip = bReloadWasEmpty && L.ReloadEmpty ? L.ReloadEmpty : L.Reload;
		UAnimSequence* GunClip = bReloadWasEmpty && L.WeaponReloadEmpty ? L.WeaponReloadEmpty : L.WeaponReload;
		const UAnimMontage* SoundSource = bReloadWasEmpty && L.ReloadEmptySoundMontage ? L.ReloadEmptySoundMontage : L.ReloadSoundMontage;
		const TArray<FZSActionSound>& Timed = bReloadWasEmpty ? L.ReloadEmptySounds : L.ReloadSounds;
		if (ArmsClip)
		{
			// Play the pack clip at whatever rate fits the weapon's reload time.
			const float Rate = ArmsClip->GetPlayLength() / FMath::Max(StateDuration, 0.1f);
			if (UZSFPAnimInstance* ArmsAnim = W->GetArmsAnim())
			{
				ArmsAnim->PlayAction(ArmsClip, Rate, 0.12f, 0.25f, nullptr, &Timed);
			}
			if (UZSFPAnimInstance* GunAnim = W->GetWeaponAnim())
			{
				GunAnim->PlayAction(GunClip, Rate, 0.05f, 0.2f, SoundSource);
			}
		}
	}
	if (W->ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, W->ReloadSound, W->GetActorLocation());
	}
}

void UZSWeaponComponent::CancelReload()
{
	if (State == EZSWeaponState::Reloading)
	{
		State = EZSWeaponState::Idle;
		if (AZSWeaponBase* W = GetCurrentWeapon())
		{
			if (W->IsAnimated())
			{
				if (UZSFPAnimInstance* A = W->GetArmsAnim()) { A->StopAction(0.15f); }
				if (UZSFPAnimInstance* G = W->GetWeaponAnim()) { G->StopAction(0.1f); }
			}
			W->SetMagazineOffset(FVector::ZeroVector, FRotator::ZeroRotator);
		}
	}
}

// ---- ADS ------------------------------------------------------------------------------------

void UZSWeaponComponent::UpdateADS(float DeltaTime)
{
	AZSWeaponBase* W = GetCurrentWeapon();
	AZSPlayerCharacter* Char = GetCharacter();
	const bool bSprinting = Char->GetZSMovement() && Char->GetZSMovement()->IsSprinting();
	const bool bCanADS = bADSHeld && !bSprinting && LowerAlpha < 0.5f &&
		State != EZSWeaponState::Reloading && State != EZSWeaponState::Unequipping;

	const float Rate = DeltaTime / FMath::Max(W->Stats.ADSTime, 0.05f);
	ADSAlpha = FMath::Clamp(ADSAlpha + (bCanADS ? Rate : -Rate * 1.3f), 0.f, 1.f);

	// ADS snap (aim assist) on the frame aiming starts.
	if (bCanADS && !bWasADSActive && IsUsingGamepad())
	{
		const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
		FVector Point; float Angle, Cone;
		if (Settings->bAimAssist && FindAimAssistTarget(Point, Angle, Cone) && Angle <= SnapAngle)
		{
			const FVector Eye = Char->GetFirstPersonCamera()->GetComponentLocation();
			const FRotator Want = (Point - Eye).Rotation();
			const FRotator Have = GetPC()->GetControlRotation();
			SnapRemaining = FRotator(FMath::FindDeltaAngleDegrees(Have.Pitch, Want.Pitch), FMath::FindDeltaAngleDegrees(Have.Yaw, Want.Yaw), 0.f)
				* (SnapStrength * FMath::Clamp(Settings->AimAssistStrength, 0.f, 1.5f));
			SnapTimeLeft = SnapTime;
		}
	}
	bWasADSActive = bCanADS;

	Char->GetFirstPersonCamera()->SetADSFOVMultiplier(FMath::Lerp(1.f, W->Stats.ADSFOVMultiplier, EaseInOut(ADSAlpha)), ADSAlpha);
	if (UZSMovementComponent* Move = Char->GetZSMovement())
	{
		Move->SetExternalSpeedScale(FMath::Lerp(1.f, W->Stats.ADSMoveSpeedMultiplier, ADSAlpha));
	}
}

// ---- Firing ---------------------------------------------------------------------------------

bool UZSWeaponComponent::CanFireNow() const
{
	const AZSWeaponBase* W = GetCurrentWeapon();
	const AZSPlayerCharacter* Char = GetCharacter();
	if (!W || !Char || State != EZSWeaponState::Idle)
	{
		return false;
	}
	const UZSMovementComponent* Move = Char->GetZSMovement();
	if (Move && (Move->IsSprinting() || Move->IsMantling()))
	{
		return false;
	}
	return TimeSinceSprint >= W->Stats.SprintToFireTime;
}

void UZSWeaponComponent::UpdateFiring(float DeltaTime)
{
	AZSWeaponBase* W = GetCurrentWeapon();
	TimeSinceLastShot += DeltaTime;
	if (bFireQueued)
	{
		FireQueuedTime += DeltaTime;
		if (FireQueuedTime > 0.2f && !bTriggerHeld)
		{
			bFireQueued = false;   // stale tap
		}
	}

	if (bTailPending && TimeSinceLastShot > W->Stats.GetShotInterval() * 1.6f)
	{
		bTailPending = false;
		W->PlayFireTail();
	}

	if (TimeSinceLastShot > FMath::Max(W->Stats.GetShotInterval() * 2.5f, 0.25f))
	{
		ShotsInSequence = 0;
	}

	if (!CanFireNow())
	{
		return;
	}

	const float Interval = W->Stats.GetShotInterval();

	// Continue an active burst.
	if (BurstRemaining > 0)
	{
		if (TimeSinceLastShot >= Interval)
		{
			if (W->GetAmmoInMag() > 0)
			{
				FireShot();
				--BurstRemaining;
			}
			else
			{
				BurstRemaining = 0;
			}
		}
		return;
	}

	const EZSFireMode Mode = W->GetFireMode();
	bool bWantsShot = false;
	switch (Mode)
	{
	case EZSFireMode::Auto:
		bWantsShot = bTriggerHeld && TimeSinceLastShot >= Interval;
		break;
	case EZSFireMode::Semi:
		bWantsShot = bFireQueued && TimeSinceLastShot >= FMath::Max(Interval, 60.f / FMath::Max(W->Stats.SemiMaxRPM, 1.f));
		break;
	case EZSFireMode::Burst:
		bWantsShot = bFireQueued && TimeSinceLastShot >= Interval * 2.f;
		break;
	}

	if (!bWantsShot)
	{
		return;
	}

	if (W->GetAmmoInMag() <= 0)
	{
		if (!bDryFired)
		{
			bDryFired = true;
			W->PlayDryFire();
			BeginReload();
		}
		bFireQueued = false;
		return;
	}

	bFireQueued = false;
	FireShot();
	if (Mode == EZSFireMode::Burst)
	{
		BurstRemaining = FMath::Max(0, W->Stats.BurstCount - 1);
	}
}

void UZSWeaponComponent::FireShot()
{
	AZSWeaponBase* W = GetCurrentWeapon();
	AZSPlayerCharacter* Char = GetCharacter();
	if (!W || !Char || !W->ConsumeRound())
	{
		return;
	}

	TimeSinceLastShot = 0.f;
	++ShotsInSequence;

	const FZSWeaponStats& S = W->Stats;
	const UCameraComponent* Cam = Char->GetFirstPersonCamera();
	const FVector Eye = Cam->GetComponentLocation();
	const FVector Forward = GetPC()->GetControlRotation().Vector();
	const float SpreadRad = FMath::DegreesToRadians(CurrentSpread);

	for (int32 Pellet = 0; Pellet < FMath::Max(1, S.PelletCount); ++Pellet)
	{
		const FVector Dir = SpreadRad > KINDA_SMALL_NUMBER ? FMath::VRandCone(Forward, SpreadRad) : Forward;
		TraceBullet(Eye, Dir, S.Damage);
	}

	// Recoil: next step of the pattern, scaled down while aiming.
	FVector2D Step(0.f, 0.5f);
	if (S.RecoilPattern.Num() > 0)
	{
		Step = S.RecoilPattern[(ShotsInSequence - 1) % S.RecoilPattern.Num()];
	}
	Step.X += FMath::FRandRange(-S.RecoilRandomYaw, S.RecoilRandomYaw);
	Step *= FMath::Lerp(1.f, S.RecoilADSScale, ADSAlpha);
	PendingRecoil += Step;

	Bloom = FMath::Min(Bloom + S.SpreadPerShot, S.MaxBloom);

	// View-model kick.
	KickBackVel += S.VisualKickBack * 60.f * FMath::Lerp(1.f, 0.5f, ADSAlpha);
	KickPitchVel += S.VisualKickPitch * 60.f * FMath::Lerp(1.f, 0.4f, ADSAlpha);

	W->PlayMuzzleFlash();
	if (W->IsAnimated())
	{
		const bool bAimed = ADSAlpha > 0.5f;
		W->PlayShotEffects(bAimed);
		const FZSLoadedAnimSet& L = W->GetLoadedAnims();
		if (UZSFPAnimInstance* ArmsAnim = W->GetArmsAnim())
		{
			UAnimSequence* FireAnim = bAimed && L.FireAimed ? L.FireAimed : L.Fire;
			ArmsAnim->PlayAction(FireAnim, 1.f, 0.025f, 0.18f);
		}
		bTailPending = true;
	}
	else if (W->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, W->FireSound, W->GetMuzzleWorldLocation());
	}
	Rumble(S.WeaponClass == EZSWeaponClass::Pistol ? 0.25f : 0.35f, 0.07f);
}

void UZSWeaponComponent::TraceBullet(const FVector& Start, const FVector& Direction, float BaseDamage)
{
	UWorld* World = GetWorld();
	AZSPlayerCharacter* Char = GetCharacter();
	AZSWeaponBase* W = GetCurrentWeapon();
	const FZSWeaponStats& S = W->Stats;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSWeaponTrace), true, Char);
	for (AZSWeaponBase* Carried : Weapons)
	{
		Params.AddIgnoredActor(Carried);
	}

	const int32 DebugMode = CVarZSShowImpacts.GetValueOnGameThread();
	FVector SegmentStart = Start;
	float Travelled = 0.f;
	float Damage = BaseDamage;
	float DepthLeft = S.PenetrationDepth;
	int32 Penetrations = 0;

	while (Travelled < S.MaxRange)
	{
		const FVector End = SegmentStart + Direction * (S.MaxRange - Travelled);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, SegmentStart, End, ZS_TRACE_WEAPON, Params))
		{
			if (DebugMode >= 2)
			{
				DrawDebugLine(World, SegmentStart, End, FColor::Yellow, false, 0.05f, 0, 0.5f);
			}
			break;
		}

		Travelled += Hit.Distance;
		if (DebugMode >= 2)
		{
			DrawDebugLine(World, SegmentStart, Hit.ImpactPoint, FColor::Yellow, false, 0.05f, 0, 0.5f);
		}

		// Range falloff on total distance from the eye.
		const float FalloffAlpha = FMath::GetMappedRangeValueClamped(FVector2D(S.FalloffStart, FMath::Max(S.FalloffEnd, S.FalloffStart + 1.f)), FVector2D(0.f, 1.f), Travelled);
		const float RangeMultiplier = FMath::Lerp(1.f, S.MinDamageMultiplier, FalloffAlpha);

		AActor* HitActor = Hit.GetActor();
		UZSHealthComponent* Health = UZSHealthComponent::FindOn(HitActor);

		if (Health)
		{
			// Hit zone from bone name or component tag.
			const FString Bone = Hit.BoneName.ToString().ToLower();
			const UPrimitiveComponent* Comp = Hit.GetComponent();
			const bool bHead = Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("neck")) || (Comp && Comp->ComponentHasTag(TEXT("Head")));
			const bool bLimb = !bHead && (Bone.Contains(TEXT("arm")) || Bone.Contains(TEXT("hand")) || Bone.Contains(TEXT("thigh")) ||
				Bone.Contains(TEXT("calf")) || Bone.Contains(TEXT("foot")) || (Comp && Comp->ComponentHasTag(TEXT("Limb"))));
			const float ZoneMultiplier = bHead ? S.HeadshotMultiplier : (bLimb ? S.LimbMultiplier : 1.f);
			const float Final = Damage * RangeMultiplier * ZoneMultiplier;

			const bool bWasAlive = !Health->IsDead();
			UGameplayStatics::ApplyPointDamage(HitActor, Final, Direction, Hit, GetPC(), W, UDamageType::StaticClass());

			if (bWasAlive)
			{
				FZSHitInfo Info;
				Info.Location = Hit.ImpactPoint;
				Info.Damage = Final;
				Info.bHeadshot = bHead;
				Info.bKill = Health->IsDead();
				Info.Time = World->GetTimeSeconds();
				RecentHits.Add(Info);
				LastHitTime = Info.Time;
				OnWeaponHit.Broadcast(Info);
				Rumble(Info.bKill ? 0.6f : 0.2f, Info.bKill ? 0.15f : 0.05f);
			}

			// Bullets pass through bodies (zombie piles), losing damage.
			Params.AddIgnoredActor(HitActor);
			SegmentStart = Hit.ImpactPoint + Direction * 1.f;
			Damage *= S.PenetrationDamageMultiplier;
			if (++Penetrations > S.MaxPenetrations)
			{
				break;
			}
			continue;
		}

		if (DebugMode >= 1)
		{
			DrawDebugPoint(World, Hit.ImpactPoint, 6.f, FColor(255, 180, 40), false, 2.5f);
		}

		// World geometry: measure thickness by tracing back from the far side.
		if (Penetrations >= S.MaxPenetrations || DepthLeft <= 0.f || !Hit.GetComponent())
		{
			break;
		}
		const FVector BackStart = Hit.ImpactPoint + Direction * DepthLeft;
		FHitResult Exit;
		FCollisionQueryParams BackParams(SCENE_QUERY_STAT(ZSWeaponExit), true);
		if (!Hit.GetComponent()->LineTraceComponent(Exit, BackStart, Hit.ImpactPoint + Direction * 0.1f, BackParams))
		{
			break;   // thicker than what's left
		}
		const float Thickness = FVector::Dist(Hit.ImpactPoint, Exit.ImpactPoint);
		DepthLeft -= Thickness;
		Damage *= S.PenetrationDamageMultiplier;
		++Penetrations;
		Travelled += Thickness;
		SegmentStart = Exit.ImpactPoint + Direction * 0.5f;
		if (DebugMode >= 1)
		{
			DrawDebugPoint(World, Exit.ImpactPoint, 5.f, FColor(120, 200, 255), false, 2.5f);
		}
	}
}

// ---- Spread ---------------------------------------------------------------------------------

void UZSWeaponComponent::UpdateSpread(float DeltaTime)
{
	const AZSWeaponBase* W = GetCurrentWeapon();
	const AZSPlayerCharacter* Char = GetCharacter();
	const UZSMovementComponent* Move = Char->GetZSMovement();
	const FZSWeaponStats& S = W->Stats;

	if (TimeSinceLastShot > 0.05f)
	{
		Bloom = FMath::Max(0.f, Bloom - S.BloomRecovery * DeltaTime);
	}

	const float SpeedAlpha = Move ? FMath::Clamp(Move->Velocity.Size2D() / FMath::Max(Move->JogSpeed, 1.f), 0.f, 1.f) : 0.f;
	const bool bAir = Move && Move->IsFalling() && !Move->IsWallRunning();
	const float Base = FMath::Lerp(S.HipSpread, S.ADSSpread, ADSAlpha);
	const float MoveTerm = S.MoveSpread * SpeedAlpha * FMath::Lerp(1.f, 0.25f, ADSAlpha);
	const float AirTerm = bAir ? S.AirSpread * FMath::Lerp(1.f, 0.4f, ADSAlpha) : 0.f;
	const float BloomTerm = Bloom * FMath::Lerp(1.f, 0.35f, ADSAlpha);
	const float Target = Base + MoveTerm + AirTerm + BloomTerm;
	CurrentSpread = FMath::FInterpTo(CurrentSpread, Target, DeltaTime, 18.f);
}

// ---- Recoil ---------------------------------------------------------------------------------

void UZSWeaponComponent::UpdateRecoil(float DeltaTime, const FRotator& PlayerLookDelta)
{
	const FZSWeaponStats& S = GetCurrentWeapon()->Stats;

	// Player pulling against the recoil cancels it (so it won't "recover" past where they aimed).
	if (RecoilOffset.Y > 0.f && PlayerLookDelta.Pitch < 0.f)
	{
		RecoilOffset.Y = FMath::Max(0.f, RecoilOffset.Y + PlayerLookDelta.Pitch);
	}
	if (!FMath::IsNearlyZero(RecoilOffset.X) && FMath::Sign(PlayerLookDelta.Yaw) == -FMath::Sign(RecoilOffset.X))
	{
		RecoilOffset.X = FMath::Sign(RecoilOffset.X) * FMath::Max(0.f, FMath::Abs(RecoilOffset.X) - FMath::Abs(PlayerLookDelta.Yaw));
	}

	// Apply pending kick quickly.
	if (!PendingRecoil.IsNearlyZero(0.001f))
	{
		const FVector2D Apply = PendingRecoil * FMath::Min(1.f, S.RecoilApplySpeed * DeltaTime);
		PendingRecoil -= Apply;
		RecoilOffset += Apply;
		RecoilPeak.X = FMath::Abs(RecoilOffset.X) > FMath::Abs(RecoilPeak.X) ? RecoilOffset.X : RecoilPeak.X;
		RecoilPeak.Y = FMath::Max(RecoilPeak.Y, RecoilOffset.Y);
		ApplyControlDelta(FRotator(Apply.Y, Apply.X, 0.f));
		return;
	}

	// Recover after the player stops firing.
	if (TimeSinceLastShot < S.RecoilRecoveryDelay || RecoilOffset.IsNearlyZero(0.001f))
	{
		return;
	}
	const float Floor = RecoilPeak.Y * (1.f - S.RecoilRecoveryFraction);
	const float StepPitch = FMath::Min(S.RecoilRecoveryRate * DeltaTime, FMath::Max(0.f, RecoilOffset.Y - Floor));
	const float StepYaw = FMath::Sign(RecoilOffset.X) * FMath::Min(S.RecoilRecoveryRate * 0.5f * DeltaTime, FMath::Abs(RecoilOffset.X));
	RecoilOffset.Y -= StepPitch;
	RecoilOffset.X -= StepYaw;
	ApplyControlDelta(FRotator(-StepPitch, -StepYaw, 0.f));

	if (StepPitch <= KINDA_SMALL_NUMBER && FMath::Abs(StepYaw) <= KINDA_SMALL_NUMBER)
	{
		RecoilOffset = FVector2D::ZeroVector;
		RecoilPeak = FVector2D::ZeroVector;
	}
}

// ---- Aim assist -----------------------------------------------------------------------------

bool UZSWeaponComponent::IsUsingGamepad() const
{
	if (const APlayerController* PC = GetPC())
	{
		if (const ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (const UCommonInputSubsystem* Input = LP->GetSubsystem<UCommonInputSubsystem>())
			{
				return Input->GetCurrentInputType() == ECommonInputType::Gamepad;
			}
		}
	}
	return false;
}

bool UZSWeaponComponent::FindAimAssistTarget(FVector& OutPoint, float& OutAngle, float& OutCone) const
{
	UWorld* World = GetWorld();
	UZSAimAssistSubsystem* Registry = World ? World->GetSubsystem<UZSAimAssistSubsystem>() : nullptr;
	const AZSPlayerCharacter* Char = GetCharacter();
	if (!Registry || !Char)
	{
		return false;
	}

	const FVector Eye = Char->GetFirstPersonCamera()->GetComponentLocation();
	const FVector Forward = GetPC()->GetControlRotation().Vector();
	const float BaseAngle = FMath::Lerp(AssistAngleHip, AssistAngleADS, ADSAlpha);

	float BestScore = 1.f;
	bool bFound = false;
	for (const FZSAimTarget& T : Registry->GetTargets())
	{
		if (const UZSHealthComponent* H = UZSHealthComponent::FindOn(T.Actor.Get()); H && H->IsDead())
		{
			continue;
		}
		// Prefer the head in ADS when it is close to the reticle; otherwise centre mass.
		USceneComponent* Points[2] = { T.Head.Get(), T.Body.Get() };
		for (USceneComponent* P : Points)
		{
			if (!P)
			{
				continue;
			}
			const FVector ToPoint = P->GetComponentLocation() - Eye;
			const float Dist = ToPoint.Size();
			if (Dist > AssistMaxRange || Dist < 50.f)
			{
				continue;
			}
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, ToPoint / Dist), -1.f, 1.f)));
			const float Cone = BaseAngle + FMath::RadiansToDegrees(FMath::Atan(T.Radius / Dist));
			const float Score = Angle / Cone;
			if (Score >= BestScore)
			{
				continue;
			}
			// Line of sight.
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSAimAssist), false, Char);
			if (World->LineTraceSingleByChannel(Hit, Eye, P->GetComponentLocation(), ZS_TRACE_WEAPON, Params) && Hit.GetActor() != T.Actor.Get())
			{
				continue;
			}
			BestScore = Score;
			OutPoint = P->GetComponentLocation();
			OutAngle = Angle;
			OutCone = Cone;
			bFound = true;
		}
	}
	return bFound;
}

void UZSWeaponComponent::UpdateAimAssist(float DeltaTime, const FRotator& PlayerLookDelta)
{
	FrictionScale = 1.f;

	// ADS snap plays out over SnapTime.
	if (SnapTimeLeft > 0.f)
	{
		const float Step = FMath::Min(DeltaTime, SnapTimeLeft) / FMath::Max(SnapTimeLeft, KINDA_SMALL_NUMBER);
		const FRotator Apply = SnapRemaining * Step;
		SnapRemaining -= Apply;
		SnapTimeLeft -= DeltaTime;
		ApplyControlDelta(Apply);
	}

	const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	if (!Settings->bAimAssist || Settings->AimAssistStrength <= 0.f || !IsUsingGamepad())
	{
		return;
	}

	FVector Point; float Angle, Cone;
	if (!FindAimAssistTarget(Point, Angle, Cone))
	{
		return;
	}
	const float Strength = FMath::Clamp(Settings->AimAssistStrength, 0.f, 1.5f);
	const float Closeness = 1.f - FMath::Clamp(Angle / Cone, 0.f, 1.f);

	// Friction: slow the stick near the target.
	FrictionScale = 1.f - FrictionStrength * Strength * Closeness;

	// Tracking: only while the player is actively moving or aiming, never on its own.
	const AZSPlayerCharacter* Char = GetCharacter();
	const bool bPlayerActive = !PlayerLookDelta.IsNearlyZero(0.01f) || Char->GetVelocity().Size2D() > 50.f;
	if (!bPlayerActive)
	{
		return;
	}
	const FVector Eye = Char->GetFirstPersonCamera()->GetComponentLocation();
	const FRotator Want = (Point - Eye).Rotation();
	const FRotator Have = GetPC()->GetControlRotation();
	const FRotator Error(FMath::FindDeltaAngleDegrees(Have.Pitch, Want.Pitch), FMath::FindDeltaAngleDegrees(Have.Yaw, Want.Yaw), 0.f);
	const float Rate = FMath::Lerp(TrackingRateHip, TrackingRateADS, ADSAlpha) * Strength * Closeness;
	ApplyControlDelta(Error * FMath::Min(1.f, Rate * DeltaTime));
}

// ---- View model -----------------------------------------------------------------------------

void UZSWeaponComponent::UpdateViewModel(float DeltaTime, const FRotator& PlayerLookDelta)
{
	AZSWeaponBase* W = GetCurrentWeapon();
	AZSPlayerCharacter* Char = GetCharacter();
	if (!ViewModelRoot || !W)
	{
		return;
	}
	if (W->IsAnimated())
	{
		UpdateAnimatedViewModel(DeltaTime, PlayerLookDelta);
		return;
	}
	const FZSWeaponStats& S = W->Stats;
	const float AdsEase = EaseInOut(ADSAlpha);
	const UZSGameUserSettings* Settings = UZSGameUserSettings::Get();
	const float Comfort = FMath::Clamp(Settings->CameraEffectsScale, 0.f, 1.f);

	// Hip vs ADS placement. At ADS the sight point sits on the camera axis, ADSEyeRelief ahead of the eye.
	const FVector HipLoc = S.HipOffset;
	const FVector AdsLoc = FVector(S.ADSEyeRelief, 0.f, 0.f) - S.SightPoint;
	FVector Loc = FMath::Lerp(HipLoc, AdsLoc, AdsEase);
	FRotator Rot = FMath::Lerp(S.HipRotation, FRotator::ZeroRotator, AdsEase);

	// Sway: the gun lags behind look motion.
	const float YawRate = PlayerLookDelta.Yaw / DeltaTime;
	const float PitchRate = PlayerLookDelta.Pitch / DeltaTime;
	const float SwayScale = S.SwayScale * FMath::Lerp(1.f, 0.2f, AdsEase) * Comfort;
	const FRotator SwayTarget(
		FMath::Clamp(-PitchRate * SwayPerDegPerSec, -SwayMax, SwayMax) * SwayScale,
		FMath::Clamp(-YawRate * SwayPerDegPerSec, -SwayMax, SwayMax) * SwayScale,
		FMath::Clamp(-YawRate * SwayPerDegPerSec * 0.8f, -SwayMax, SwayMax) * SwayScale);
	SwayRotation = FMath::RInterpTo(SwayRotation, SwayTarget, DeltaTime, SwayInterpSpeed);
	Rot += SwayRotation;
	Loc.Y += SwayRotation.Yaw * 0.15f;
	Loc.Z += SwayRotation.Pitch * 0.12f;

	// Walk bob (figure eight), stronger when sprinting, almost none when aiming.
	const UZSMovementComponent* Move = Char->GetZSMovement();
	const bool bGrounded = Move && Move->IsMovingOnGround() && !Move->IsSliding();
	const float Speed = Move ? Move->Velocity.Size2D() : 0.f;
	const float SpeedAlpha = bGrounded ? FMath::Clamp(Speed / FMath::Max(Move->JogSpeed, 1.f), 0.f, 1.6f) : 0.f;
	BobPhase += DeltaTime * (6.f + 5.f * SpeedAlpha) * (SpeedAlpha > 0.05f ? 1.f : 0.3f);
	const float BobAmp = BobAmount * (0.15f + SpeedAlpha) * FMath::Lerp(1.f, 0.12f, AdsEase) * Comfort;
	Loc.Z += FMath::Sin(BobPhase * 2.f) * BobAmp;
	Loc.Y += FMath::Sin(BobPhase) * BobAmp * 1.2f;
	Rot.Roll += FMath::Sin(BobPhase) * BobAmp * 1.5f;

	// Sprint / mantle poses.
	Loc += SprintPoseOffset * SprintAlpha;
	Rot += SprintPoseRotation * SprintAlpha;
	Loc += FVector(-6.f, 0.f, -18.f) * LowerAlpha;
	Rot.Pitch -= 35.f * LowerAlpha;

	// Equip / unequip: rise from below.
	const float Hidden = 1.f - EaseInOut(EquipAlpha);
	Loc.Z -= 28.f * Hidden;
	Rot.Pitch -= 55.f * Hidden;

	// Procedural reload: tilt the gun, drop the mag out and bring a new one in.
	FVector MagOffset = FVector::ZeroVector;
	float LeftHandToMag = 0.f;
	if (State == EZSWeaponState::Reloading)
	{
		const float T = GetReloadProgress();
		const float Tilt = Window(T, 0.05f, 0.95f, 0.15f);
		Rot.Roll += 22.f * Tilt;
		Rot.Pitch += 8.f * Tilt;
		Rot.Yaw -= 6.f * Tilt;
		Loc += FVector(-3.f, -2.f, -4.f) * Tilt;

		const float Commit = S.ReloadCommitFraction;
		const float Out = FMath::Clamp((T - 0.12f) / 0.18f, 0.f, 1.f);          // old mag out
		const float In = FMath::Clamp((T - (Commit - 0.22f)) / 0.2f, 0.f, 1.f);   // new mag in
		const float MagDown = T < Commit - 0.22f ? EaseInOut(Out) : 1.f - EaseInOut(In);
		MagOffset = FVector(0.f, 0.f, -30.f) * MagDown;
		LeftHandToMag = Window(T, 0.1f, Commit + 0.08f, 0.1f);
		// An empty reload also racks the action at the end.
		if (bReloadWasEmpty)
		{
			const float Rack = Window(T, 0.78f, 0.93f, 0.05f);
			Loc.X -= 2.5f * Rack;
			Rot.Roll -= 6.f * Rack;
		}
	}
	W->SetMagazineOffset(MagOffset, FRotator::ZeroRotator);

	// Firing kick (springs).
	SpringStep(KickBack, KickBackVel, 380.f, 24.f, DeltaTime);
	SpringStep(KickPitch, KickPitchVel, 300.f, 20.f, DeltaTime);
	Loc.X -= KickBack;
	Rot.Pitch += KickPitch;

	// Lock to the camera's bob / roll so the gun moves with the view.
	const FTransform Feel = Char->GetFirstPersonCamera()->GetViewFeelLocalTransform();
	const FTransform ViewModel(Rot, Loc);
	ViewModelRoot->SetRelativeTransform(ViewModel * Feel);

	// ---- Arms: IK onto the grips ----
	if (Arms && Arms->HasArms())
	{
		const FTransform WeaponToWorld = W->GetWeaponSpaceToWorld();
		auto MakeTarget = [&WeaponToWorld](const FZSHandGrip& G)
		{
			FZSArmTarget T;
			T.Wrist = WeaponToWorld.TransformPosition(G.Location);
			T.FingerDirection = WeaponToWorld.TransformVectorNoScale(G.FingerDirection.GetSafeNormal());
			T.PalmNormal = WeaponToWorld.TransformVectorNoScale(G.PalmNormal.GetSafeNormal());
			T.ElbowHint = T.Wrist + WeaponToWorld.TransformVectorNoScale(G.ElbowHint);
			T.FingerCurl = G.FingerCurl;
			T.IndexCurl = G.IndexCurl;
			T.ThumbCurl = G.ThumbCurl;
			return T;
		};

		const FZSArmTarget Right = MakeTarget(S.RightHand);
		FZSArmTarget Left = MakeTarget(S.LeftHand);
		if (LeftHandToMag > 0.f)
		{
			// Reach down to the magazine.
			FZSHandGrip MagGrip = S.LeftHand;
			MagGrip.Location = FVector(0.f, -5.f, -2.f) + MagOffset;
			MagGrip.FingerDirection = FVector(0.3f, 0.6f, -0.7f);
			MagGrip.PalmNormal = FVector(0.f, 1.f, 0.f);
			const FZSArmTarget MagTarget = MakeTarget(MagGrip);
			Left.Wrist = FMath::Lerp(Left.Wrist, MagTarget.Wrist, LeftHandToMag);
			Left.FingerDirection = FMath::Lerp(Left.FingerDirection, MagTarget.FingerDirection, LeftHandToMag).GetSafeNormal();
			Left.PalmNormal = FMath::Lerp(Left.PalmNormal, MagTarget.PalmNormal, LeftHandToMag).GetSafeNormal();
		}
		Arms->SetVisibility(!W->IsHidden());
		Arms->UpdateArms(Right, Left);
	}
}

void UZSWeaponComponent::Rumble(float Intensity, float Duration) const
{
	if (APlayerController* PC = GetPC())
	{
		PC->PlayDynamicForceFeedback(Intensity, Duration, true, true, true, true);
	}
}

// ---- Animated weapons -----------------------------------------------------------------------

void UZSWeaponComponent::SetupArmsFor(AZSWeaponBase* Weapon)
{
	const bool bAnimated = Weapon && Weapon->IsAnimated();
	if (AnimatedArms)
	{
		if (bAnimated)
		{
			if (!AnimatedArms->GetSkinnedAsset() && Arms)
			{
				AnimatedArms->SetSkeletalMeshAsset(Arms->ArmsMesh.LoadSynchronous());
			}
			// Same component transform as the driver, bones copied by name every frame.
			AnimatedArms->AttachToComponent(Weapon->GetDriverMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			AnimatedArms->SetLeaderPoseComponent(Weapon->GetDriverMesh());
			AnimatedArms->SetVisibility(AnimatedArms->GetSkinnedAsset() != nullptr);
		}
		else
		{
			AnimatedArms->SetLeaderPoseComponent(nullptr);
			AnimatedArms->SetVisibility(false);
		}
	}
	if (Arms)
	{
		Arms->SetVisibility(!bAnimated && Arms->HasArms());
	}
}

void UZSWeaponComponent::UpdateAnimatedViewModel(float DeltaTime, const FRotator& PlayerLookDelta)
{
	AZSWeaponBase* W = GetCurrentWeapon();
	AZSPlayerCharacter* Char = GetCharacter();
	const FZSWeaponStats& S = W->Stats;
	const float AdsEase = EaseInOut(ADSAlpha);
	const float Comfort = FMath::Clamp(UZSGameUserSettings::Get()->CameraEffectsScale, 0.f, 1.f);

	// Drive the clips.
	const UZSMovementComponent* Move = Char->GetZSMovement();
	const bool bGrounded = Move && Move->IsMovingOnGround() && !Move->IsSliding();
	const float Speed01 = bGrounded ? Move->Velocity.Size2D() / FMath::Max(Move->JogSpeed, 1.f) : 0.f;
	if (UZSFPAnimInstance* ArmsAnim = W->GetArmsAnim())
	{
		ArmsAnim->SetLocomotionState(Speed01, SprintAlpha, AdsEase);
	}

	// The clips handle hip / aim / sprint / reload poses; code only adds look sway, a little kick and the mantle dip.
	FVector Loc = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;

	const float YawRate = PlayerLookDelta.Yaw / DeltaTime;
	const float PitchRate = PlayerLookDelta.Pitch / DeltaTime;
	const float SwayScale = S.SwayScale * FMath::Lerp(1.f, 0.15f, AdsEase) * Comfort;
	const FRotator SwayTarget(
		FMath::Clamp(-PitchRate * SwayPerDegPerSec, -SwayMax, SwayMax) * SwayScale,
		FMath::Clamp(-YawRate * SwayPerDegPerSec, -SwayMax, SwayMax) * SwayScale,
		FMath::Clamp(-YawRate * SwayPerDegPerSec * 0.6f, -SwayMax, SwayMax) * SwayScale);
	SwayRotation = FMath::RInterpTo(SwayRotation, SwayTarget, DeltaTime, SwayInterpSpeed);
	Rot += SwayRotation;

	Loc += FVector(-6.f, 0.f, -18.f) * LowerAlpha;
	Rot.Pitch -= 35.f * LowerAlpha;

	SpringStep(KickBack, KickBackVel, 380.f, 24.f, DeltaTime);
	SpringStep(KickPitch, KickPitchVel, 300.f, 20.f, DeltaTime);
	Loc.X -= KickBack;
	Rot.Pitch += KickPitch;

	const FTransform Feel = Char->GetFirstPersonCamera()->GetViewFeelLocalTransform();
	ViewModelRoot->SetRelativeTransform(FTransform(Rot, Loc) * Feel);

	W->UpdateDriverPlacement();
	if (AnimatedArms)
	{
		AnimatedArms->SetVisibility(!W->IsHidden() && AnimatedArms->GetSkinnedAsset() != nullptr);
	}
}
