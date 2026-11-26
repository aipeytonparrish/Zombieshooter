#include "World/ZSMovementCourse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "World/ZSTargetDummy.h"
#include "World/ZSSpawnPoint.h"
#include "World/ZSAmmoCrate.h"
#include "Rounds/ZSGameMode_Survival.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AZSMovementCourse::AZSMovementCourse()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Static);

	Blocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Blocks"));
	Blocks->SetupAttachment(Root);
	Blocks->SetMobility(EComponentMobility::Static);
	Blocks->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Blocks->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GridMaterial(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (GridMaterial.Succeeded())
	{
		BlockMaterial = GridMaterial.Object;
	}

	for (int32 i = 0; i < NumLabels; ++i)
	{
		UTextRenderComponent* Label = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("Label%d"), i));
		Label->SetupAttachment(Root);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(60.f);
		Label->SetTextRenderColor(FColor(255, 200, 40));
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Labels.Add(Label);
	}
}

void AZSMovementCourse::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AZSMovementCourse::AddBox(const FVector& Min, const FVector& Max)
{
	const FVector Size = (Max - Min).GetAbs();
	const FVector Center = (Min + Max) * 0.5f;
	// The engine cube is 100 cm with its pivot at the centre.
	Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f));
}

void AZSMovementCourse::AddRamp(float StartX, float CenterY, float Length, float Width, float BaseZ, float Rise)
{
	const float Thickness = 30.f;
	const float Angle = FMath::Atan2(Rise, Length);
	const float SlopeLength = FMath::Sqrt(Length * Length + Rise * Rise);

	// Top surface midpoint, then push down along the surface normal by half the thickness.
	const FVector TopMid(StartX + Length * 0.5f, CenterY, BaseZ + Rise * 0.5f);
	const FVector Normal(-FMath::Sin(Angle), 0.f, FMath::Cos(Angle));
	const FVector Center = TopMid - Normal * (Thickness * 0.5f);

	const FRotator Rotation(FMath::RadiansToDegrees(Angle), 0.f, 0.f);
	Blocks->AddInstance(FTransform(Rotation, Center, FVector(SlopeLength, Width, Thickness) / 100.f));
}

void AZSMovementCourse::SetLabel(int32 Index, const FVector& Location, const FString& Text)
{
	if (Labels.IsValidIndex(Index) && Labels[Index])
	{
		Labels[Index]->SetRelativeLocation(Location);
		// Face back toward the start of the course (-X).
		Labels[Index]->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
		Labels[Index]->SetText(FText::FromString(Text));
	}
}

