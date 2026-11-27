#include "Bunker/ZSBunkerLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AZSBunkerLayout::AZSBunkerLayout()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AZSBunkerLayout::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AZSBunkerLayout::BeginPlay()
{
	Super::BeginPlay();
	if (Built.Num() == 0)
	{
		Build();   // construction scripts don't run for actors loaded from a cooked / saved level in game
	}
}

UStaticMeshComponent* AZSBunkerLayout::AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollide)
{
	static UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	static UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ZS/Materials/M_ZS_Paint.M_ZS_Paint"));
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(Cube);
	C->SetRelativeLocation(Center);
	C->SetRelativeScale3D(Size / 100.f);
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	C->SetMobility(EComponentMobility::Movable);
	if (Shape)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Shape, C);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
	}
	C->RegisterComponent();
	Built.Add(C);
	return C;
}

void AZSBunkerLayout::AddLight(const FVector& Location, float Intensity, const FLinearColor& Color, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
	L->SetupAttachment(RootComponent);
	L->SetRelativeLocation(Location);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Intensity);
	L->SetAttenuationRadius(Radius);
	L->SetLightColor(Color);
	L->SetMobility(EComponentMobility::Movable);
	L->SetSourceRadius(12.f);
	L->RegisterComponent();
	Built.Add(L);

	// A small lamp housing so the light has a visible source.
	AddBox(Location + FVector(0.f, 0.f, 12.f), FVector(50.f, 20.f, 8.f), FLinearColor(0.9f, 0.75f, 0.5f), false);
}

void AZSBunkerLayout::Build()
{
	for (UActorComponent* C : Built)
	{
		if (C) { C->DestroyComponent(); }
	}
	Built.Reset();

	const float X = RoomSize.X * 0.5f, Y = RoomSize.Y * 0.5f, H = RoomSize.Z, T = WallThickness;

	// Shell.
	AddBox(FVector(0.f, 0.f, -T * 0.5f), FVector(RoomSize.X + 2 * T, RoomSize.Y + 2 * T, T), FloorColor);
	AddBox(FVector(0.f, 0.f, H + T * 0.5f), FVector(RoomSize.X + 2 * T, RoomSize.Y + 2 * T, T), ConcreteColor * 0.6f);
	AddBox(FVector(X + T * 0.5f, 0.f, H * 0.5f), FVector(T, RoomSize.Y, H), ConcreteColor);
	AddBox(FVector(-X - T * 0.5f, 0.f, H * 0.5f), FVector(T, RoomSize.Y, H), ConcreteColor);
	AddBox(FVector(0.f, Y + T * 0.5f, H * 0.5f), FVector(RoomSize.X, T, H), ConcreteColor);
	AddBox(FVector(0.f, -Y - T * 0.5f, H * 0.5f), FVector(RoomSize.X, T, H), ConcreteColor);

	// Hazard stripe along the bottom of the walls and a blast door at the back.
	const FLinearColor Stripe(0.45f, 0.33f, 0.05f);
	AddBox(FVector(0.f, Y - 2.f, 15.f), FVector(RoomSize.X, 4.f, 30.f), Stripe, false);
	AddBox(FVector(0.f, -Y + 2.f, 15.f), FVector(RoomSize.X, 4.f, 30.f), Stripe, false);
	AddBox(FVector(-X + 6.f, 0.f, 150.f), FVector(12.f, 260.f, 300.f), FLinearColor(0.12f, 0.13f, 0.12f));
	AddBox(FVector(-X + 14.f, 0.f, 150.f), FVector(4.f, 20.f, 300.f), Stripe, false);

	// Ceiling beams and pillars.
	for (float BX = -X + 400.f; BX < X; BX += 400.f)
	{
		AddBox(FVector(BX, 0.f, H - 20.f), FVector(30.f, RoomSize.Y, 40.f), ConcreteColor * 0.8f);
	}
	for (const FVector2D P : { FVector2D(-200.f, -300.f), FVector2D(-200.f, 300.f), FVector2D(800.f, -300.f), FVector2D(800.f, 300.f) })
	{
		AddBox(FVector(P.X, P.Y, H * 0.5f), FVector(50.f, 50.f, H), ConcreteColor * 1.1f);
	}

	// Clutter: supply crates in the corners.
	const FLinearColor Crate(0.2f, 0.17f, 0.1f);
	AddBox(FVector(X - 80.f, Y - 80.f, 50.f), FVector(110.f, 110.f, 100.f), Crate);
	AddBox(FVector(X - 80.f, Y - 80.f, 140.f), FVector(80.f, 80.f, 80.f), Crate * 1.2f);
	AddBox(FVector(X - 80.f, -Y + 80.f, 45.f), FVector(100.f, 140.f, 90.f), Crate);
	AddBox(FVector(-X + 120.f, Y - 70.f, 40.f), FVector(90.f, 90.f, 80.f), Crate * 0.9f);
	AddBox(FVector(-X + 120.f, -Y + 70.f, 60.f), FVector(70.f, 70.f, 120.f), FLinearColor(0.1f, 0.12f, 0.08f));

	// Warm work lights down the middle, cold light near the door.
	const FLinearColor Warm(1.f, 0.78f, 0.52f);
	for (float LX = -X + 300.f; LX < X; LX += 500.f)
	{
		AddLight(FVector(LX, 0.f, H - 60.f), LightIntensity, Warm, 1400.f);
	}
	AddLight(FVector(-X + 150.f, 0.f, H - 80.f), LightIntensity * 0.4f, FLinearColor(0.6f, 0.75f, 1.f), 700.f);
}
