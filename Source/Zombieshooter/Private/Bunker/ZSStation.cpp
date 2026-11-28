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
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

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

	/** The owner's Hong Kong city pack (market stalls, tables, clothes racks): what the camp's stations are built from. */
	UStaticMesh* HK(const TCHAR* Rel)
	{
		const FString Name = FPaths::GetBaseFilename(Rel);
		return Mesh(*FString::Printf(TEXT("/Game/CityCore_HongKong/Models/%s.%s"), Rel, *Name));
	}

	constexpr float FloorZ = -50.f;   // the station's root sits 50 cm above the roof

	/** Lays a weapon part flat on a table: of the three right-angle orientations, the one whose height is smallest. */
	FRotator LayFlat(const UStaticMesh* M, float Yaw)
	{
		FRotator Best(0.f, Yaw, 0.f);
		if (!M)
		{
			return Best;
		}
		float BestZ = BIG_NUMBER;
		for (const FRotator& R : { FRotator(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 90.f), FRotator(90.f, 0.f, 0.f) })
		{
			const float Z = M->GetBoundingBox().TransformBy(FTransform(R)).GetSize().Z;
			if (Z < BestZ - 0.5f)
			{
				BestZ = Z;
				Best = (FRotator(0.f, Yaw, 0.f).Quaternion() * R.Quaternion()).Rotator();
			}
		}
		return Best;
	}

	/**
	 * Puts Mesh on C standing on the floor at At (XY), turned by Yaw. Uniform: scaled by the tightest of the Box axes
	 * that are > 0. Otherwise stretched to Box exactly (tables). Returns the top's height (relative Z).
	 */
	float Fit(UStaticMeshComponent* C, UStaticMesh* M, const FVector& Box, bool bUniform, const FVector2D& At = FVector2D::ZeroVector, float Yaw = 0.f, float BaseZ = FloorZ)
	{
		C->EmptyOverrideMaterials();
		C->SetStaticMesh(M);
		if (!M)
		{
			return BaseZ;
		}
		const FBox Bounds = M->GetBoundingBox();
		const FVector Size = Bounds.GetSize().ComponentMax(FVector(1.f));
		FVector Scale(1.f);
		if (bUniform)
		{
			float K = 1000.f;
			for (int32 i = 0; i < 3; ++i) { if (Box[i] > 0.f) { K = FMath::Min(K, Box[i] / Size[i]); } }
			Scale = FVector(K);
		}
		else
		{
			Scale = FVector(Box.X / Size.X, Box.Y / Size.Y, Box.Z / Size.Z);
		}
		const FRotator Rot(0.f, Yaw, 0.f);
		const FVector Centre = Bounds.GetCenter();
		const FVector Off = Rot.RotateVector(FVector(-Centre.X * Scale.X, -Centre.Y * Scale.Y, 0.f));
		C->SetRelativeScale3D(Scale);
		C->SetRelativeRotation(Rot);
		C->SetRelativeLocation(FVector(At.X + Off.X, At.Y + Off.Y, BaseZ - Bounds.Min.Z * Scale.Z));
		return BaseZ + Size.Z * Scale.Z;
	}
}