void AZSMovementCourse::Rebuild()
{
	if (!Blocks)
	{
		return;
	}

	Blocks->ClearInstances();
	if (BlockMaterial)
	{
		Blocks->SetMaterial(0, BlockMaterial);
	}

	// ---- Floor (top surface at Z = 0) --------------------------------------------------------
	if (bBuildFloor)
	{
		AddBox(FVector(-1500.f, -3200.f, -40.f), FVector(8500.f, 7200.f, 0.f));
	}

	// ---- Lane 1: Dash gap (Y = -2400) --------------------------------------------------------
	// Two 2 m platforms with a 7 m gap: sprint, jump, dash across. Climb up via the 1 m step.
	{
		const float Y = -2400.f;
		AddBox(FVector(300.f, Y - 250.f, 0.f), FVector(600.f, Y + 250.f, 100.f));    // step
		AddBox(FVector(600.f, Y - 250.f, 0.f), FVector(1600.f, Y + 250.f, 200.f));   // platform A
		AddBox(FVector(2300.f, Y - 250.f, 0.f), FVector(3300.f, Y + 250.f, 200.f));  // platform B (7 m gap)
		AddBox(FVector(4300.f, Y - 250.f, 0.f), FVector(5300.f, Y + 250.f, 200.f));  // platform C (10 m gap: slide-hop + double jump + dash)
		SetLabel(0, FVector(200.f, Y, 260.f), TEXT("DASH GAPS 7m / 10m"));
	}

	// ---- Lane 2: Mantle heights (Y = -1200) ------------------------------------------------
	{
		const float Y = -1200.f;
		const float Heights[] = { 60.f, 100.f, 150.f, 190.f, 230.f, 300.f };
		float X = 600.f;
		for (const float H : Heights)
		{
			AddBox(FVector(X, Y - 200.f, 0.f), FVector(X + 300.f, Y + 200.f, H));
			X += 600.f;
		}
		// A 2.3 m wall with a crouch-height gap on top (mantle into crouch).
		AddBox(FVector(X, Y - 200.f, 0.f), FVector(X + 300.f, Y + 200.f, 200.f));
		AddBox(FVector(X, Y - 200.f, 310.f), FVector(X + 300.f, Y + 200.f, 360.f));
		SetLabel(1, FVector(200.f, Y, 260.f), TEXT("MANTLE 0.6 / 1.0 / 1.5 / 1.9 / 2.3 / 3.0(no) / crouch-gap"));
	}

	// ---- Lane 3: Slide tunnel + slide ramp (Y = 0) ------------------------------------------
	{
		const float Y = 0.f;
		// Tunnel: 1.15 m clearance, only passable crouched or sliding.
		AddBox(FVector(1600.f, Y - 200.f, 0.f), FVector(2200.f, Y - 160.f, 300.f));
		AddBox(FVector(1600.f, Y + 160.f, 0.f), FVector(2200.f, Y + 200.f, 300.f));
		AddBox(FVector(1600.f, Y - 200.f, 115.f), FVector(2200.f, Y + 200.f, 160.f));
		// Ramp up, platform, long ramp down (slide down it to gain speed).
		AddRamp(2600.f, Y, 700.f, 400.f, 0.f, 250.f);
		AddBox(FVector(3300.f, Y - 200.f, 0.f), FVector(3700.f, Y + 200.f, 250.f));
		AddRamp(3700.f, Y, 1200.f, 400.f, 250.f, -250.f);
		SetLabel(2, FVector(200.f, Y, 260.f), TEXT("SLIDE: tunnel 1.15m, then ramp"));
	}

	// ---- Lane 4: Wall-run across a gap (Y = 1400) -------------------------------------------
	{
		const float Y = 1400.f;
		AddBox(FVector(300.f, Y - 250.f, 0.f), FVector(600.f, Y + 250.f, 100.f));     // step
		AddBox(FVector(600.f, Y - 250.f, 0.f), FVector(1300.f, Y + 250.f, 150.f));    // start platform
		AddBox(FVector(2800.f, Y - 250.f, 0.f), FVector(3500.f, Y + 250.f, 150.f));   // end platform (15 m gap)
		AddBox(FVector(1000.f, Y + 300.f, 0.f), FVector(3100.f, Y + 340.f, 650.f));   // runnable wall on the right
		SetLabel(3, FVector(200.f, Y, 260.f), TEXT("WALL-RUN (wall on right)"));
	}

	// ---- Lane 5: Wall-run corridor / wall jumps (Y = 2600) ----------------------------------
	{
		const float Y = 2600.f;
		AddBox(FVector(800.f, Y - 290.f, 0.f), FVector(3800.f, Y - 250.f, 650.f));
		AddBox(FVector(800.f, Y + 250.f, 0.f), FVector(3800.f, Y + 290.f, 650.f));
		AddBox(FVector(3800.f, Y - 290.f, 0.f), FVector(4200.f, Y + 290.f, 400.f));   // goal ledge (mantle from a wall jump)
		SetLabel(4, FVector(200.f, Y, 260.f), TEXT("WALL-JUMP CORRIDOR"));
	}

	// ---- Lane 6: Double-jump tower (Y = 3800) -----------------------------------------------
	{
		const float Y = 3800.f;
		const float Heights[] = { 150.f, 400.f, 650.f, 900.f };
		float X = 700.f;
		for (const float H : Heights)
		{
			AddBox(FVector(X, Y - 200.f, 0.f), FVector(X + 350.f, Y + 200.f, H));
			X += 350.f;
		}
		SetLabel(5, FVector(200.f, Y, 260.f), TEXT("DOUBLE JUMP / AIR-MANTLE STEPS (+2.5m)"));
	}

	// ---- Lane 7: Shooting range (Y = 5400 .. 6800) ----------------------------------------------
	// Firing line at X = 0. Targets at 10 / 20 / 35 / 50 m (spawned in BeginPlay), a strafing target at 20 m,
	// a 20 cm wall (penetrable by the rifle) and a 60 cm wall (not) with targets behind them.
	{
		AddBox(FVector(-60.f, 5000.f, 0.f), FVector(-40.f, 7000.f, 100.f));       // firing-line rail
		AddBox(FVector(1400.f, 6250.f, 0.f), FVector(1420.f, 6550.f, 220.f));    // thin wall (20 cm)
		AddBox(FVector(1400.f, 6650.f, 0.f), FVector(1460.f, 6950.f, 220.f));    // thick wall (60 cm)
		AddBox(FVector(5600.f, 4900.f, 0.f), FVector(5700.f, 7100.f, 600.f));    // backstop
		SetLabel(6, FVector(200.f, 5900.f, 260.f), TEXT("SHOOTING RANGE 10 / 20 / 35 / 50 m  (walls: 20 cm / 60 cm)"));
	}
}

