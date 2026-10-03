#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetCoverStance.generated.h"

UCLASS()
class TACTICALAI_API UBTTask_SetCoverStance : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetCoverStance();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Tactical AI")
	bool bEnteringCover = true;
};