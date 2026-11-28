#include "Bunker/ZSStation.h"
#include "Bunker/ZSBunkerMenus.h"
#include "Player/ZSPlayerCharacter.h"
#include "Player/ZSPlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Core/ZSCharacterCatalog.h"
#include "Profile/ZSProfileSubsystem.h"

namespace
{
	UStaticMesh* Mesh(const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }
	UMaterialInterface* Mat(const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }

	/** A worn-metal material from the owner's industrial kit if it's there, else our paint in a flat colour. */
	UMaterialInterface* KitOrPaint(UObject* Outer, const TCHAR* KitPath, const FLinearColor& Fallback, float Emissive = 0.f)
	{
		if (Emissive <= 0.f)
		{
			if (UMaterialInterface* Kit = Mat(KitPath))
			{
				return Kit;
			}
		}
		UMaterialInterface* Paint = Mat(TEXT("/Game/ZS/Materials/M_ZS_Paint.M_ZS_Paint"));
		if (!Paint)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Paint, Outer);
		MID->SetVectorParameterValue(TEXT("Color"), Fallback);
		MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		return MID;
	}
	const TCHAR* Rust = TEXT("/Game/IndustrialKit/SM_Ceiling_Beams_2x2/Materials/M_Rust_Heavy.M_Rust_Heavy");
	const TCHAR* Steel = TEXT("/Game/IndustrialKit/SM_Floor_TreadPlate_2x2/Materials/M_Steel_Worn.M_Steel_Worn");
	const TCHAR* Tread = TEXT("/Game/IndustrialKit/SM_Floor_TreadPlate_2x2/Materials/M_Tread_Plate.M_Tread_Plate");
	const TCHAR* Corrugated = TEXT("/Game/IndustrialKit/SM_Wall_Corrugated_2x3/Materials/M_Corrugated_Metal.M_Corrugated_Metal");
}

AZSStation::AZSStation()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetBoxExtent(FVector(90.f, 55.f, 50.f));
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ZS_TRACE_INTERACT, ECR_Block);
	RootComponent = Collision;

	auto MakeMesh = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* M = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		M->SetupAttachment(Collision);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return M;
	};
	Table = MakeMesh(TEXT("Table"));
	PropA = MakeMesh(TEXT("PropA"));
	PropB = MakeMesh(TEXT("PropB"));

	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Collision);
	Sign->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Sign->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetWorldSize(26.f);
	Sign->SetTextRenderColor(FColor(255, 205, 90));
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Lamp = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Collision);
	Lamp->SetRelativeLocation(FVector(-60.f, 0.f, 170.f));
	Lamp->SetIntensityUnits(ELightUnits::Candelas);
	Lamp->SetIntensity(60.f);
	Lamp->SetAttenuationRadius(500.f);
	Lamp->SetLightColor(FLinearColor(1.f, 0.82f, 0.6f));

	// Locker only: a survivor standing beside the bench, and a camera framing them on the right of the screen.
	Preview = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Preview"));
	Preview->SetupAttachment(Collision);
	Preview->SetRelativeLocationAndRotation(FVector(0.f, -150.f, -50.f), FRotator(0.f, 90.f, 0.f));
	Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Preview->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(Collision);
	PreviewCamera->SetRelativeLocationAndRotation(FVector(-250.f, -215.f, 75.f), FRotator(-6.f, 0.f, 0.f));
	PreviewCamera->SetFieldOfView(70.f);
	PreviewCamera->SetAutoActivate(false);
}

FText AZSStation::StationName(EZSStationType InType)
{
	switch (InType)
	{
	case EZSStationType::MapTable:  return NSLOCTEXT("ZS", "St_Map", "MAP TABLE");
	case EZSStationType::Armory:    return NSLOCTEXT("ZS", "St_Armory", "ARMORY");
	case EZSStationType::Market:    return NSLOCTEXT("ZS", "St_Market", "MARKET");
	case EZSStationType::Character: return NSLOCTEXT("ZS", "St_Character", "LOCKER");
	case EZSStationType::Missions:  return NSLOCTEXT("ZS", "St_Missions", "MISSION BOARD");
	}
	return FText::GetEmpty();
}

FText AZSStation::GetInteractPrompt() const
{
	return StationName(Type);
}

void AZSStation::Interact(AZSPlayerCharacter* Player)
{
	if (AZSPlayerController* PC = Player ? Cast<AZSPlayerController>(Player->GetController()) : nullptr)
	{
		ZSBunkerMenus::Open(PC, Type);
	}
}

void AZSStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyVisuals();
}

void AZSStation::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisuals();   // construction doesn't re-run when a saved level loads in game
	if (Type == EZSStationType::Character)
	{
		RefreshPreview();
		if (UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this))
		{
			LookChangedHandle = Profile->OnLookChanged.AddUObject(this, &AZSStation::RefreshPreview);
		}
	}
}

void AZSStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this))
	{
		Profile->OnLookChanged.Remove(LookChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AZSStation::RefreshPreview()
{
	const UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this);
	const int32 Body = Profile ? Profile->GetLook().Body : 0;
	if (USkeletalMesh* Mesh = ZSCharacterCatalog::LoadBody(Body))
	{
		Preview->SetSkeletalMeshAsset(Mesh);
		Preview->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (UAnimSequence* Idle = ZSCharacterCatalog::LoadIdle(Body))
		{
			Preview->PlayAnimation(Idle, true);
		}
	}
}

void AZSStation::ApplyVisuals()
{
	Sign->SetText(StationName(Type));

	// A plain dark workbench for every station, with props that say what it is.
	Table->SetStaticMesh(Mesh(TEXT("/Engine/BasicShapes/Cube.Cube")));
	Table->SetRelativeLocation(FVector(0.f, 0.f, -5.f));
	Table->SetRelativeScale3D(Type == EZSStationType::MapTable ? FVector(2.4f, 1.6f, 0.9f) : FVector(1.8f, 1.f, 0.9f));
	// Worn steel workbench (it used to render plain white: the engine shape material ignored our colour).
	Table->SetMaterial(0, KitOrPaint(this, Type == EZSStationType::MapTable ? Tread : Steel, FLinearColor(0.08f, 0.07f, 0.06f)));
	Collision->SetBoxExtent(Type == EZSStationType::MapTable ? FVector(120.f, 80.f, 50.f) : FVector(90.f, 50.f, 50.f));

	PropA->SetStaticMesh(nullptr);
	PropB->SetStaticMesh(nullptr);
	Preview->SetVisibility(Type == EZSStationType::Character);
	switch (Type)
	{
	case EZSStationType::Armory:
		PropA->SetStaticMesh(Mesh(TEXT("/Game/ZS_Imported/Weapons/RD704/SM_RD704_Receiver.SM_RD704_Receiver")));
		PropA->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 46.f), FRotator(0.f, -90.f, 90.f));
		PropB->SetStaticMesh(Mesh(TEXT("/Game/ZS_Imported/Weapons/RD704/SM_RD704_Stock.SM_RD704_Stock")));
		PropB->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 46.f), FRotator(0.f, -90.f, 90.f));
		break;
	case EZSStationType::Market:
		PropA->SetStaticMesh(Mesh(TEXT("/Game/ZS_Imported/Props/AmmoBoxes/SM_Ammobox_50.SM_Ammobox_50")));
		PropA->SetRelativeLocation(FVector(-30.f, 0.f, 45.f));
		PropB->SetStaticMesh(Mesh(TEXT("/Game/ZS_Imported/Props/AmmoBoxes/SM_Ammobox_40mm_Grenade_Closed.SM_Ammobox_40mm_Grenade_Closed")));
		PropB->SetRelativeLocation(FVector(40.f, 10.f, 45.f));
		break;
	case EZSStationType::Character:
		// A tall mirror / locker.
		PropA->SetStaticMesh(Mesh(TEXT("/Engine/BasicShapes/Cube.Cube")));
		PropA->SetRelativeLocationAndRotation(FVector(30.f, 0.f, 100.f), FRotator::ZeroRotator);
		PropA->SetRelativeScale3D(FVector(0.1f, 0.9f, 2.f));
		PropA->SetMaterial(0, KitOrPaint(this, Corrugated, FLinearColor(0.1f, 0.12f, 0.1f)));
		break;
	case EZSStationType::Missions:
		PropA->SetStaticMesh(Mesh(TEXT("/Engine/BasicShapes/Plane.Plane")));
		PropA->SetRelativeLocationAndRotation(FVector(40.f, 0.f, 110.f), FRotator(90.f, 0.f, 0.f));
		PropA->SetRelativeScale3D(FVector(1.f, 1.6f, 1.f));
		PropA->SetMaterial(0, KitOrPaint(this, TEXT("/Game/MS_NewYorkCityKitbash_Assets/Materials/M_Wood01.M_Wood01"), FLinearColor(0.2f, 0.13f, 0.07f)));   // board (was white: empty kit path)
		break;
	case EZSStationType::MapTable:
		PropA->SetStaticMesh(Mesh(TEXT("/Engine/BasicShapes/Plane.Plane")));
		PropA->SetRelativeLocation(FVector(0.f, 0.f, 41.f));
		PropA->SetRelativeScale3D(FVector(2.f, 1.3f, 1.f));
		PropA->SetMaterial(0, KitOrPaint(this, TEXT(""), FLinearColor(0.05f, 0.2f, 0.08f), 0.6f));   // glowing tactical map screen
		break;
	}
}
