#include "BTTask_SetCoverStance.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "TacticalAIController.h"
#include "TacticalEnemyCharacter.h"

UBTTask_SetCoverStance::UBTTask_SetCoverStance()
{
	NodeName = "Set Cover Stance";
}

EBTNodeResult::Type UBTTask_SetCoverStance::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ATacticalAIController* AICon = Cast<ATacticalAIController>(OwnerComp.GetAIOwner());
	if (!AICon)
	{
		return EBTNodeResult::Failed;
	}

	ATacticalEnemyCharacter* EnemyChar = Cast<ATacticalEnemyCharacter>(AICon->GetPawn());
	if (!EnemyChar)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	const bool bIsLowCover = BlackboardComp->GetValueAsBool(ATacticalAIController::IsLowCoverKey);

	EnemyChar->SetInCoverStance(bEnteringCover, bIsLowCover);
	BlackboardComp->SetValueAsBool(ATacticalAIController::IsInCoverKey, bEnteringCover);

	return EBTNodeResult::Succeeded;
}