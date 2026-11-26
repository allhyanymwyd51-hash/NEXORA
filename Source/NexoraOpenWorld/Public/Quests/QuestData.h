#pragma once

#include "CoreMinimal.h"
#include "QuestData.generated.h"

UENUM(BlueprintType)
enum class EQuestState : uint8
{
	NotStarted UMETA(DisplayName = "Not Started"),
	InProgress UMETA(DisplayName = "In Progress"),
	Completed  UMETA(DisplayName = "Completed"),
	Failed     UMETA(DisplayName = "Failed")
};

USTRUCT(BlueprintType)
struct FQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName ObjectiveID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 RequiredCount = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Quest")
	int32 CurrentCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Quest")
	bool bIsCompleted = false;
};

USTRUCT(BlueprintType)
struct FQuestInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName QuestID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FQuestObjective> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 RewardXP = 100;

	UPROPERTY(BlueprintReadWrite, Category = "Quest")
	EQuestState State = EQuestState::NotStarted;
};
