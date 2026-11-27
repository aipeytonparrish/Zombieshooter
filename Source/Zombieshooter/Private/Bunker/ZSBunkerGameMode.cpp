#include "Bunker/ZSBunkerGameMode.h"
#include "Profile/ZSProfileSubsystem.h"
#include "Engine/World.h"

AZSBunkerGameMode::AZSBunkerGameMode()
{
}

void AZSBunkerGameMode::BeginPlay()
{
	Super::BeginPlay();
	FZSRunResult Previous;
	if (UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this); Profile && Profile->ConsumeForceQuitNotice(Previous))
	{
		FString Lost;
		for (const FText& T : Previous.LostItems)
		{
			Lost += (Lost.IsEmpty() ? TEXT("") : TEXT(", ")) + T.ToString();
		}
		Notice = FText::Format(NSLOCTEXT("ZS", "ForceQuitNotice", "Your last run was quit mid-match and counts as a loss{0}"),
			Lost.IsEmpty() ? FText::GetEmpty() : FText::FromString(FString::Printf(TEXT(" - lost: %s"), *Lost)));
		NoticeTime = 10.f;
	}
}
