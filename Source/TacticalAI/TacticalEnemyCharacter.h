#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TacticalEnemyCharacter.generated.h"

UCLASS()
class TACTICALAI_API ATacticalEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATacticalEnemyCharacter();

	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Stance")
	void SetInCoverStance(bool bInCover, bool bIsLowCover);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical AI|State")
	bool bHunkeredInCover = false;
};