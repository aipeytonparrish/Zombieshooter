#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZSZombieBase.generated.h"

class UAnimSequence;
class UZSHealthComponent;
class UZSZombieAnimInstance;

UENUM(BlueprintType)
enum class EZSZombieType : uint8
{
	Walker,
	Runner
};

UENUM(BlueprintType)
enum class EZSZombieState : uint8
{
	Spawning,
	Enraging,   // runner's roar before charging
	Chasing,
	Attacking,
	Staggered,
	Dead
};

/** One of the owner's zombie characters: mesh + its own animation set (all on the same skeleton). */
USTRUCT()
struct FZSZombieAppearance
{
	GENERATED_BODY()

	FString MeshPath;
	FString AnimPrefix;               // e.g. /Game/ZombieCity01/Animations/Anim_ZombieCity01_
	TArray<FString> Idles, Walks, Attacks, HitReacts;
	FString Run, Rage;
	float WalkAnimSpeed = 120.f;      // cm/s the walk clip was authored at
	float RunAnimSpeed = 380.f;
	TArray<float> AttackHitFractions; // when in each attack clip the blow lands (0..1)
};

/**
 * Zombie base (Milestone 4: Walker + Runner). Chases the player on the NavMesh (or straight at them when the
 * level has no NavMesh, e.g. the quick-test map), attacks in melee, flinches when shot, and goes full ragdoll on
 * death with an impulse from the killing shot. Headshots come from the physics-asset head body.
 * Specials (Crawler, Exploder, ...) subclass this in Milestone 11.
 */
UCLASS()
class ZOMBIESHOOTER_API AZSZombieBase : public ACharacter
{
	GENERATED_BODY()

public:
	AZSZombieBase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie", meta = (ExposeOnSpawn = true))
	EZSZombieType ZombieType = EZSZombieType::Walker;

	// ---- Tuning (per type; set in ApplyTypeDefaults, override in Blueprint children) ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float WalkerHealth = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float RunnerHealth = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	FVector2D WalkerSpeedRange = FVector2D(95.f, 140.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	FVector2D RunnerSpeedRange = FVector2D(390.f, 470.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float AttackDamage = 20.f;

	/** Start an attack when the player is this close (2D, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float AttackRange = 135.f;

	/** The blow connects if the player is still within AttackRange + this when it lands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float AttackReachBonus = 45.f;

	/** Chance a non-lethal hit makes the zombie flinch (headshots always do). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float FlinchChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float FlinchCooldown = 1.2f;

	/** Ragdoll impulse from the killing shot (scaled by damage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float DeathImpulse = 9000.f;

	/** Seconds a ragdoll stays before being removed (pooling arrives with the round system). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Zombie")
	float CorpseLifetime = 25.f;

	UFUNCTION(BlueprintPure, Category = "ZS|Zombie")
	EZSZombieState GetZombieState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "ZS|Zombie")
	bool IsDead() const { return State == EZSZombieState::Dead; }

	UZSHealthComponent* GetHealth() const { return Health; }

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Number of living zombies (for debug / the round system). */
	static int32 GetLiveCount() { return LiveCount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Zombie")
	TObjectPtr<UZSHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Zombie")
	TObjectPtr<USceneComponent> AimBody;

	UPROPERTY(VisibleAnywhere, Category = "ZS|Zombie")
	TObjectPtr<USceneComponent> AimHead;

private:
	void ApplyAppearance();
	void ApplyTypeDefaults();
	UAnimSequence* LoadAnim(const FString& Name) const;
	UAnimSequence* RandomAnim(const TArray<TObjectPtr<UAnimSequence>>& From) const;

	void UpdateChase(float DeltaSeconds);
	void UpdateAttack(float DeltaSeconds);
	void StartAttack();
	void FaceTarget(float DeltaSeconds, float Rate);
	APawn* GetTarget() const;

	UFUNCTION()
	void HandleDeath(UZSHealthComponent* InHealth, AController* Killer, AActor* DamageCauser);

	void EnterRagdoll();

	UZSZombieAnimInstance* GetZombieAnim() const;

	FZSZombieAppearance Appearance;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> AttackAnims;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> HitAnims;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> RageAnim;

	EZSZombieState State = EZSZombieState::Spawning;
	float StateTime = 0.f;
	float StateDuration = 0.f;
	float RepathTimer = 0.f;
	bool bDirectChase = false;
	int32 AttackIndex = 0;
	float AttackHitFraction = 0.45f;
	bool bAttackLanded = false;
	float FlinchTimer = 0.f;
	float MoveSpeed = 120.f;

	// Last hit (for the ragdoll impulse).
	FVector LastHitLocation = FVector::ZeroVector;
	FVector LastHitDirection = FVector::ForwardVector;
	FName LastHitBone = NAME_None;
	float LastHitDamage = 0.f;
	bool bLastHitHead = false;

	static int32 LiveCount;
};
