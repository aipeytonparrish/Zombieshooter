#include "World/ZSBarricade.h"
#include "Player/ZSPlayerCharacter.h"
#include "Rounds/ZSGameState_Survival.h"
#include "UI/ZSHUD.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Weapons/ZSWeaponTypes.h"
#include "Zombieshooter.h"

AZSBarricade::AZSBarricade()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Fills the opening: players can never climb through; bullets and zombies' (scripted) vaults can.
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(Root);
	Blocker->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Blocker->SetCollisionResponseToChannel(ZS_TRACE_WEAPON, ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ZS_TRACE_INTERACT, ECR_Block);
	Blocker->SetCanEverAffectNavigation(false);
}

FVector AZSBarricade::GetOutsidePoint() const
{
	return GetActorLocation() + GetActorForwardVector() * StandOff + FVector(0.f, 0.f, 95.f);
}

FVector AZSBarricade::GetSillPoint() const
{
	return GetActorLocation() + FVector(0.f, 0.f, SillHeight + 65.f);   // body almost flat (see the zombie's dive): centre ~0.65 m over the sill
}

FVector AZSBarricade::GetInsidePoint() const
{
	return GetActorLocation() - GetActorForwardVector() * StandOff + FVector(0.f, 0.f, 95.f);
}

void AZSBarricade::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Boards = MaxBoards;
	Build();
}

void AZSBarricade::BeginPlay()
{
	Super::BeginPlay();
	Boards = MaxBoards;
	Build();
	BreakSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/RealWeaponSoundFX/Bullet_Sound_FX/Combained_Cues/Cue_Bullet_on_wood.Cue_Bullet_on_wood"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	RepairSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/InfimaGames/TacticalFPSAnimations/Common/Audio/Melee/A_TFA_Melee_Impact_Cue.A_TFA_Melee_Impact_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void AZSBarricade::Build()
{
	for (UStaticMeshComponent* B : BoardMeshes)
	{
		if (B) { B->DestroyComponent(); }
	}
	BoardMeshes.Reset();

	const float W = OpeningSize.X;
	const float H = OpeningSize.Y;
	Blocker->SetBoxExtent(FVector(12.f, W * 0.5f, H * 0.5f));
	Blocker->SetRelativeLocation(FVector(0.f, 0.f, SillHeight + H * 0.5f));

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Paint.M_ZS_Paint"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Cube)
	{
		return;
	}
	// Planks nailed across the opening, alternating tilt, on the outside face.
	FRandomStream Rand(GetTypeHash(GetActorLocation().ToString()));
	for (int32 i = 0; i < MaxBoards; ++i)
	{
		UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		B->SetupAttachment(RootComponent);
		B->SetStaticMesh(Cube);
		B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		B->SetCanEverAffectNavigation(false);
		B->SetCastShadow(true);
		const float Z = SillHeight + H * (0.08f + 0.84f * (i + 0.5f) / MaxBoards);
		const float Roll = (i % 2 ? 1.f : -1.f) * Rand.FRandRange(6.f, 22.f);
		B->SetRelativeLocationAndRotation(FVector(10.f + Rand.FRandRange(-3.f, 3.f), Rand.FRandRange(-8.f, 8.f), Z), FRotator(0.f, 0.f, Roll));
		B->SetRelativeScale3D(FVector(0.04f, (W + 30.f) / 100.f, Rand.FRandRange(0.14f, 0.2f)));
		if (BoardMaterial)
		{
			B->SetMaterial(0, BoardMaterial);
		}
		else if (Paint)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Paint, B);
			const float Shade = Rand.FRandRange(0.65f, 1.1f);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.17f * Shade, 0.105f * Shade, 0.055f * Shade));
			MID->SetScalarParameterValue(TEXT("Roughness"), 0.85f);
			B->SetMaterial(0, MID);
		}
		B->RegisterComponent();
		BoardMeshes.Add(B);
	}
	RefreshBoards();
}

void AZSBarricade::RefreshBoards()
{
	for (int32 i = 0; i < BoardMeshes.Num(); ++i)
	{
		if (BoardMeshes[i])
		{
			BoardMeshes[i]->SetVisibility(i < Boards);
		}
	}
}

void AZSBarricade::TearBoard(AActor* Zombie)
{
	if (Boards <= 0)
	{
		return;
	}
	--Boards;
	UE_LOG(LogZS, Log, TEXT("Barricade %s: board torn off by %s, %d left"), *GetName(), *GetNameSafe(Zombie), Boards);
	// Fling a copy of the board inward; the slot itself just hides.
	if (UStaticMeshComponent* Slot = BoardMeshes.IsValidIndex(Boards) ? BoardMeshes[Boards].Get() : nullptr)
	{
		UStaticMeshComponent* Flung = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Flung->SetStaticMesh(Slot->GetStaticMesh());
		Flung->SetMaterial(0, Slot->GetMaterial(0));
		Flung->SetWorldTransform(Slot->GetComponentTransform());
		Flung->SetCollisionProfileName(TEXT("PhysicsActor"));
		Flung->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Flung->SetCollisionResponseToChannel(ZS_TRACE_WEAPON, ECR_Ignore);
		Flung->SetCanEverAffectNavigation(false);
		Flung->RegisterComponent();
		Flung->SetSimulatePhysics(true);
		Flung->SetMassOverrideInKg(NAME_None, 4.f, true);
		Flung->SetPhysicsLinearVelocity(-GetActorForwardVector() * FMath::FRandRange(250.f, 450.f) + FVector(0.f, 0.f, 150.f) + FMath::VRand() * 80.f);
		Flung->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 400.f);
		TWeakObjectPtr<UStaticMeshComponent> Weak = Flung;
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Weak]() { if (Weak.IsValid()) { Weak->DestroyComponent(); } }), 6.f, false);
	}
	RefreshBoards();
	if (BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, GetActorLocation() + FVector(0.f, 0.f, 120.f), 1.f, FMath::FRandRange(0.6f, 0.8f));
	}
}

FText AZSBarricade::GetInteractPrompt() const
{
	return FText::Format(NSLOCTEXT("ZS", "BarricadeRepair", "REPAIR BARRICADE  (+${0})"), FText::AsNumber(RepairReward));
}

bool AZSBarricade::CanInteract(const AZSPlayerCharacter* Player) const
{
	return Boards < MaxBoards;
}

void AZSBarricade::Interact(AZSPlayerCharacter* Player)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Boards >= MaxBoards || Now - LastRepairTime < 0.45)
	{
		return;
	}
	LastRepairTime = Now;
	++Boards;
	RefreshBoards();
	if (AZSGameState_Survival* GS = GetWorld()->GetGameState<AZSGameState_Survival>())
	{
		GS->RunCash += RepairReward;
	}
	if (RepairSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RepairSound, GetActorLocation() + FVector(0.f, 0.f, 120.f), 0.8f, FMath::FRandRange(0.9f, 1.1f));
	}
}
