#include "BTTask_FindAndReserveCover.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "TacticalAIController.h"
#include "TacticalEnemyCharacter.h"
#include "TacticalSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UBTTask_FindAndReserveCover::UBTTask_FindAndReserveCover()
{
	NodeName = "Find And Reserve Cover";
}

EBTNodeResult::Type UBTTask_FindAndReserveCover::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ATacticalAIController* AICon = Cast<ATacticalAIController>(OwnerComp.GetAIOwner());
	if (!AICon)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[TacticalAI] Failed: No AIController"));
		return EBTNodeResult::Failed;
	}

	APawn* AIPawn = AICon->GetPawn();
	if (!AIPawn)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[TacticalAI] Failed: No BlackboardComponent"));
		return EBTNodeResult::Failed;
	}

	UWorld* World = AIPawn->GetWorld();
	if (!World)
	{
		return EBTNodeResult::Failed;
	}

	// RACE CONDITION FIX:
	// If ThreatActor wasn't populated during OnPossess, fetch and cache it now
	AActor* ThreatActor = Cast<AActor>(BlackboardComp->GetValueAsObject(ATacticalAIController::ThreatActorKey));
	if (!ThreatActor)
	{
		ThreatActor = UGameplayStatics::GetPlayerPawn(World, 0);
		if (ThreatActor)
		{
			BlackboardComp->SetValueAsObject(ATacticalAIController::ThreatActorKey, ThreatActor);
		}
		else
		{
			// Player hasn't finished spawning yet; retry next tick
			return EBTNodeResult::Failed;
		}
	}

	UTacticalSubsystem* TacticalSubsystem = World->GetSubsystem<UTacticalSubsystem>();
	if (!TacticalSubsystem)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[TacticalAI] Failed: TacticalSubsystem missing"));
		return EBTNodeResult::Failed;
	}

	// Release any previous reservation
	TacticalSubsystem->ReleaseCover(AIPawn);

	FCoverPoint BestCover;
	const bool bFound = TacticalSubsystem->FindBestCover(
		AIPawn->GetActorLocation(),
		ThreatActor->GetActorLocation(),
		bPreferFlank,
		BestCover
	);

	if (!bFound)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, TEXT("[TacticalAI] No valid cover reachable"));
		return EBTNodeResult::Failed;
	}

	// Reserve slot
	if (!TacticalSubsystem->ReserveCover(BestCover.Location, AIPawn))
	{
		return EBTNodeResult::Failed;
	}

	// Write coordinates and stance flags
	BlackboardComp->SetValueAsVector(ATacticalAIController::TargetCoverLocationKey, BestCover.Location);
	BlackboardComp->SetValueAsBool(ATacticalAIController::IsLowCoverKey, BestCover.CoverType == ECoverType::LowCover);
	BlackboardComp->SetValueAsBool(ATacticalAIController::IsInCoverKey, false);

	return EBTNodeResult::Succeeded;
}