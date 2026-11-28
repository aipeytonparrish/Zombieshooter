#include "Zombies/ZSSpitProjectile.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Combat/ZSGoreSubsystem.h"
#include "Player/ZSPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/DamageType.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor AcidGreen(0.35f, 1.f, 0.1f);

	void ApplyGlow(UStaticMeshComponent* C, const FLinearColor& Color, float Opacity)
	{
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Glow.M_ZS_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Glow, C);
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			MID->SetScalarParameterValue(TEXT("Fresnel"), 1.2f);
			C->SetMaterial(0, MID);
		}
	}
}

AZSSpitProjectile::AZSSpitProjectile()
{
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCollisionResponseToChannel(ZS_TRACE_WEAPON, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	RootComponent = Collision;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Glob = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glob"));
	Glob->SetupAttachment(Collision);
	Glob->SetStaticMesh(Sphere.Object);
	Glob->SetRelativeScale3D(FVector(0.28f));
	Glob->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Collision);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(40.f);
	Light->SetAttenuationRadius(300.f);
	Light->SetLightColor(AcidGreen);
	Light->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bAutoActivate = false;
	Movement->ProjectileGravityScale = 1.f;
	Movement->bRotationFollowsVelocity = true;
	InitialLifeSpan = 6.f;
}

void AZSSpitProjectile::BeginPlay()
{
	Super::BeginPlay();
	ApplyGlow(Glob, Color, 0.9f);
	Light->SetLightColor(Color);
	Glob->SetRelativeScale3D(FVector(0.28f * Size));
	Collision->SetSphereRadius(12.f * Size);
	Collision->OnComponentHit.AddDynamic(this, &ThisClass::HandleHit);
	if (AActor* Shooter = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
	}
}

FVector AZSSpitProjectile::AimVelocity(const FVector& From, const FVector& Target, float Speed)
{
	FVector Out;
	UGameplayStatics::SuggestProjectileVelocity_CustomArc(nullptr, Out, From, Target, 0.f, 0.42f);
	if (Out.IsNearlyZero())
	{
		Out = (Target - From).GetSafeNormal() * Speed + FVector(0, 0, 300.f);
	}
	return Out;
}

void AZSSpitProjectile::Launch(const FVector& Velocity)
{
	Movement->Velocity = Velocity;
	Movement->Activate(true);
}

void AZSSpitProjectile::HandleHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHit)
	{
		return;
	}
	bHit = true;
	if (Cast<AZSPlayerCharacter>(Other))
	{
		UGameplayStatics::ApplyDamage(Other, ImpactDamage, GetInstigatorController(), this, UDamageType::StaticClass());
		if (UZSGoreSubsystem* Gore = GetWorld()->GetSubsystem<UZSGoreSubsystem>())
		{
			Gore->SplatterNearCamera(Hit.ImpactPoint, 400.f, 0.5f);
		}
	}
	if (SplashRadius > 0.f)
	{
		if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const float D = FVector::Dist(P->GetActorLocation(), Hit.ImpactPoint);
			if (D < SplashRadius && !Cast<AZSPlayerCharacter>(Other))
			{
				UGameplayStatics::ApplyDamage(P, SplashDamage * (1.f - D / SplashRadius * 0.6f), GetInstigatorController(), this, UDamageType::StaticClass());
			}
		}
		if (UZSGoreSubsystem* Gore = GetWorld()->GetSubsystem<UZSGoreSubsystem>())
		{
			Gore->SplatterNearCamera(Hit.ImpactPoint, SplashRadius, 0.4f);
		}
	}
	if (!bLeavePool)
	{
		Destroy();
		return;
	}
	// Pool on the ground below the impact.
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZSAcid), false, this);
	FVector Loc = Hit.ImpactPoint;
	if (GetWorld()->LineTraceSingleByChannel(Ground, Loc + FVector(0, 0, 50.f), Loc - FVector(0, 0, 400.f), ECC_Visibility, Params))
	{
		Loc = Ground.ImpactPoint;
	}
	if (AZSAcidPool* Pool = GetWorld()->SpawnActor<AZSAcidPool>(AZSAcidPool::StaticClass(), FTransform(Loc)))
	{
		Pool->Setup(PoolRadius, PoolDamagePerSecond, PoolLifetime);
	}
	Destroy();
}

// ---- Acid pool ------------------------------------------------------------------------------

AZSAcidPool::AZSAcidPool()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Pool = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pool"));
	Pool->SetupAttachment(RootComponent);
	Pool->SetStaticMesh(Cyl.Object);
	Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pool->SetCastShadow(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0, 0, 40.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(25.f);
	Light->SetAttenuationRadius(350.f);
	Light->SetLightColor(AcidGreen);
	Light->SetCastShadows(false);
}

void AZSAcidPool::Setup(float InRadius, float InDPS, float InLife)
{
	Radius = InRadius;
	DPS = InDPS;
	Life = InLife;
	Pool->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.02f));
	Pool->SetRelativeLocation(FVector(0, 0, 2.f));
	ApplyGlow(Pool, AcidGreen, 0.55f);
}

void AZSAcidPool::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= Life)
	{
		Destroy();
		return;
	}
	const float Fade = FMath::Clamp((Life - Age) / 1.f, 0.f, 1.f);
	Pool->SetRelativeScale3D(FVector(Radius / 50.f * (0.6f + 0.4f * Fade), Radius / 50.f * (0.6f + 0.4f * Fade), 0.02f));
	DamageTick -= DeltaSeconds;
	if (DamageTick <= 0.f)
	{
		DamageTick = 0.5f;
		if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const FVector D = P->GetActorLocation() - GetActorLocation();
			if (D.Size2D() <= Radius && FMath::Abs(D.Z) < 150.f)
			{
				UGameplayStatics::ApplyDamage(P, DPS * 0.5f, nullptr, this, UDamageType::StaticClass());
			}
		}
	}
}
