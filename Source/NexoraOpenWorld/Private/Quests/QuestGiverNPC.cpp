#include "Quests/QuestGiverNPC.h"
#include "Character/NexoraCharacter.h"
#include "Quests/QuestManagerComponent.h"

AQuestGiverNPC::AQuestGiverNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	QuestToGive.QuestID = FName("Quest_ExploreTheValley");
	QuestToGive.Title = FText::FromString(TEXT("Explore the Ancient Valley"));
	QuestToGive.Description = FText::FromString(TEXT("Travel to the eastern ruins and uncover the lost relic."));
	QuestToGive.RewardXP = 250;

	FQuestObjective Obj;
	Obj.ObjectiveID = FName("Obj_ReachRuins");
	Obj.Description = FText::FromString(TEXT("Reach the Eastern Ruins"));
	Obj.RequiredCount = 1;
	QuestToGive.Objectives.Add(Obj);
}

void AQuestGiverNPC::Interact_Implementation(AActor* Interactor)
{
	if (ANexoraCharacter* Player = Cast<ANexoraCharacter>(Interactor))
	{
		if (Player->QuestManager)
		{
			Player->QuestManager->AcceptQuest(QuestToGive);
		}
	}
}

FText AQuestGiverNPC::GetInteractionPrompt_Implementation() const
{
	return FText::FromString(TEXT("Press E to Talk / Accept Quest"));
}
