#include "World/ZSBuyDoor.h"
#include "Player/ZSPlayerCharacter.h"
#include "Rounds/ZSGameMode_Survival.h"
#include "Rounds/ZSGameState_Survival.h"
#include "Rounds/ZSRoundManagerComponent.h"
#include "UI/ZSHUD.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AZSBuyDoor::AZSBuyDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	Label = NSLOCTEXT("ZS", "DoorDefaultLabel", "DOOR");

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(Root);
	Blocker->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Blocker->SetCollisionResponseToChannel(ZS_TRACE_INTERACT, ECR_Block);
	Blocker->SetCanEverAffectNavigation(false);   // zombies path through as soon as it is open

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(12.f);
	Light->SetAttenuationRadius(300.f);
	Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	Light->SetCastShadows(false);
}

void AZSBuyDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AZSBuyDoor::BeginPlay()
{
	Super::BeginPlay();
	if (Boards.Num() == 0)
	{
		Build();
	}
	if (!OpenSound)
	{
		OpenSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/GunSoundsPackVol1/Bonus/cue/Explosion_Medium_1-1_Cue.Explosion_Medium_1-1_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

void AZSBuyDoor::Build()
{
	for (UStaticMeshComponent* B : Boards)
	{
		if (B)
		{
			B->DestroyComponent();
		}
	}
	Boards.Reset();

	const float W = OpeningSize.X;
	const float H = OpeningSize.Y;
	Blocker->SetBoxExtent(FVector(15.f, W * 0.5f, H * 0.5f));
	Blocker->SetRelativeLocation(FVector(0.f, 0.f, H * 0.5f));
	Light->SetRelativeLocation(FVector(-60.f, 0.f, H * 0.8f));

	// Rough planks nailed across the opening at slightly random angles.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Cube)
	{
		return;
	}
	FRandomStream Rand(GetTypeHash(GetFName()));
	const int32 Count = FMath::Max(3, FMath::RoundToInt(H / 38.f));
	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		B->SetupAttachment(Root);
		B->SetStaticMesh(Cube);
		B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		B->SetCanEverAffectNavigation(false);
		const float Z = H * (0.1f + 0.85f * (i + 0.5f) / Count);
		const float Roll = Rand.FRandRange(-14.f, 14.f);
		B->SetRelativeLocationAndRotation(FVector(Rand.FRandRange(-6.f, 6.f), 0.f, Z), FRotator(0.f, 0.f, Roll));
		B->SetRelativeScale3D(FVector(0.05f, (W + 40.f) / 100.f, Rand.FRandRange(0.16f, 0.24f)));
		if (BoardMaterial)
		{
			B->SetMaterial(0, BoardMaterial);
		}
		else if (Mat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, B);
			const float Shade = Rand.FRandRange(0.7f, 1.1f);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.16f * Shade, 0.1f * Shade, 0.055f * Shade));
			B->SetMaterial(0, MID);
		}
		B->RegisterComponent();
		Boards.Add(B);
	}
}

int32 AZSBuyDoor::GetRunCash() const
{
	const AZSGameState_Survival* GS = GetWorld() ? GetWorld()->GetGameState<AZSGameState_Survival>() : nullptr;
	return GS ? GS->RunCash : 0;
}

FText AZSBuyDoor::GetInteractPrompt() const
{
	const int32 Cash = GetRunCash();
	if (Cash < Cost)
	{
		return FText::Format(NSLOCTEXT("ZS", "DoorNeedCash", "OPEN {0}  ${1}  (NEED ${2} MORE)"), Label, FText::AsNumber(Cost), FText::AsNumber(Cost - Cash));
	}
	return FText::Format(NSLOCTEXT("ZS", "DoorBuy", "OPEN {0}  ${1}"), Label, FText::AsNumber(Cost));
}

void AZSBuyDoor::Interact(AZSPlayerCharacter* Player)
{
	if (bOpen || !Player)
	{
		return;
	}
	AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>();
	if (!GS || GS->RunCash < Cost)
	{
		AZSHUD::Toast(Player, NSLOCTEXT("ZS", "DoorTooPoor", "NOT ENOUGH CASH - KILL MORE ZOMBIES"));
		return;
	}
	GS->RunCash -= Cost;
	AZSHUD::Toast(Player, FText::Format(NSLOCTEXT("ZS", "DoorOpened", "{0} OPENED  -${1}"), Label, FText::AsNumber(Cost)));
	Open();
}

void AZSBuyDoor::Open()
{
	bOpen = true;
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Light->SetVisibility(false);
	if (OpenSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetActorLocation(), 0.45f, 1.35f);
	}
	if (AZSGameMode_Survival* GM = GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
	{
		if (GM->GetRoundManager())
		{
			GM->GetRoundManager()->OpenZone(Zone);
		}
	}

	// Kick the boards out through the doorway; they only collide with the world, then go away.
	const FVector Out = GetActorForwardVector();
	for (UStaticMeshComponent* B : Boards)
	{
		if (!B)
		{
			continue;
		}
		B->SetCollisionProfileName(TEXT("PhysicsActor"));
		B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		B->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		B->SetSimulatePhysics(true);
		B->AddImpulse((Out * FMath::FRandRange(250.f, 450.f) + FVector(0.f, FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(50.f, 150.f))), NAME_None, true);
		B->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 0.f), NAME_None, true);
	}
	FTimerHandle Cleanup;
	GetWorldTimerManager().SetTimer(Cleanup, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		for (UStaticMeshComponent* B : Boards)
		{
			if (B) { B->DestroyComponent(); }
		}
		Boards.Reset();
	}), 8.f, false);
}
