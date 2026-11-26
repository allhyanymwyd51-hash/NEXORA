#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/QuestData.h"
#include "QuestManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestUpdated, const FQuestInfo&, QuestInfo);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NEXORAOPENWORLD_API UQuestManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestManagerComponent();

	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool AcceptQuest(const FQuestInfo& NewQuest);

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void UpdateObjectiveProgress(FName QuestID, FName ObjectiveID, int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool IsQuestActive(FName QuestID) const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool IsQuestCompleted(FName QuestID) const;

	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<FQuestInfo> GetActiveQuests() const;

	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestUpdated OnQuestAccepted;

	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestUpdated OnQuestProgressed;

	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestUpdated OnQuestCompleted;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quests")
	TArray<FQuestInfo> Quests;
};
