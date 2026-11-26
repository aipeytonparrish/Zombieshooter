#include "Combat/ZSHealthComponent.h"
#include "GameFramework/Actor.h"

UZSHealthComponent::UZSHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UZSHealthComponent* UZSHealthComponent::FindOn(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UZSHealthComponent>() : nullptr;
}

void UZSHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	Armor = StartingArmor < 0.f ? MaxArmor : FMath::Min(StartingArmor, MaxArmor);
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &ThisClass::HandleAnyDamage);
	}
}

void UZSHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bDead || Damage <= 0.f)
	{
		return;
	}

	float ToHealth = Damage;
	if (Armor > 0.f)
	{
		const float Absorbed = FMath::Min(Armor, Damage * ArmorAbsorption);
		Armor -= Absorbed;
		ToHealth -= Absorbed;
	}

	Health = FMath::Max(0.f, Health - ToHealth);
	TimeSinceDamage = 0.f;
	OnDamaged.Broadcast(this, Damage, DamageType, InstigatedBy, DamageCauser);

	if (Health <= 0.f)
	{
		bDead = true;
		TimeDead = 0.f;
		OnDeath.Broadcast(this, InstigatedBy, DamageCauser);
	}
}

void UZSHealthComponent::Revive()
{
	bDead = false;
	Health = MaxHealth;
	Armor = StartingArmor < 0.f ? MaxArmor : FMath::Min(StartingArmor, MaxArmor);
	TimeSinceDamage = 0.f;
}

void UZSHealthComponent::AddArmor(float Amount)
{
	Armor = FMath::Clamp(Armor + Amount, 0.f, MaxArmor);
}

float UZSHealthComponent::Heal(float Amount)
{
	if (bDead)
	{
		return 0.f;
	}
	const float Before = Health;
	Health = FMath::Min(MaxHealth, Health + FMath::Max(0.f, Amount));
	return Health - Before;
}

void UZSHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDead)
	{
		TimeDead += DeltaTime;
		if (AutoReviveDelay > 0.f && TimeDead >= AutoReviveDelay)
		{
			Revive();
		}
		return;
	}

	TimeSinceDamage += DeltaTime;
	if (RegenDelay > 0.f && RegenRate > 0.f && TimeSinceDamage >= RegenDelay && Health < MaxHealth)
	{
		Health = FMath::Min(MaxHealth, Health + RegenRate * DeltaTime);
	}
}