AZSStation::AZSStation()
{
	PrimaryActorTick.bCanEverTick = true;   // the sign turns to face the player (a fixed sign read edge-on from half the camp)

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
	PropC = MakeMesh(TEXT("PropC"));

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
	Lamp->SetCastShadows(false);   // shadowed local lights + Nanite VSM hang this GPU

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

void AZSStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - Sign->GetComponentLocation();
		Sign->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));   // text faces its +X
	}
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

	// Each station is a real piece of camp furniture with props that say what it is (they used to be plain boxes).
	for (UStaticMeshComponent* C : { PropA.Get(), PropB.Get(), PropC.Get() })
	{
		C->EmptyOverrideMaterials();
		C->SetStaticMesh(nullptr);
		C->SetRelativeRotation(FRotator::ZeroRotator);
		C->SetRelativeScale3D(FVector(1.f));
	}
	Preview->SetVisibility(Type == EZSStationType::Character);
	UStaticMesh* WoodTable = HK(TEXT("Props/SM_TableRestaurant_01"));
	UMaterialInterface* Wood = Mat(TEXT("/Game/MS_NewYorkCityKitbash_Assets/Materials/M_Wood01.M_Wood01"));   // planks, not the pack's orange
	if (!WoodTable)
	{
		// Hong Kong pack missing: the old plain workbench.
		Table->EmptyOverrideMaterials();
		Table->SetStaticMesh(Mesh(TEXT("/Engine/BasicShapes/Cube.Cube")));
		Table->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -5.f), FRotator::ZeroRotator);
		Table->SetRelativeScale3D(Type == EZSStationType::MapTable ? FVector(2.4f, 1.6f, 0.9f) : FVector(1.8f, 1.f, 0.9f));
		Table->SetMaterial(0, KitOrPaint(this, Steel, FLinearColor(0.08f, 0.07f, 0.06f)));
		Collision->SetBoxExtent(Type == EZSStationType::MapTable ? FVector(120.f, 80.f, 50.f) : FVector(90.f, 50.f, 50.f));
		if (Type == EZSStationType::MapTable)
		{
			PropA->SetStaticMesh(Mesh(TEXT("/Engine/BasicShapes/Plane.Plane")));
			PropA->SetRelativeLocation(FVector(0.f, 0.f, 41.f));
			PropA->SetRelativeScale3D(FVector(2.f, 1.3f, 1.f));
			PropA->SetMaterial(0, KitOrPaint(this, TEXT(""), FLinearColor(0.05f, 0.2f, 0.08f), 0.6f));
		}
		return;
	}
	switch (Type)
	{
	case EZSStationType::Armory:
	{
		// Gunsmith's bench: the rifle laid out on a long table, ammo cans beside it.
		const float Top = Fit(Table, WoodTable, FVector(180.f, 95.f, 92.f), false);
		Table->SetMaterial(0, Wood);
		// The rifle laid flat on the bench (receiver and stock share a pivot, so one transform keeps them together).
		UStaticMesh* Receiver = Mesh(TEXT("/Game/ZS_Imported/Weapons/RD704/SM_RD704_Receiver.SM_RD704_Receiver"));
		const FRotator Flat = LayFlat(Receiver, 0.f);
		const float Lift = Receiver ? -Receiver->GetBoundingBox().TransformBy(FTransform(Flat)).Min.Z : 4.f;
		PropA->SetStaticMesh(Receiver);
		PropA->SetRelativeLocationAndRotation(FVector(0.f, -10.f, Top + Lift + 4.f), Flat);
		PropB->SetStaticMesh(Mesh(TEXT("/Game/ZS_Imported/Weapons/RD704/SM_RD704_Stock.SM_RD704_Stock")));
		PropB->SetRelativeLocationAndRotation(FVector(0.f, -10.f, Top + Lift + 4.f), Flat);
		Fit(PropC, Mesh(TEXT("/Game/ZS_Imported/Props/AmmoBoxes/SM_Ammobox_556_NATO_Closed.SM_Ammobox_556_NATO_Closed")), FVector(40.f, 0.f, 0.f), true, FVector2D(-62.f, 26.f), 15.f, Top);
		Collision->SetBoxExtent(FVector(90.f, 50.f, 50.f));
		break;
	}
	case EZSStationType::Market:
	{
		// The trader's counter under her tarp: crates of produce and a can of rounds on the planks, more stock below.
		const float Top = Fit(Table, WoodTable, FVector(180.f, 90.f, 90.f), false);
		Table->SetMaterial(0, Wood);
		Fit(PropA, HK(TEXT("Market/SM_BoxLimes_01")), FVector(50.f, 0.f, 0.f), true, FVector2D(-45.f, -5.f), 8.f, Top);
		Fit(PropB, HK(TEXT("Market/SM_BoxPotatos_01")), FVector(50.f, 0.f, 0.f), true, FVector2D(15.f, 10.f), -10.f, Top);
		Fit(PropC, Mesh(TEXT("/Game/ZS_Imported/Props/AmmoBoxes/SM_Ammobox_9mm_Closed.SM_Ammobox_9mm_Closed")), FVector(38.f, 0.f, 0.f), true, FVector2D(65.f, -10.f), 80.f, Top);
		Collision->SetBoxExtent(FVector(95.f, 65.f, 50.f));
		break;
	}
	case EZSStationType::Character:
	{
		// The wardrobe: a clothes rack and a folded pile on a crate.
		Fit(Table, HK(TEXT("Market/SM_HK_MarketClothesHanger_01")), FVector(170.f, 100.f, 150.f), true);
		const float CrateTop = Fit(PropA, HK(TEXT("Market/SM_HK_MarketBox_01")), FVector(55.f, 0.f, 0.f), true, FVector2D(10.f, 95.f), 5.f);
		Fit(PropB, HK(TEXT("Market/SM_StackClothes_01")), FVector(36.f, 0.f, 0.f), true, FVector2D(10.f, 95.f), 30.f, CrateTop);
		Collision->SetBoxExtent(FVector(85.f, 50.f, 50.f));
		break;
	}
	case EZSStationType::Missions:
	{
		// Ops desk: a table with papers, the job list chalked on a board behind it.
		const float Top = Fit(Table, WoodTable, FVector(150.f, 75.f, 90.f), false);
		Table->SetMaterial(0, Wood);
		Fit(PropA, HK(TEXT("Props/StreetProps/SM_FrameChalkboard")), FVector(0.f, 0.f, 175.f), true, FVector2D(70.f, 0.f), 90.f);
		Fit(PropB, HK(TEXT("Trash/SM_CardboardBox_01")), FVector(38.f, 0.f, 0.f), true, FVector2D(-15.f, 25.f), 20.f, Top);
		Fit(PropC, HK(TEXT("Props/SM_CoffeeCupWithPlate_02")), FVector(14.f, 0.f, 0.f), true, FVector2D(-20.f, -30.f), 0.f, Top);
		Collision->SetBoxExtent(FVector(80.f, 45.f, 50.f));
		break;
	}
	case EZSStationType::MapTable:
	{
		// Planning table with the city map glowing on it.
		const float Top = Fit(Table, WoodTable, FVector(240.f, 160.f, 90.f), false);
		Table->SetMaterial(0, Wood);
		PropA->SetStaticMesh(Mesh(TEXT("/Engine/BasicShapes/Plane.Plane")));
		PropA->SetRelativeLocation(FVector(0.f, 0.f, Top + 0.6f));
		PropA->SetRelativeScale3D(FVector(2.f, 1.3f, 1.f));
		PropA->SetMaterial(0, KitOrPaint(this, TEXT(""), FLinearColor(0.05f, 0.2f, 0.08f), 0.6f));   // glowing tactical map
		Fit(PropB, HK(TEXT("Props/SM_CoffeeCupWithPlate_02")), FVector(14.f, 0.f, 0.f), true, FVector2D(-105.f, 65.f), 0.f, Top);
		Fit(PropC, Mesh(TEXT("/Game/ZS_Imported/Props/AmmoBoxes/SM_Ammobox_50_Closed.SM_Ammobox_50_Closed")), FVector(38.f, 0.f, 0.f), true, FVector2D(100.f, -62.f), 30.f, Top);
		Collision->SetBoxExtent(FVector(120.f, 80.f, 50.f));
		break;
	}
	}
}
