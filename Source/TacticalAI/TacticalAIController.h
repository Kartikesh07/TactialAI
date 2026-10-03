#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "TacticalSubsystem.h"
#include "TacticalAIController.generated.h"

class UBlackboardComponent;

UCLASS()
class TACTICALAI_API ATacticalAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATacticalAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION(BlueprintCallable, Category = "Tactical AI")
	void FreeReservedCover();

	// Blackboard Key Names
	static const FName TargetCoverLocationKey;
	static const FName ThreatActorKey;
	static const FName IsInCoverKey;
	static const FName IsLowCoverKey;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Tactical AI|Tree")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

private:
	FVector CurrentReservedCoverLocation = FVector::ZeroVector;
	bool bHasReservedCover = false;
};