#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Weapons/ZSFPAnimInstance.h"
#include "ZSWeaponBase.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USoundBase;
class USkeletalMeshComponent;
class UZSFPAnimInstance;
class UAnimSequence;

/** The animation set after loading (raw pointers; kept alive by AZSWeaponBase::KeepAlive). */
struct FZSLoadedAnimSet
{
	UAnimSequence* Fire = nullptr;
	UAnimSequence* FireAimed = nullptr;
	UAnimSequence* Reload = nullptr;
	UAnimSequence* ReloadEmpty = nullptr;
	UAnimSequence* Equip = nullptr;
	UAnimSequence* Holster = nullptr;
	UAnimSequence* Melee = nullptr;
	UAnimSequence* WeaponFire = nullptr;
	UAnimSequence* WeaponReload = nullptr;
	UAnimSequence* WeaponReloadEmpty = nullptr;
	const class UAnimMontage* ReloadSoundMontage = nullptr;
	const class UAnimMontage* ReloadEmptySoundMontage = nullptr;
	TArray<USoundBase*> FireSounds;
	USoundBase* FireTail = nullptr;
	USoundBase* DryFire = nullptr;
	USoundBase* EquipSound = nullptr;
	class UNiagaraSystem* MuzzleFlash = nullptr;
	TArray<FZSActionSound> ReloadSounds;
	TArray<FZSActionSound> ReloadEmptySounds;
};

/**
 * A weapon: its stats, ammo and first-person visuals. Firing, recoil, ADS and aim assist live in
 * UZSWeaponComponent on the player, which owns and drives these actors.
 *
 * Visuals are built at BeginPlay from Stats.Parts. If a part's imported mesh is missing (third-party packs not
 * restored on this PC) a coloured engine-shape placeholder is used, so the game always runs.
 */
UCLASS(Abstract)
class ZOMBIESHOOTER_API AZSWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AZSWeaponBase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Weapon")
	FZSWeaponStats Stats;

	/** Attachment ids fitted to this gun (set before BeginPlay, e.g. via SpawnActorDeferred). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZS|Weapon")
	TArray<FName> Attachments;

	/** Real first-person animation (used when its assets are present; see FZSWeaponAnimSet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Weapon")
	FZSWeaponAnimSet AnimSet;

	/** True when the animated rig loaded; false = code-posed arms + Stats.Parts. */
	bool IsAnimated() const { return bAnimated; }
	USkeletalMeshComponent* GetDriverMesh() const { return DriverMesh; }
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	UZSFPAnimInstance* GetArmsAnim() const;
	UZSFPAnimInstance* GetWeaponAnim() const;
	const FZSLoadedAnimSet& GetLoadedAnims() const { return Loaded; }

	/** Animated mode: keep the camera socket on the camera (call after the pose updates). */
	void UpdateDriverPlacement();

	/** Fire sound (random variation) + weapon fire clip + muzzle flash. */
	void PlayShotEffects(bool bAimed);
	void PlayFireTail();
	void PlayDryFire();

	// ---- Optional sounds (owner-provided assets later; silent when unset) ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Weapon|Audio")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Weapon|Audio")
	TObjectPtr<USoundBase> DryFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Weapon|Audio")
	TObjectPtr<USoundBase> ReloadSound;

	// ---- Ammo ----
	UFUNCTION(BlueprintPure, Category = "ZS|Weapon")
	int32 GetAmmoInMag() const { return AmmoInMag; }

	UFUNCTION(BlueprintPure, Category = "ZS|Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintCallable, Category = "ZS|Weapon")
	void RefillAmmo();

	bool ConsumeRound();
	bool CanReload() const;

	/** Moves rounds from reserve into the magazine. bWasEmpty decides whether a chambered round is kept. */
	void CommitReload(bool bWasEmpty);

	// ---- Fire mode ----
	EZSFireMode GetFireMode() const;
	void CycleFireMode();

	// ---- Visuals ----
	FVector GetMuzzleWorldLocation() const;
	FTransform GetWeaponSpaceToWorld() const;
	void PlayMuzzleFlash();

	/** Offsets the magazine part(s) (weapon space) - used by the procedural reload. */
	void SetMagazineOffset(const FVector& Offset, const FRotator& Rotation);

	/** Shows or hides all first-person visuals. */
	void SetWeaponVisible(bool bVisible);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Weapon")
	TObjectPtr<USceneComponent> WeaponRoot;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Weapon")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Weapon")
	TObjectPtr<USkeletalMeshComponent> DriverMesh;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MagazineComponents;

	/** Helpers for subclass constructors. */
	static FZSWeaponPart MakeImportedPart(FName Name, const TCHAR* MeshPath, bool bMagazine = false);
	static FZSWeaponPart MakeBoxPart(FName Name, const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bMagazine = false);
	static FZSWeaponPart MakeCylinderPart(FName Name, const FVector& Center, float Length, float Diameter, const FLinearColor& Color);

private:
	void BuildVisuals();
	bool TryBuildAnimated();

	FZSLoadedAnimSet Loaded;
	bool bAnimated = false;

	TArray<FTransform> MagazineBaseTransforms;

	int32 AmmoInMag = 0;
	int32 ReserveAmmo = 0;
	int32 FireModeIndex = 0;
	float MuzzleFlashTime = 0.f;
};
