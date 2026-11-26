#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZSHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FZSOnDamaged, UZSHealthComponent*, Health, float, Damage, const UDamageType*, DamageType, AController*, InstigatedBy, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FZSOnDeath, UZSHealthComponent*, Health, AController*, Killer, AActor*, DamageCauser);

/**
 * Health + armor + regeneration, shared by the player, zombies and test targets.
 * Listens to the owner's OnTakeAnyDamage, so anything that calls UGameplayStatics::Apply*Damage works.
 * Armor absorbs ArmorAbsorption of incoming damage while it lasts (player armor arrives in Milestone 5).
 */
UCLASS(ClassGroup = (ZS), meta = (BlueprintSpawnableComponent))
class ZOMBIESHOOTER_API UZSHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZSHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Health", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Health", meta = (ClampMin = "0"))
	float MaxArmor = 0.f;

	/** Fraction of damage armor soaks up while it has points left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Health", meta = (ClampMin = "0", ClampMax = "1"))
	float ArmorAbsorption = 0.6f;

	/** Seconds without damage before health starts regenerating. <= 0 disables regen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Health")
	float RegenDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Health", meta = (ForceUnits = "1/s"))
	float RegenRate = 0.f;

	/** Restore full health automatically this long after dying (test targets only). <= 0 = stay dead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZS|Health")
	float AutoReviveDelay = 0.f;

	UPROPERTY(BlueprintAssignable, Category = "ZS|Health")
	FZSOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "ZS|Health")
	FZSOnDeath OnDeath;

	UFUNCTION(BlueprintPure, Category = "ZS|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "ZS|Health")
	float GetArmor() const { return Armor; }

	UFUNCTION(BlueprintPure, Category = "ZS|Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintCallable, Category = "ZS|Health")
	void Revive();

	UFUNCTION(BlueprintCallable, Category = "ZS|Health")
	void AddArmor(float Amount);

	/** Find the health component on an actor (nullptr if it has none). */
	static UZSHealthComponent* FindOn(const AActor* Actor);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	float Health = 0.f;
	float Armor = 0.f;
	float TimeSinceDamage = 0.f;
	float TimeDead = 0.f;
	bool bDead = false;
};
