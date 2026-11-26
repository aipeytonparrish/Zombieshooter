#include "World/ZSPickup.h"
#include "Player/ZSPlayerCharacter.h"
#include "Combat/ZSHealthComponent.h"
#include "UI/ZSHUD.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AZSPickup::AZSPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(75.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Trigger;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Trigger);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(30.f);
	Glow->SetAttenuationRadius(220.f);
	Glow->SetCastShadows(false);
}

void AZSPickup::BeginPlay()
{
	Super::BeginPlay();
	BaseZ = Mesh->GetRelativeLocation().Z + 40.f;

	const bool bHealth = Type == EZSPickupType::Health;
	const FLinearColor Color = bHealth ? FLinearColor(0.1f, 0.9f, 0.25f) : FLinearColor(0.15f, 0.45f, 1.f);
	UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, bHealth ? TEXT("/Engine/BasicShapes/Cube.Cube") : TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Mesh->SetStaticMesh(Shape);
	// Health: a small box; armor: a flat plate.
	Mesh->SetRelativeScale3D(bHealth ? FVector(0.22f) : FVector(0.32f, 0.32f, 0.06f));
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Mesh->SetMaterial(0, MID);
	}
	Glow->SetLightColor(Color);

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);
}

void AZSPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	Mesh->SetRelativeLocation(FVector(0.f, 0.f, BaseZ + FMath::Sin(Age * 2.5f) * 6.f));
	Mesh->AddLocalRotation(FRotator(0.f, 90.f * DeltaSeconds, 0.f));

	if (Lifetime > 0.f)
	{
		const float Left = Lifetime - Age;
		if (Left <= 0.f)
		{
			Destroy();
			return;
		}
		// Blink for the last 6 seconds.
		const bool bVisible = Left > 6.f || FMath::Fmod(Age * (Left < 2.f ? 10.f : 5.f), 1.f) < 0.6f;
		Mesh->SetVisibility(bVisible);
	}

	// Standing on it when it becomes useful (e.g. after taking damage) also collects it.
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, AZSPlayerCharacter::StaticClass());
	for (AActor* A : Overlapping)
	{
		if (TryApply(A))
		{
			return;
		}
	}
}

void AZSPickup::HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	TryApply(Other);
}

bool AZSPickup::TryApply(AActor* Other)
{
	AZSPlayerCharacter* Player = Cast<AZSPlayerCharacter>(Other);
	UZSHealthComponent* Health = Player ? Player->GetHealthComponent() : nullptr;
	if (!Health || Health->IsDead() || IsActorBeingDestroyed())
	{
		return false;
	}

	FText Message;
	if (Type == EZSPickupType::Health)
	{
		if (Health->GetHealth() >= Health->MaxHealth)
		{
			return false;
		}
		const float Healed = Health->Heal(Amount);
		Message = FText::Format(NSLOCTEXT("ZS", "PickupHealth", "+{0} HEALTH"), FText::AsNumber(FMath::RoundToInt(Healed)));
	}
	else
	{
		if (Health->GetArmor() >= Health->MaxArmor)
		{
			return false;
		}
		Health->AddArmor(Amount);
		Message = NSLOCTEXT("ZS", "PickupArmor", "+ARMOR PLATE");
	}

	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/GunSoundsPackVol1/Foley/cue/Insert_Ammo_1-1_Cue.Insert_Ammo_1-1_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
	AZSHUD::Toast(Player, Message);
	Destroy();
	return true;
}
