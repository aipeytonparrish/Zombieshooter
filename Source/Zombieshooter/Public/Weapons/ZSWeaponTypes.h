#pragma once

#include "CoreMinimal.h"
#include "ZSWeaponTypes.generated.h"

class UStaticMesh;

/** Collision channel used by every hitscan trace (DefaultEngine.ini: GameTraceChannel1 = "Weapon"). */
#define ZS_TRACE_WEAPON ECC_GameTraceChannel1

UENUM(BlueprintType)
enum class EZSFireMode : uint8
{
	Semi,
	Burst,
	Auto
};

UENUM(BlueprintType)
enum class EZSWeaponClass : uint8
{
	Pistol,
	SMG,
	AssaultRifle,
	Shotgun,
	LMG,
	Marksman
};

UENUM(BlueprintType)
enum class EZSWeaponState : uint8
{
	Idle,
	Equipping,
	Unequipping,
	Reloading
};

/**
 * Where one hand holds the weapon, in weapon space (X forward along the barrel, Y right, Z up, cm).
 * The arms are posed by two-bone IK onto Location (the wrist), then the hand is turned so its fingers
 * point along FingerDirection with the palm facing PalmNormal, then the fingers curl.
 */
USTRUCT(BlueprintType)
struct FZSHandGrip
{
	GENERATED_BODY()

	/** Wrist position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FVector Location = FVector::ZeroVector;

	/** Wrist-to-knuckles direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FVector FingerDirection = FVector::ForwardVector;

	/** Direction the palm faces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FVector PalmNormal = FVector::LeftVector;

	/** Curl per finger joint (degrees) for middle / ring / pinky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float FingerCurl = 55.f;

	/** Curl per index-finger joint (trigger finger is straighter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float IndexCurl = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float ThumbCurl = 20.f;

	/** Where the elbow should point, relative to the wrist (weapon space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FVector ElbowHint = FVector(-30.f, 0.f, -40.f);
};

/** One mesh piece of a weapon. Modular guns (e.g. the RD-704) are several parts modelled in place. */
USTRUCT(BlueprintType)
struct FZSWeaponPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FName Name;

	/** Imported mesh. When it can't be loaded (packs not restored), PlaceholderShape is used instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Transform of the part in weapon space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FTransform Offset;

	/** Engine basic shape (/Engine/BasicShapes/Cube etc.) used if Mesh is missing or unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	TSoftObjectPtr<UStaticMesh> PlaceholderShape;

	/** Placeholder box/cylinder transform (scale = size / 100 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FTransform PlaceholderOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FLinearColor PlaceholderColor = FLinearColor(0.05f, 0.05f, 0.05f);

	/** Part moves with the reload (drops out and back in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	bool bIsMagazine = false;
};

/**
 * Every tunable number for one weapon. Defaults are set per weapon class in C++ (AZSWeapon_* constructors)
 * and become a DataTable row (FZSWeaponRow) when the market arrives in Milestone 8.
 */
USTRUCT(BlueprintType)
struct FZSWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info")
	EZSWeaponClass WeaponClass = EZSWeaponClass::AssaultRifle;

	// ---- Damage ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Damage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float HeadshotMultiplier = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float LimbMultiplier = 0.8f;

	/** Pellets per shot (shotguns > 1). Damage is per pellet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "1"))
	int32 PelletCount = 1;

	/** Full damage up to here (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Range")
	float FalloffStart = 2000.f;

	/** MinDamageMultiplier from here on (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Range")
	float FalloffEnd = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Range")
	float MinDamageMultiplier = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Range")
	float MaxRange = 15000.f;

	/** Total material thickness (cm) a bullet can pass through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Penetration")
	float PenetrationDepth = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Penetration")
	int32 MaxPenetrations = 2;

	/** Damage kept after each surface passed through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Penetration")
	float PenetrationDamageMultiplier = 0.6f;

	// ---- Firing ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Firing", meta = (ForceUnits = "RPM"))
	float FireRateRPM = 600.f;

	/** Available modes; the first is the default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Firing")
	TArray<EZSFireMode> FireModes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Firing")
	int32 BurstCount = 3;

	/** Fastest a semi-auto can be tapped (RPM). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Firing")
	float SemiMaxRPM = 450.f;

	// ---- Ammo ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MagSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MaxReserve = 180;

	/** Reload with rounds left (keeps one in the chamber: mag fills to MagSize + 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	float TacticalReloadTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	float EmptyReloadTime = 2.6f;

	/** Point in the reload (0..1) where the ammo is actually added; cancelling before this keeps the old mag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	float ReloadCommitFraction = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	bool bChamberRound = true;

	// ---- Recoil (degrees of camera kick per shot; X = yaw (right +), Y = pitch (up +)) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	TArray<FVector2D> RecoilPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoilRandomYaw = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoilADSScale = 0.75f;

	/** How fast the kick is applied to the camera (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoilApplySpeed = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoilRecoveryDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ForceUnits = "deg/s"))
	float RecoilRecoveryRate = 12.f;

	/** Fraction of the un-countered kick that recovers automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoilRecoveryFraction = 0.8f;

	// ---- Spread (degrees, cone half-angle) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float HipSpread = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float ADSSpread = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float MoveSpread = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float AirSpread = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float SpreadPerShot = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float MaxBloom = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ForceUnits = "deg/s"))
	float BloomRecovery = 6.f;

	// ---- Handling ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handling")
	float ADSTime = 0.25f;

	/** Camera FOV multiplier at full ADS (0.8 = 20 % zoom). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handling")
	float ADSFOVMultiplier = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handling")
	float ADSMoveSpeedMultiplier = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handling")
	float EquipTime = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handling")
	float UnequipTime = 0.3f;

	/** Delay after sprinting before the weapon can fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handling")
	float SprintToFireTime = 0.15f;

	// ---- View model (first-person placement) ----

	/** Weapon origin relative to the camera when hip firing (X forward, Y right, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	FVector HipOffset = FVector(40.f, 10.f, -24.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	FRotator HipRotation = FRotator::ZeroRotator;

	/** Point on the sight that lines up with the eye when aiming (weapon space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	FVector SightPoint = FVector(-10.f, 0.f, 8.f);

	/** Distance from the eye to the sight point at full ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	float ADSEyeRelief = 14.f;

	/** Barrel end (weapon space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	FVector MuzzlePoint = FVector(30.f, 0.f, 8.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	FZSHandGrip RightHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	FZSHandGrip LeftHand;

	/** Visual kick per shot: cm backwards and degrees of muzzle climb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	float VisualKickBack = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	float VisualKickPitch = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewModel")
	float SwayScale = 1.f;

	// ---- Parts ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts")
	TArray<FZSWeaponPart> Parts;

	float GetShotInterval() const { return 60.f / FMath::Max(FireRateRPM, 1.f); }
};

/** One resolved bullet impact, for hit markers / damage numbers / effects. */
USTRUCT(BlueprintType)
struct FZSHitInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float Damage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bHeadshot = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bKill = false;

	/** World time the hit happened. */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float Time = 0.f;
};
