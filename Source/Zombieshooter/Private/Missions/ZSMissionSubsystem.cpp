#include "Missions/ZSMissionSubsystem.h"
#include "Profile/ZSProfileSubsystem.h"
#include "Profile/ZSSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Zombieshooter.h"

namespace
{
	using S = EZSMissionStat;
	using K = EZSMissionKind;

	FZSMission M(const TCHAR* Id, K Kind, const TCHAR* Title, S Stat, int32 Target, int32 XP, int32 Cash)
	{
		FZSMission Out;
		Out.Id = Id;
		Out.Kind = Kind;
		Out.Title = FText::FromString(Title);
		Out.Stat = Stat;
		Out.Target = Target;
		Out.RewardXP = XP;
		Out.RewardCash = Cash;
		return Out;
	}

	const TArray<FZSMission>& DailyPool()
	{
		static const TArray<FZSMission> P = {
			M(TEXT("D_KILL150"), K::Daily, TEXT("Kill 150 zombies"), S::Kills, 150, 600, 300),
			M(TEXT("D_KILL300"), K::Daily, TEXT("Kill 300 zombies"), S::Kills, 300, 1000, 500),
			M(TEXT("D_HEAD40"), K::Daily, TEXT("Land 40 headshot kills"), S::Headshots, 40, 700, 350),
			M(TEXT("D_RUNNER25"), K::Daily, TEXT("Kill 25 runners"), S::RunnerKills, 25, 600, 300),
			M(TEXT("D_ROUNDS15"), K::Daily, TEXT("Survive 15 rounds"), S::RoundsSurvived, 15, 700, 350),
			M(TEXT("D_REACH10"), K::Daily, TEXT("Reach round 10 in one run"), S::ReachRound, 10, 800, 400),
			M(TEXT("D_CRATES8"), K::Daily, TEXT("Refill at ammo crates 8 times"), S::CratesUsed, 8, 400, 250),
			M(TEXT("D_TASKS3"), K::Daily, TEXT("Complete 3 timed tasks"), S::TasksCompleted, 3, 800, 400),
			M(TEXT("D_MULTI30"), K::Daily, TEXT("Chain 30 multi-kills"), S::MultiKills, 30, 600, 300),
			M(TEXT("D_PICKUP6"), K::Daily, TEXT("Collect 6 power-ups or armor plates"), S::PickupsCollected, 6, 500, 300),
		};
		return P;
	}

	const TArray<FZSMission>& WeeklyPool()
	{
		static const TArray<FZSMission> P = {
			M(TEXT("W_KILL2000"), K::Weekly, TEXT("Kill 2,000 zombies"), S::Kills, 2000, 4000, 2000),
			M(TEXT("W_HEAD400"), K::Weekly, TEXT("Land 400 headshot kills"), S::Headshots, 400, 4000, 2000),
			M(TEXT("W_WIN2"), K::Weekly, TEXT("Win 2 runs"), S::RunsWon, 2, 5000, 2500),
			M(TEXT("W_TASKS15"), K::Weekly, TEXT("Complete 15 timed tasks"), S::TasksCompleted, 15, 4000, 2000),
			M(TEXT("W_ROUNDS80"), K::Weekly, TEXT("Survive 80 rounds"), S::RoundsSurvived, 80, 4000, 2000),
			M(TEXT("W_RUNNER200"), K::Weekly, TEXT("Kill 200 runners"), S::RunnerKills, 200, 3500, 1800),
		};
		return P;
	}

	const TArray<FZSMission>& StoryChain()
	{
		static const TArray<FZSMission> P = {
			M(TEXT("S01"), K::Story, TEXT("Chapter 1 - First Night: reach round 5"), S::ReachRound, 5, 500, 300),
			M(TEXT("S02"), K::Story, TEXT("Chapter 2 - Scavenger: refill at 5 ammo crates"), S::CratesUsed, 5, 600, 400),
			M(TEXT("S03"), K::Story, TEXT("Chapter 3 - Holding the Line: reach round 10"), S::ReachRound, 10, 1000, 600),
			M(TEXT("S04"), K::Story, TEXT("Chapter 4 - Field Work: complete 3 timed tasks"), S::TasksCompleted, 3, 1200, 700),
			M(TEXT("S05"), K::Story, TEXT("Chapter 5 - Marksman: 150 headshot kills"), S::Headshots, 150, 1500, 800),
			M(TEXT("S06"), K::Story, TEXT("Chapter 6 - Deep Night: reach round 15"), S::ReachRound, 15, 2000, 1000),
			M(TEXT("S07"), K::Story, TEXT("Chapter 7 - The Mutant: win a run"), S::RunsWon, 1, 4000, 2500),
		};
		return P;
	}

	/** Picks Count distinct entries, seeded. */
	TArray<FZSMission> Pick(const TArray<FZSMission>& Pool, int32 Count, int32 Seed)
	{
		TArray<FZSMission> Copy = Pool;
		FRandomStream Rng(Seed);
		for (int32 i = Copy.Num() - 1; i > 0; --i)
		{
			Copy.Swap(i, Rng.RandRange(0, i));
		}
		Copy.SetNum(FMath::Min(Count, Copy.Num()));
		return Copy;
	}
}

UZSMissionSubsystem* UZSMissionSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UZSMissionSubsystem>() : nullptr;
}

int32 UZSMissionSubsystem::DayIndex()
{
	return static_cast<int32>(FDateTime::UtcNow().GetTicks() / ETimespan::TicksPerDay);
}

int32 UZSMissionSubsystem::WeekIndex()
{
	// 0001-01-01 was a Monday, so weeks start on Monday UTC.
	return DayIndex() / 7;
}

