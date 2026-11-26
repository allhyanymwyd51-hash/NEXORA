#include "Quests/QuestManagerComponent.h"

UQuestManagerComponent::UQuestManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UQuestManagerComponent::AcceptQuest(const FQuestInfo& NewQuest)
{
	for (const FQuestInfo& Existing : Quests)
	{
		if (Existing.QuestID == NewQuest.QuestID)
		{
			return false;
		}
	}

	FQuestInfo AddedQuest = NewQuest;
	AddedQuest.State = EQuestState::InProgress;
	Quests.Add(AddedQuest);
	OnQuestAccepted.Broadcast(AddedQuest);
	return true;
}

void UQuestManagerComponent::UpdateObjectiveProgress(FName QuestID, FName ObjectiveID, int32 Amount)
{
	for (FQuestInfo& Quest : Quests)
	{
		if (Quest.QuestID == QuestID && Quest.State == EQuestState::InProgress)
		{
			bool bAllCompleted = true;
			for (FQuestObjective& Obj : Quest.Objectives)
			{
				if (Obj.ObjectiveID == ObjectiveID && !Obj.bIsCompleted)
				{
					Obj.CurrentCount = FMath::Clamp(Obj.CurrentCount + Amount, 0, Obj.RequiredCount);
					if (Obj.CurrentCount >= Obj.RequiredCount)
					{
						Obj.bIsCompleted = true;
					}
				}

				if (!Obj.bIsCompleted)
				{
					bAllCompleted = false;
				}
			}

			OnQuestProgressed.Broadcast(Quest);

			if (bAllCompleted)
			{
				Quest.State = EQuestState::Completed;
				OnQuestCompleted.Broadcast(Quest);
			}
			return;
		}
	}
}

bool UQuestManagerComponent::IsQuestActive(FName QuestID) const
{
	for (const FQuestInfo& Quest : Quests)
	{
		if (Quest.QuestID == QuestID && Quest.State == EQuestState::InProgress)
		{
			return true;
		}
	}
	return false;
}

bool UQuestManagerComponent::IsQuestCompleted(FName QuestID) const
{
	for (const FQuestInfo& Quest : Quests)
	{
		if (Quest.QuestID == QuestID && Quest.State == EQuestState::Completed)
		{
			return true;
		}
	}
	return false;
}

TArray<FQuestInfo> UQuestManagerComponent::GetActiveQuests() const
{
	TArray<FQuestInfo> Result;
	for (const FQuestInfo& Quest : Quests)
	{
		if (Quest.State == EQuestState::InProgress)
		{
			Result.Add(Quest);
		}
	}
	return Result;
}
