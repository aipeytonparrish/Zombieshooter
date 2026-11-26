#include "World/ZSTargetDummy.h"
#include "Combat/ZSHealthComponent.h"
#include "Combat/ZSAimAssistSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor BodyColor(0.35f, 0.32f, 0.28f);
	const FLinearColor HeadColor(0.55f, 0.45f, 0.35f);
	const FLinearColor FlashColor(1.f, 0.2f, 0.1f);
}

AZSTargetDummy::AZSTargetDummy()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// Body: 60 cm wide, 150 cm tall, standing on the ground.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Pivot);
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
	}
	Body->SetRelativeLocation(FVector(0.f, 0.f, 75.f));
	Body->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.5f));
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	Body->ComponentTags.Add(TEXT("Body"));

	// Head: 26 cm sphere on top.
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Pivot);
	if (Sphere.Succeeded())
	{
		Head->SetStaticMesh(Sphere.Object);
	}
	Head->SetRelativeLocation(FVector(0.f, 0.f, 165.f));
	Head->SetRelativeScale3D(FVector(0.26f));
	Head->SetCollisionProfileName(TEXT("BlockAll"));
	Head->ComponentTags.Add(TEXT("Head"));

	Readout = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Readout"));
	Readout->SetupAttachment(Root);
	Readout->SetRelativeLocation(FVector(0.f, 0.f, 210.f));
	Readout->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	Readout->SetHorizontalAlignment(EHTA_Center);
	Readout->SetWorldSize(22.f);
	Readout->SetTextRenderColor(FColor::White);
	Readout->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Health = CreateDefaultSubobject<UZSHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 150.f;
	Health->AutoReviveDelay = 3.f;
	Health->RegenDelay = 2.f;
	Health->RegenRate = 200.f;
}

void AZSTargetDummy::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetActorLocation();
	MovePhase = FMath::FRand() * 6.f;

	BodyMID = Body->CreateDynamicMaterialInstance(0);
	HeadMID = Head->CreateDynamicMaterialInstance(0);
	if (BodyMID) { BodyMID->SetVectorParameterValue(TEXT("Color"), BodyColor); }
	if (HeadMID) { HeadMID->SetVectorParameterValue(TEXT("Color"), HeadColor); }

	Health->OnDamaged.AddDynamic(this, &ThisClass::HandleDamaged);

	if (UZSAimAssistSubsystem* Registry = GetWorld()->GetSubsystem<UZSAimAssistSubsystem>())
	{
		Registry->RegisterTarget(this, Body, Head, 35.f);
	}
	UpdateReadout();
}

void AZSTargetDummy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UZSAimAssistSubsystem* Registry = World->GetSubsystem<UZSAimAssistSubsystem>())
		{
			Registry->UnregisterTarget(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AZSTargetDummy::HandleDamaged(UZSHealthComponent* InHealth, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	FlashTime = 0.08f;
	LastDamage = Damage;
}

void AZSTargetDummy::UpdateReadout()
{
	const FString Text = Health->IsDead()
		? FString(TEXT("DOWN"))
		: FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Health->GetHealth()), FMath::RoundToInt(Health->MaxHealth));
	Readout->SetText(FText::FromString(Text));
}

void AZSTargetDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMoving && !Health->IsDead())
	{
		MovePhase += DeltaSeconds * MoveSpeed / FMath::Max(MoveDistance * 0.5f, 1.f);
		const FVector Side = GetActorRightVector() * FMath::Sin(MovePhase) * MoveDistance * 0.5f;
		SetActorLocation(StartLocation + Side);
	}

	// Fall over when killed, stand back up on revive.
	FallAlpha = FMath::FInterpTo(FallAlpha, Health->IsDead() ? 1.f : 0.f, DeltaSeconds, Health->IsDead() ? 9.f : 5.f);
	Pivot->SetRelativeRotation(FRotator(-85.f * FallAlpha, 0.f, 0.f));

	if (FlashTime > 0.f)
	{
		FlashTime -= DeltaSeconds;
	}
	const float Flash = FlashTime > 0.f ? 1.f : 0.f;
	if (BodyMID) { BodyMID->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(BodyColor, FlashColor, Flash)); }
	if (HeadMID) { HeadMID->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(HeadColor, FlashColor, Flash)); }

	UpdateReadout();
}
