#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZSZombieBase.generated.h"

class UAnimSequence;
class UZSHealthComponent;
class UZSZombieAnimInstance;
class AZSZombieBase;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FZSOnZombieKilled, AZSZombieBase* /*Zombie*/, AController* /*Killer*/, bool /*bHeadshot*/);

UENUM(BlueprintType)
enum class EZSZombieType : uint8
{
	Walker,
	Runner,
	// ---- Specials (Milestone 11) ----
	Crawler,    // legless, low to the ground, hard to hit
	Exploder,   // swollen; bursts when shot dead or when it reaches you
	Spitter,    // lobs acid from range
	Brute,      // huge, tanky, charges and knocks you back
	Screamer    // shrieks to enrage nearby zombies
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

	/** Round scaling, set by the round manager before the zombie finishes spawning. */
	UPROPERTY(BlueprintReadWrite, Category = "ZS|Zombie", meta = (ExposeOnSpawn = true))
	float HealthMultiplier = 1.f;

	UPROPERTY(BlueprintReadWrite, Category = "ZS|Zombie", meta = (ExposeOnSpawn = true))
	float SpeedMultiplier = 1.f;

	/** Fires once when this zombie dies. */
	FZSOnZombieKilled OnKilled;

	static FText TypeName(EZSZombieType Type);
	bool IsSpecial() const { return ZombieType >= EZSZombieType::Crawler; }

	/** Legs gone: drop to the ground and crawl (uses the mannequin crawl clips through a compatible skeleton). */
	void BecomeCrawler(bool bHideLegs);
	/** Exploder burst (also used when one dies). */
	void Explode();
	/** Screamer buff: move faster for a while. */
	void ApplySpeedBuff(float Multiplier, float Duration);

	UFUNCTION(BlueprintPure, Category = "ZS|Zombie")
	EZSZombieState GetZombieState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "ZS|Zombie")
	bool IsDead() const { return State == EZSZombieState::Dead; }

	UZSHealthComponent* GetHealth() const { return Health; }
	bool IsDirectChasing() const { return bDirectChase; }

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

	// ---- Specials ----
	void ApplySpecialLook();
	void UpdateSpecial(float DeltaSeconds);
	bool TrySpecialAction(float Dist2D, APawn* Target);
	void FireSpit();
	void Scream();
	bool HasLineOfSight(const APawn* Target) const;

	// ---- Gore ----
	/** Maps a hit bone to the bone that gets cut (or None). */
	static FName SeverBoneFor(FName HitBone);
	void Sever(FName Bone, const FVector& Direction);

	TMap<FName, float> LimbDamage;
	TSet<FName> Severed;
	float SpecialCooldown = 0.f;
	float SpecialTime = -1.f;     // >= 0 while a special action runs
	bool bSpecialFired = false;
	bool bExploded = false;
	bool bCrawling = false;
	float BuffTime = 0.f;
	float BuffMultiplier = 1.f;
	float ChargeTime = 0.f;
	TWeakObjectPtr<AController> LastInstigator;

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
	float StuckTime = 0.f;
	float SidestepTime = 0.f;
	FVector SidestepDir = FVector::ZeroVector;
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
