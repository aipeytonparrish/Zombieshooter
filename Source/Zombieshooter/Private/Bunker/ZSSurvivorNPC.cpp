#include "Bunker/ZSSurvivorNPC.h"
#include "Core/ZSCharacterCatalog.h"
#include "UI/ZSHUD.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* DriverMeshPath = TEXT("/Game/MC_Idles_Pack/Demo/Characters/MCUE5v2/Meshes/SKM_MCUE5v2.SKM_MCUE5v2");
}

AZSSurvivorNPC::AZSSurvivorNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 90.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = Capsule;

	Driver = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Driver"));
	Driver->SetupAttachment(Capsule);
	Driver->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	Driver->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Driver->SetVisibility(false);
	Driver->bHiddenInGame = true;
	Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;   // hidden, but it drives the body

	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(Capsule);
	BodyMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->bCastDynamicShadow = true;

	NameTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(Capsule);
	NameTag->SetRelativeLocation(FVector(0.f, 0.f, 112.f));
	NameTag->SetHorizontalAlignment(EHTA_Center);
	NameTag->SetVerticalAlignment(EVRTA_TextBottom);
	NameTag->SetWorldSize(5.5f);
	NameTag->SetTextRenderColor(FColor(235, 215, 170));
	NameTag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NameTag->SetVisibility(false);
}

void AZSSurvivorNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AZSSurvivorNPC::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
}

void AZSSurvivorNPC::ApplyLook()
{
	const int32 Index = FMath::Clamp(Body, 1, 5) - 1;
	if (USkeletalMesh* Mesh = ZSCharacterCatalog::LoadBody(Index))
	{
		BodyMesh->SetSkeletalMeshAsset(Mesh);
	}

	UAnimSequence* Idle = (!bOwnIdle && !IdleAnim.IsEmpty()) ? LoadObject<UAnimSequence>(nullptr, *IdleAnim, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	USkeletalMesh* DriverMesh = Idle ? LoadObject<USkeletalMesh>(nullptr, DriverMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (Idle && DriverMesh)
	{
		Driver->SetSkeletalMeshAsset(DriverMesh);
		Driver->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Driver->PlayAnimation(Idle, true);
		Driver->SetPlayRate(PlayRate);
		Driver->SetPosition(FMath::FRandRange(0.f, Idle->GetPlayLength()));
		BodyMesh->SetLeaderPoseComponent(Driver);
	}
	else if (UAnimSequence* Own = ZSCharacterCatalog::LoadIdle(Index))
	{
		BodyMesh->SetLeaderPoseComponent(nullptr);
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		BodyMesh->PlayAnimation(Own, true);
		BodyMesh->SetPlayRate(PlayRate);
	}

	FString Tag = SurvivorName.ToString().ToUpper();
	if (!Job.IsEmpty())
	{
		Tag += TEXT("\n") + Job.ToString().ToUpper();
	}
	NameTag->SetText(FText::FromString(Tag));
}

void AZSSurvivorNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Player || !Cam)
	{
		return;
	}
	const float Dist = FVector::Dist2D(Player->GetActorLocation(), GetActorLocation());

	// Name tag: only up close, always facing the camera.
	const bool bShowTag = Dist < 600.f;
	NameTag->SetVisibility(bShowTag);
	if (bShowTag)
	{
		const FVector To = Cam->GetCameraLocation() - NameTag->GetComponentLocation();
		NameTag->SetWorldRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	}

	// Greeting when the player walks up.
	const bool bNear = Dist < GreetRadius;
	const double Now = GetWorld()->GetTimeSeconds();
	if (bNear && !bPlayerWasNear && Lines.Num() > 0 && Now - LastGreetTime > GreetCooldown)
	{
		LastGreetTime = Now;
		const FText& Line = Lines[NextLine % Lines.Num()];
		++NextLine;
		AZSHUD::Toast(Player, FText::Format(NSLOCTEXT("ZS", "NPCLine", "{0}:  \"{1}\""), SurvivorName, Line));
	}
	bPlayerWasNear = bNear;
}
