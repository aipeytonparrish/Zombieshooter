#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapons/ZSWeaponTypes.h"
#include "ZSWeaponComponent.generated.h"

class AZSWeaponBase;
class AZSPlayerCharacter;
class UZSFirstPersonArmsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZSOnWeaponHit, const FZSHitInfo&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZSOnWeaponChanged, AZSWeaponBase*, Weapon);

/**
 * The player's gun handling: inventory, firing (hitscan + penetration + range falloff), fire modes, reloads,
 * recoil with player-counter and recovery, spread/bloom, ADS, controller aim assist, rumble, and the procedural
 * first-person view model (hip/ADS placement, sway, bob, sprint pose, kick, reload motion, equip motion) that
 * also drives the IK arms.
 */
UCLASS(ClassGroup = (ZS), meta = (BlueprintSpawnableComponent))
class ZOMBIESHOOTER_API UZSWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZSWeaponComponent();

	/** Weapons given at spawn (Milestone 8 replaces this with the deployed loadout). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Weapons")
	TArray<TSubclassOf<AZSWeaponBase>> DefaultWeapons;

	// ---- Input (called by the character) ----
	void SetTriggerHeld(bool bHeld);
	void SetADSHeld(bool bHeld);
	void RequestReload();
	void RequestSwitchWeapon();
	void RequestCycleFireMode();

	/** Multiplier the character applies to look input this frame (ADS sensitivity x aim-assist friction). */
	float GetLookInputScale(bool bGamepad) const;

	// ---- Queries (HUD) ----
	UFUNCTION(BlueprintPure, Category = "ZS|Weapons")
	AZSWeaponBase* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category = "ZS|Weapons")
	float GetADSAlpha() const { return ADSAlpha; }

	/** Current bullet cone half-angle in degrees. */
	UFUNCTION(BlueprintPure, Category = "ZS|Weapons")
	float GetCurrentSpread() const { return CurrentSpread; }

	UFUNCTION(BlueprintPure, Category = "ZS|Weapons")
	EZSWeaponState GetWeaponState() const { return State; }

	/** 0..1 progress of the current reload (0 when not reloading). */
	UFUNCTION(BlueprintPure, Category = "ZS|Weapons")
	float GetReloadProgress() const;

	UFUNCTION(BlueprintPure, Category = "ZS|Weapons")
	bool IsWeaponLowered() const;

	const TArray<FZSHitInfo>& GetRecentHits() const { return RecentHits; }
	float GetLastHitTime() const { return LastHitTime; }

	UPROPERTY(BlueprintAssignable, Category = "ZS|Weapons")
	FZSOnWeaponHit OnWeaponHit;

	UPROPERTY(BlueprintAssignable, Category = "ZS|Weapons")
	FZSOnWeaponChanged OnWeaponChanged;

	/** Refill every carried weapon (debug / ammo crates in Milestone 5). */
	UFUNCTION(BlueprintCallable, Category = "ZS|Weapons")
	void RefillAllAmmo();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Aim assist tuning (controller only) ----
	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist", meta = (ForceUnits = "deg"))
	float AssistAngleHip = 6.f;

	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist", meta = (ForceUnits = "deg"))
	float AssistAngleADS = 4.5f;

	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist", meta = (ForceUnits = "cm"))
	float AssistMaxRange = 6000.f;

	/** Look-speed reduction at the centre of a target (0.45 = 45 % slower). */
	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist")
	float FrictionStrength = 0.45f;

	/** Fraction of the angular error closed per second while the player is moving / looking. */
	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist")
	float TrackingRateHip = 1.2f;

	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist")
	float TrackingRateADS = 2.6f;

	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist", meta = (ForceUnits = "deg"))
	float SnapAngle = 9.f;

	/** Fraction of the error removed by the ADS snap. */
	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist")
	float SnapStrength = 0.6f;

	UPROPERTY(EditAnywhere, Category = "ZS|AimAssist", meta = (ForceUnits = "s"))
	float SnapTime = 0.14f;

	// ---- View model feel ----
	UPROPERTY(EditAnywhere, Category = "ZS|ViewModel")
	float SwayPerDegPerSec = 0.012f;

	UPROPERTY(EditAnywhere, Category = "ZS|ViewModel", meta = (ForceUnits = "deg"))
	float SwayMax = 6.f;

	UPROPERTY(EditAnywhere, Category = "ZS|ViewModel")
	float SwayInterpSpeed = 9.f;

	UPROPERTY(EditAnywhere, Category = "ZS|ViewModel")
	FVector SprintPoseOffset = FVector(-4.f, -5.f, -5.f);

	UPROPERTY(EditAnywhere, Category = "ZS|ViewModel")
	FRotator SprintPoseRotation = FRotator(-12.f, 28.f, -14.f);

	UPROPERTY(EditAnywhere, Category = "ZS|ViewModel")
	float BobAmount = 0.6f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ---- Setup ----
	void SpawnDefaultWeapons();
	void EquipIndex(int32 Index, bool bInstant);
