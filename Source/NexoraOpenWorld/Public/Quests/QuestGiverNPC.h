#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/InteractableInterface.h"
#include "Quests/QuestData.h"
#include "QuestGiverNPC.generated.h"

UCLASS()
class NEXORAOPENWORLD_API AQuestGiverNPC : public ACharacter, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AQuestGiverNPC();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FQuestInfo QuestToGive;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
};
