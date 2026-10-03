#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindAndReserveCover.generated.h"

UCLASS()
class TACTICALAI_API UBTTask_FindAndReserveCover : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindAndReserveCover();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	/** If true, queries high-scoring flank/blindspot vectors; if false, prioritizes maximum occlusion defense */
	UPROPERTY(EditAnywhere, Category = "Tactical AI")
	bool bPreferFlank = false;
};