public:
	/** Animated weapons: show the follower arms on the weapon's driver rig; otherwise the code-posed arms. */
	void SetupArmsFor(AZSWeaponBase* Weapon);
private:
	void UpdateAnimatedViewModel(float DeltaTime, const FRotator& PlayerLookDelta);

	// ---- Firing ----
	bool CanFireNow() const;
	void UpdateFiring(float DeltaTime);
	void FireShot();
	void TraceBullet(const FVector& Start, const FVector& Direction, float BaseDamage);
	void BeginReload();
	void CancelReload();

	// ---- Per-frame systems ----
	void UpdateStateMachine(float DeltaTime);
	void UpdateADS(float DeltaTime);
	void UpdateSpread(float DeltaTime);
	void UpdateRecoil(float DeltaTime, const FRotator& PlayerLookDelta);
	void UpdateAimAssist(float DeltaTime, const FRotator& PlayerLookDelta);
	void UpdateViewModel(float DeltaTime, const FRotator& PlayerLookDelta);
	void ApplyControlDelta(const FRotator& Delta);

	bool FindAimAssistTarget(FVector& OutPoint, float& OutAngle, float& OutCone) const;
	bool IsUsingGamepad() const;
	void Rumble(float Intensity, float Duration) const;

	AZSPlayerCharacter* GetCharacter() const;
	APlayerController* GetPC() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AZSWeaponBase>> Weapons;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ViewModelRoot;

	UPROPERTY(Transient)
	TObjectPtr<UZSFirstPersonArmsComponent> Arms;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> AnimatedArms;

	int32 CurrentIndex = INDEX_NONE;
	int32 PendingIndex = INDEX_NONE;
	EZSWeaponState State = EZSWeaponState::Idle;
	float StateTime = 0.f;
	float StateDuration = 0.f;
	bool bReloadWasEmpty = false;
	bool bReloadCommitted = false;

	// Trigger
	bool bTriggerHeld = false;
	bool bFireQueued = false;
	float FireQueuedTime = 0.f;
	int32 BurstRemaining = 0;
	float TimeSinceLastShot = 10.f;
	int32 ShotsInSequence = 0;
	bool bDryFired = false;
	bool bTailPending = false;
	float EmptyTime = 0.f;

	// ADS
	bool bADSHeld = false;
	bool bWasADSActive = false;
	float ADSAlpha = 0.f;

	// Sprint gating
	float TimeSinceSprint = 10.f;
	float SprintAlpha = 0.f;
	float LowerAlpha = 0.f;

	// Spread
	float Bloom = 0.f;
	float CurrentSpread = 0.f;

	// Recoil (X = yaw, Y = pitch, degrees)
	FVector2D PendingRecoil = FVector2D::ZeroVector;
	FVector2D RecoilOffset = FVector2D::ZeroVector;
	FVector2D RecoilPeak = FVector2D::ZeroVector;

	// Control rotation bookkeeping so player look input can be told apart from our own changes.
	FRotator LastControlRotation = FRotator::ZeroRotator;
	bool bHasLastControlRotation = false;

	// Aim assist
	float FrictionScale = 1.f;
	FRotator SnapRemaining = FRotator::ZeroRotator;
	float SnapTimeLeft = 0.f;

	// View model
	FRotator SwayRotation = FRotator::ZeroRotator;
	float BobPhase = 0.f;
	float EquipAlpha = 0.f;
	float KickBack = 0.f;
	float KickBackVel = 0.f;
	float KickPitch = 0.f;
	float KickPitchVel = 0.f;

	// Hits
	TArray<FZSHitInfo> RecentHits;
	float LastHitTime = -10.f;
};
