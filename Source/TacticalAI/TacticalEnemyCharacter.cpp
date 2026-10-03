#include "TacticalEnemyCharacter.h"
#include "TacticalAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

ATacticalEnemyCharacter::ATacticalEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 200.0f;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	GetCharacterMovement()->bRequestedMoveUseAcceleration = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	AIControllerClass = ATacticalAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ATacticalEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// FIX: Forces acceleration on at runtime regardless of CDO serialization in Blueprint assets
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bRequestedMoveUseAcceleration = true;
	}
}

void ATacticalEnemyCharacter::SetInCoverStance(bool bInCover, bool bIsLowCover)
{
	bHunkeredInCover = bInCover;

	if (bInCover && bIsLowCover)
	{
		Crouch();
	}
	else
	{
		UnCrouch();
	}
}