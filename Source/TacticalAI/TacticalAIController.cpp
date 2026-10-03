#include "TacticalAIController.h"
#include "Engine/World.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

const FName ATacticalAIController::TargetCoverLocationKey = TEXT("TargetCoverLocation");
const FName ATacticalAIController::ThreatActorKey = TEXT("ThreatActor");
const FName ATacticalAIController::IsInCoverKey = TEXT("IsInCover");
const FName ATacticalAIController::IsLowCoverKey = TEXT("IsLowCover");

ATacticalAIController::ATacticalAIController()
{
	bWantsPlayerState = false;
}

void ATacticalAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);

		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (PlayerPawn && GetBlackboardComponent())
		{
			GetBlackboardComponent()->SetValueAsObject(ThreatActorKey, PlayerPawn);
		}
	}
}

void ATacticalAIController::OnUnPossess()
{
	FreeReservedCover();
	Super::OnUnPossess();
}

void ATacticalAIController::FreeReservedCover()
{
	if (APawn* MyPawn = GetPawn())
	{
		if (UWorld* World = GetWorld())
		{
			if (UTacticalSubsystem* TacticalSubsystem = World->GetSubsystem<UTacticalSubsystem>())
			{
				TacticalSubsystem->ReleaseCover(MyPawn);
			}
		}
	}
	bHasReservedCover = false;
	CurrentReservedCoverLocation = FVector::ZeroVector;
}