FTimespan UZSMissionSubsystem::TimeToDailyReset()
{
	const FDateTime Now = FDateTime::UtcNow();
	return (Now.GetDate() + FTimespan::FromDays(1.0)) - Now;
}

FTimespan UZSMissionSubsystem::TimeToWeeklyReset()
{
	const FDateTime Now = FDateTime::UtcNow();
	const int32 DaysLeft = 7 - (DayIndex() % 7);
	return (Now.GetDate() + FTimespan::FromDays(DaysLeft)) - Now;
}

FString UZSMissionSubsystem::Key(const FZSMission& Mission) const
{
	switch (Mission.Kind)
	{
	case EZSMissionKind::Daily:  return FString::Printf(TEXT("D%d_%s"), DayIndex(), *Mission.Id.ToString());
	case EZSMissionKind::Weekly: return FString::Printf(TEXT("W%d_%s"), WeekIndex(), *Mission.Id.ToString());
	default:                     return FString::Printf(TEXT("STORY_%s"), *Mission.Id.ToString());
	}
}

int32 UZSMissionSubsystem::GetSaved(const FZSMission& Mission) const
{
	const UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this);
	const UZSSaveGame* Save = Profile ? Profile->GetSave() : nullptr;
	const int32* V = Save ? Save->MissionProgress.Find(FName(*Key(Mission))) : nullptr;
	return V ? *V : 0;
}

int32 UZSMissionSubsystem::RunValue(EZSMissionStat Stat) const
{
	const int32* V = RunStats.Find(static_cast<uint8>(Stat));
	return V ? *V : 0;
}

int32 UZSMissionSubsystem::GetStoryChapterCount() const
{
	return StoryChain().Num();
}

int32 UZSMissionSubsystem::GetStoryChapter() const
{
	int32 Done = 0;
	for (const FZSMission& Ch : StoryChain())
	{
		if (GetSaved(Ch) < Ch.Target) { break; }
		++Done;
	}
	return Done;
}

TArray<FZSMissionStatus> UZSMissionSubsystem::GetActive() const
{
	TArray<FZSMission> List = Pick(DailyPool(), NumDailies, DayIndex() * 31 + 7);
	List.Append(Pick(WeeklyPool(), NumWeeklies, WeekIndex() * 53 + 11));
	const int32 Chapter = GetStoryChapter();
	if (StoryChain().IsValidIndex(Chapter))
	{
		List.Add(StoryChain()[Chapter]);
	}

	TArray<FZSMissionStatus> Out;
	for (const FZSMission& Mission : List)
	{
		FZSMissionStatus& St = Out.AddDefaulted_GetRef();
		St.Mission = Mission;
		St.Saved = GetSaved(Mission);
		St.bComplete = St.Saved >= Mission.Target;
		if (bRunActive && !St.bComplete)
		{
			const int32 Run = RunValue(Mission.Stat);
			// "Reach round N in one run" counts the best run, not a sum.
			St.InRun = Mission.Stat == EZSMissionStat::ReachRound ? FMath::Max(0, Run - St.Saved) : Run;
		}
	}
	return Out;
}

void UZSMissionSubsystem::BeginRun()
{
	RunStats.Reset();
	bRunActive = true;
}

void UZSMissionSubsystem::Record(EZSMissionStat Stat, int32 Amount)
{
	if (!bRunActive)
	{
		return;
	}
	int32& V = RunStats.FindOrAdd(static_cast<uint8>(Stat));
	V = Stat == EZSMissionStat::ReachRound ? FMath::Max(V, Amount) : V + Amount;
}

TArray<FText> UZSMissionSubsystem::EndRun(bool bWon)
{
	TArray<FText> Lines;
	if (!bRunActive)
	{
		return Lines;
	}
	if (bWon)
	{
		Record(EZSMissionStat::RunsWon, 1);
	}
	const TArray<FZSMissionStatus> Active = GetActive();
	bRunActive = false;

	UZSProfileSubsystem* Profile = UZSProfileSubsystem::Get(this);
	UZSSaveGame* Save = Profile ? Profile->GetSave() : nullptr;
	if (!Save)
	{
		return Lines;
	}
	if (!bWon)
	{
		for (const FZSMissionStatus& St : Active)
		{
			if (St.InRun > 0)
			{
				Lines.Add(NSLOCTEXT("ZS", "MissionLost", "Mission progress from this run was lost"));
				break;
			}
		}
		RunStats.Reset();
		return Lines;
	}

	int32 XP = 0, Cash = 0;
	for (const FZSMissionStatus& St : Active)
	{
		if (St.bComplete || St.InRun <= 0)
		{
			continue;
		}
		const int32 NewValue = FMath::Min(St.Mission.Target, St.Saved + St.InRun);
		Save->MissionProgress.Add(FName(*Key(St.Mission)), NewValue);
		if (NewValue >= St.Mission.Target)
		{
			XP += St.Mission.RewardXP;
			Cash += St.Mission.RewardCash;
			Lines.Add(FText::Format(NSLOCTEXT("ZS", "MissionDone", "Mission complete: {0}  (+{1} XP, ${2})"),
				St.Mission.Title, FText::AsNumber(St.Mission.RewardXP), FText::AsNumber(St.Mission.RewardCash)));
		}
	}
	RunStats.Reset();
	if (XP > 0) { Profile->GiveXP(XP); }
	if (Cash > 0) { Profile->GiveCash(Cash); }
	Profile->Save();
	UE_LOG(LogZS, Log, TEXT("Missions banked: %d completed, +%d XP, +$%d."), Lines.Num(), XP, Cash);
	return Lines;
}
