#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "TacticalSubsystem.generated.h"

UENUM(BlueprintType)
enum class ECoverType : uint8
{
	None,
	LowCover,   // Crouch cover (~65cm to 100cm tall)
	HighCover   // Full standing cover (>130cm tall)
};

USTRUCT(BlueprintType)
struct FCoverPoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical AI|Cover")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical AI|Cover")
	FVector WallNormal = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical AI|Cover")
	ECoverType CoverType = ECoverType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical AI|Cover")
	bool bIsOccupied = false;

	TWeakObjectPtr<AActor> ReservedBy = nullptr;
};

USTRUCT(BlueprintType)
struct FTacticalCell
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical Grid")
	float Threat = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical Grid")
	float FriendlyDensity = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical Grid")
	float FlankPotential = 0.0f;
};

UCLASS()
class TACTICALAI_API UTacticalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UTacticalSubsystem();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	bool WorldToGrid(const FVector& WorldLocation, int32& OutX, int32& OutY) const;
	FVector GridToWorld(int32 GridX, int32 GridY) const;
	FORCEINLINE int32 GetCellIndex(int32 GridX, int32 GridY) const { return (GridY * GridWidth) + GridX; }
	FORCEINLINE bool IsValidGridIndex(int32 GridX, int32 GridY) const
	{
		return GridX >= 0 && GridX < GridWidth && GridY >= 0 && GridY < GridHeight;
	}

	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Queries")
	float GetThreatAtLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Queries")
	float GetFlankScoreAtLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Cover")
	const TArray<FCoverPoint>& GetCoverPoints() const { return CoverPoints; }

	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Cover")
	void GenerateCoverPoints();

	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Cover")
	bool FindBestCover(
		const FVector& QuerierLocation,
		const FVector& ThreatLocation,
		bool bPreferFlank,
		FCoverPoint& OutBestCover
	);

	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Cover")
	bool ReserveCover(const FVector& CoverLocation, AActor* ClaimingActor);

	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Cover")
	void ReleaseCover(AActor* ClaimingActor);

protected:
	void UpdateGrid();
	void DrawDebugGrid();
	void DrawDebugCover();

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Grid Config")
	float CellSize = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Grid Config")
	int32 GridWidth = 60;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Grid Config")
	int32 GridHeight = 60;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Grid Config")
	FVector2D GridOrigin = FVector2D(-6000.0f, -6000.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Grid Config")
	float UpdateInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Grid Config")
	bool bEnableDebugDraw = true;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Grid Config")
	float PlayerThreatRadius = 2500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Cover Config")
	float MinCoverDistance = 140.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Cover Config")
	float WallOffsetDistance = 55.0f;

private:
	TArray<FTacticalCell> Cells;
	TArray<FCoverPoint> CoverPoints;
	FTimerHandle GridUpdateTimerHandle;

	FCoverPoint BestDebugCover;
	bool bHasValidDebugCover = false;
	int32 LastSelectedCoverIndex = INDEX_NONE;
};