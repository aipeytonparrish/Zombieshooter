#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ZSMissionSubsystem.generated.h"

/** What a mission counts. */
enum class EZSMissionStat : uint8
{
	Kills,
	Headshots,
	RunnerKills,
	MultiKills,       // kills chained within the multi-kill window
	RoundsSurvived,   // total, across runs
	ReachRound,       // best round in a single run
	CratesUsed,
	PickupsCollected,
	TasksCompleted,   // in-run timed tasks
	RunsWon,
};

enum class EZSMissionKind : uint8
{
	Daily,
	Weekly,
	Story
};

struct FZSMission
{
	FName Id;
	EZSMissionKind Kind = EZSMissionKind::Daily;
	FText Title;
	EZSMissionStat Stat = EZSMissionStat::Kills;
	int32 Target = 1;
	int32 RewardXP = 0;
	int32 RewardCash = 0;
};

/** An active mission with its progress. */
struct FZSMissionStatus
{
	FZSMission Mission;
	int32 Saved = 0;      // banked progress
	int32 InRun = 0;      // this run's progress (not banked until the run is won)
	bool bComplete = false;

	int32 Total() const { return FMath::Min(Mission.Target, Saved + InRun); }
};

/**
 * Daily missions (3, new set every UTC day), weekly missions (2, every Monday UTC) and a story chain (one chapter at a
 * time). Progress is stored in the profile save. Everything counted during a run is held back and only banked when the
 * run is WON - a failed run loses its mission progress too. Rewards are paid automatically when a mission completes.
 */
UCLASS()
class ZOMBIESHOOTER_API UZSMissionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UZSMissionSubsystem* Get(const UObject* WorldContext);

	static constexpr int32 NumDailies = 3;
	static constexpr int32 NumWeeklies = 2;

	static int32 DayIndex();
	static int32 WeekIndex();
	static FTimespan TimeToDailyReset();
	static FTimespan TimeToWeeklyReset();

	/** Today's dailies, this week's weeklies and the current story chapter (if any), with progress. */
	TArray<FZSMissionStatus> GetActive() const;
	/** Story chapters done / total. */
	int32 GetStoryChapter() const;
	int32 GetStoryChapterCount() const;

	// ---- Run tracking ----
	void BeginRun();
	void Record(EZSMissionStat Stat, int32 Amount = 1);
	/** Bank (won) or discard the run's progress. Returns display lines for completed missions (rewards already paid). */
	TArray<FText> EndRun(bool bWon);
	bool IsRunActive() const { return bRunActive; }

private:
	FString Key(const FZSMission& M) const;
	int32 GetSaved(const FZSMission& M) const;
	int32 RunValue(EZSMissionStat Stat) const;

	TMap<uint8, int32> RunStats;
	bool bRunActive = false;
};