void AZSMovementCourse::BeginPlay()
{
	Super::BeginPlay();

	// Survival: no range targets; add spawn points + ammo crates unless the level already has its own.
	if (GetWorld()->GetAuthGameMode<AZSGameMode_Survival>())
	{
		if (!TActorIterator<AZSSpawnPoint>(GetWorld()))
		{
			SpawnSurvivalProps();
		}
		return;
	}
	if (!bSpawnTargets)
	{
		return;
	}

	struct FTargetSpec { FVector Location; bool bMoving; };
	const FTargetSpec Specs[] = {
		{ FVector(1000.f, 5300.f, 0.f), false },
		{ FVector(1000.f, 5650.f, 0.f), false },
		{ FVector(2000.f, 5900.f, 0.f), true  },   // strafes 4 m
		{ FVector(1700.f, 6400.f, 0.f), false },   // behind the thin wall
		{ FVector(1700.f, 6800.f, 0.f), false },   // behind the thick wall
		{ FVector(3500.f, 5400.f, 0.f), false },
		{ FVector(3500.f, 5900.f, 0.f), false },
		{ FVector(5000.f, 5650.f, 0.f), false },
	};

	const FTransform& ToWorld = GetActorTransform();
	for (const FTargetSpec& Spec : Specs)
	{
		const FTransform T(GetActorRotation(), ToWorld.TransformPosition(Spec.Location));
		if (AZSTargetDummy* Dummy = GetWorld()->SpawnActorDeferred<AZSTargetDummy>(AZSTargetDummy::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Dummy->bMoving = Spec.bMoving;
			Dummy->FinishSpawning(T);
		}
	}
}

void AZSMovementCourse::SpawnSurvivalProps()
{
	const FTransform& ToWorld = GetActorTransform();
	const FVector2D Spawns[] = {
		{ -1300.f, -3000.f }, { 3500.f, -3000.f }, { 8300.f, -3000.f },
		{ -1300.f, 7000.f }, { 3500.f, 7000.f }, { 8300.f, 7000.f },
		{ 8300.f, 2000.f }, { 6000.f, -1500.f }, { 6000.f, 4000.f },
		{ 2500.f, 4600.f }, { 1500.f, -2000.f }, { -1300.f, 3500.f },
	};
	for (const FVector2D& S : Spawns)
	{
		const FVector Loc = ToWorld.TransformPosition(FVector(S.X, S.Y, 95.f));
		GetWorld()->SpawnActor<AZSSpawnPoint>(AZSSpawnPoint::StaticClass(), Loc, FRotator::ZeroRotator);
	}

	const FVector Crates[] = { { -450.f, 450.f, 0.f }, { 2600.f, 5600.f, 0.f }, { 6200.f, 1500.f, 0.f } };
	for (const FVector& C : Crates)
	{
		GetWorld()->SpawnActor<AZSAmmoCrate>(AZSAmmoCrate::StaticClass(), ToWorld.TransformPosition(C), GetActorRotation());
	}
}
