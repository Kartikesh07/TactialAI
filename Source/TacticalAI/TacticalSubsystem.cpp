#include "TacticalSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "CollisionQueryParams.h"
#include "EngineUtils.h"

UTacticalSubsystem::UTacticalSubsystem()
{}

void UTacticalSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	Cells.Init(FTacticalCell(), GridWidth * GridHeight);

	GenerateCoverPoints();

	InWorld.GetTimerManager().SetTimer(
		GridUpdateTimerHandle,
		this,
		&UTacticalSubsystem::UpdateGrid,
		UpdateInterval,
		true
	);
}

void UTacticalSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GridUpdateTimerHandle);
	}
	Cells.Empty();
	CoverPoints.Empty();

	Super::Deinitialize();
}

bool UTacticalSubsystem::WorldToGrid(const FVector& WorldLocation, int32& OutX, int32& OutY) const
{
	const float NormalizedX = (WorldLocation.X - GridOrigin.X) / CellSize;
	const float NormalizedY = (WorldLocation.Y - GridOrigin.Y) / CellSize;

	OutX = FMath::FloorToInt(NormalizedX);
	OutY = FMath::FloorToInt(NormalizedY);

	return IsValidGridIndex(OutX, OutY);
}

FVector UTacticalSubsystem::GridToWorld(int32 GridX, int32 GridY) const
{
	const float WorldX = GridOrigin.X + (GridX * CellSize) + (CellSize * 0.5f);
	const float WorldY = GridOrigin.Y + (GridY * CellSize) + (CellSize * 0.5f);
	return FVector(WorldX, WorldY, 0.0f);
}

float UTacticalSubsystem::GetThreatAtLocation(const FVector& WorldLocation) const
{
	int32 GridX, GridY;
	if (WorldToGrid(WorldLocation, GridX, GridY))
	{
		return Cells[GetCellIndex(GridX, GridY)].Threat;
	}
	return 0.0f;
}

float UTacticalSubsystem::GetFlankScoreAtLocation(const FVector& WorldLocation) const
{
	int32 GridX, GridY;
	if (WorldToGrid(WorldLocation, GridX, GridY))
	{
		return Cells[GetCellIndex(GridX, GridY)].FlankPotential;
	}
	return 0.0f;
}

void UTacticalSubsystem::GenerateCoverPoints()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return;
	}

	CoverPoints.Empty();

	const FVector TraceDirections[4] = {
		FVector(1.0f, 0.0f, 0.0f),
		FVector(-1.0f, 0.0f, 0.0f),
		FVector(0.0f, 1.0f, 0.0f),
		FVector(0.0f, -1.0f, 0.0f)
	};

	const float TraceDistance = CellSize * 1.2f;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;

	// Use TActorIterator to ignore all live Pawns so characters never register as walls
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (APawn* P = *It)
		{
			QueryParams.AddIgnoredActor(P);
		}
	}

	const FCollisionShape CharacterCapsule = FCollisionShape::MakeCapsule(40.0f, 88.0f);

	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const FVector ProbeLocation = GridToWorld(X, Y);

			FNavLocation NavLocation;
			const bool bOnNavMesh = NavSys->ProjectPointToNavigation(
				ProbeLocation,
				NavLocation,
				FVector(30.0f, 30.0f, 50.0f)
			);

			if (!bOnNavMesh)
			{
				continue;
			}

			const FVector BasePos = NavLocation.Location;

			for (const FVector& Direction : TraceDirections)
			{
				const FVector LowStart = BasePos + FVector(0.0f, 0.0f, 50.0f);
				const FVector LowEnd = LowStart + (Direction * TraceDistance);

				FHitResult LowHit;
				const bool bHitLow = World->LineTraceSingleByChannel(
					LowHit,
					LowStart,
					LowEnd,
					ECC_WorldStatic,
					QueryParams
				);

				if (!bHitLow || !LowHit.bBlockingHit || !LowHit.GetActor() || LowHit.GetActor()->IsA<APawn>())
				{
					continue;
				}

				// Vertical wall filter (excludes ramps)
				if (FMath::Abs(LowHit.ImpactNormal.Z) > 0.08f)
				{
					continue;
				}

				// Minimum barrier height check (65cm)
				const FVector MidStart = BasePos + FVector(0.0f, 0.0f, 65.0f);
				const FVector MidEnd = MidStart + (Direction * TraceDistance);
				FHitResult MidHit;
				const bool bHitMid = World->LineTraceSingleByChannel(MidHit, MidStart, MidEnd, ECC_WorldStatic, QueryParams);
				if (!bHitMid || !MidHit.bBlockingHit || (MidHit.GetActor() && MidHit.GetActor()->IsA<APawn>()))
				{
					continue;
				}

				const FVector CandidatePos = LowHit.ImpactPoint + (LowHit.ImpactNormal * WallOffsetDistance);

				FNavLocation CoverNavLoc;
				const bool bCoverReachable = NavSys->ProjectPointToNavigation(
					CandidatePos,
					CoverNavLoc,
					FVector(30.0f, 30.0f, 60.0f)
				);

				if (!bCoverReachable)
				{
					continue;
				}

				// Capsule clearance test
				const FVector CapsuleCenter = CoverNavLoc.Location + FVector(0.0f, 0.0f, 90.0f);
				const bool bCapsuleBlocked = World->OverlapBlockingTestByChannel(
					CapsuleCenter,
					FQuat::Identity,
					ECC_WorldStatic,
					CharacterCapsule,
					QueryParams
				);

				if (bCapsuleBlocked)
				{
					continue;
				}

				// Spacing check
				bool bTooClose = false;
				for (const FCoverPoint& ExistingPoint : CoverPoints)
				{
					if (FVector::DistSquared2D(ExistingPoint.Location, CoverNavLoc.Location) < FMath::Square(MinCoverDistance))
					{
						bTooClose = true;
						break;
					}
				}

				if (bTooClose)
				{
					continue;
				}

				// High trace (130cm) for cover height classification
				const FVector HighStart = CoverNavLoc.Location + FVector(0.0f, 0.0f, 130.0f);
				const FVector HighEnd = HighStart - (LowHit.ImpactNormal * (WallOffsetDistance + 40.0f));

				FHitResult HighHit;
				const bool bHitHigh = World->LineTraceSingleByChannel(
					HighHit,
					HighStart,
					HighEnd,
					ECC_WorldStatic,
					QueryParams
				);

				const bool bIsHighCover = (bHitHigh && HighHit.bBlockingHit && (!HighHit.GetActor() || !HighHit.GetActor()->IsA<APawn>()));

				FCoverPoint NewCover;
				NewCover.Location = CoverNavLoc.Location;
				NewCover.WallNormal = LowHit.ImpactNormal;
				NewCover.CoverType = bIsHighCover ? ECoverType::HighCover : ECoverType::LowCover;
				NewCover.bIsOccupied = false;

				CoverPoints.Add(NewCover);
			}
		}
	}
}

bool UTacticalSubsystem::FindBestCover(
	const FVector& QuerierLocation,
	const FVector& ThreatLocation,
	bool bPreferFlank,
	FCoverPoint& OutBestCover)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfEmpty::DontCreate);
	if (!NavData)
	{
		return false;
	}

	FNavLocation ProjectedQuerier;
	if (!NavSys->ProjectPointToNavigation(QuerierLocation, ProjectedQuerier, FVector(100.0f, 100.0f, 200.0f)))
	{
		return false;
	}

	float BestScore = -MAX_flt;
	int32 BestIndex = INDEX_NONE;

	const float MaxSearchDist = 2500.0f;
	const float CurrentDistToThreat = FVector::Dist2D(ProjectedQuerier.Location, ThreatLocation);

	// Safe combat standoff distance (7.5 meters)
	const float ThreatDangerRadius = 750.0f;

	FCollisionQueryParams LoSParams;
	LoSParams.bTraceComplex = false;

	for (int32 i = 0; i < CoverPoints.Num(); ++i)
	{
		const FCoverPoint& Cover = CoverPoints[i];

		if (Cover.bIsOccupied)
		{
			continue;
		}

		// 1. Distance filter
		const float DistToCover = FVector::Dist2D(ProjectedQuerier.Location, Cover.Location);
		if (DistToCover > MaxSearchDist)
		{
			continue;
		}

		// 2. Directional check: wall normal must face away from threat
		const FVector DirFromCoverToThreat = (ThreatLocation - Cover.Location).GetSafeNormal2D();
		const float WallDot = FVector::DotProduct(Cover.WallNormal, DirFromCoverToThreat);
		if (WallDot > -0.05f)
		{
			continue;
		}

		// 3. Occlusion raycast test
		const float EyeHeight = (Cover.CoverType == ECoverType::LowCover) ? 50.0f : 120.0f;
		const FVector CoverEyePos = Cover.Location + FVector(0.0f, 0.0f, EyeHeight);
		const FVector ThreatEyePos = ThreatLocation + FVector(0.0f, 0.0f, 60.0f);

		FHitResult LoSHit;
		const bool bBlocked = World->LineTraceSingleByChannel(
			LoSHit,
			CoverEyePos,
			ThreatEyePos,
			ECC_WorldStatic,
			LoSParams
		);

		if (!bBlocked)
		{
			continue;
		}

		// 4. PATH TRAVERSAL & CONTINUOUS SEGMENT INTERSECTION TEST
		FPathFindingQuery Query(nullptr, *NavData, ProjectedQuerier.Location, Cover.Location);
		FPathFindingResult PathResult = NavSys->FindPathSync(Query);

		if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid())
		{
			continue;
		}

		const TArray<FNavPathPoint>& PathPoints = PathResult.Path->GetPathPoints();
		bool bRouteEntersDangerZone = false;

		// Evaluate every continuous line segment along the route
		for (int32 PointIdx = 0; PointIdx < PathPoints.Num() - 1; ++PointIdx)
		{
			const FVector SegStart = PathPoints[PointIdx].Location;
			const FVector SegEnd = PathPoints[PointIdx + 1].Location;

			// Computes the shortest 3D distance from the Threat to this path segment
			const float DistToSegment = FMath::PointDistToSegment(ThreatLocation, SegStart, SegEnd);

			// RULE A: Outside danger bubble -> entirely forbidden from entering it
			if (CurrentDistToThreat >= ThreatDangerRadius)
			{
				if (DistToSegment < ThreatDangerRadius)
				{
					bRouteEntersDangerZone = true;
					break;
				}
			}
			// RULE B: Already inside danger bubble -> forbidden from moving closer than starting position
			else
			{
				if (DistToSegment < (CurrentDistToThreat - 50.0f))
				{
					bRouteEntersDangerZone = true;
					break;
				}
			}
		}

		if (bRouteEntersDangerZone)
		{
			continue;
		}

		// 5. Composite Scoring
		const float OcclusionQuality = -WallDot;
		const float NormalizedDist = DistToCover / MaxSearchDist;
		const float CellFlank = GetFlankScoreAtLocation(Cover.Location);

		float Score = 0.0f;
		if (bPreferFlank)
		{
			Score = (OcclusionQuality * 2.5f) - (NormalizedDist * 2.0f) + (CellFlank * 3.0f);
		}
		else
		{
			Score = (OcclusionQuality * 4.0f) - (NormalizedDist * 2.5f);
		}

		if (i == LastSelectedCoverIndex)
		{
			Score += 1.5f;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = i;
		}
	}

	if (BestIndex != INDEX_NONE)
	{
		LastSelectedCoverIndex = BestIndex;
		OutBestCover = CoverPoints[BestIndex];
		return true;
	}

	return false;
}

bool UTacticalSubsystem::ReserveCover(const FVector& CoverLocation, AActor* ClaimingActor)
{
	if (!ClaimingActor)
	{
		return false;
	}

	for (FCoverPoint& Cover : CoverPoints)
	{
		if (FVector::DistSquared2D(Cover.Location, CoverLocation) < FMath::Square(50.0f))
		{
			if (Cover.bIsOccupied)
			{
				return false;
			}

			Cover.bIsOccupied = true;
			Cover.ReservedBy = ClaimingActor;
			return true;
		}
	}
	return false;
}

void UTacticalSubsystem::ReleaseCover(AActor* ClaimingActor)
{
	if (!ClaimingActor)
	{
		return;
	}

	for (FCoverPoint& Cover : CoverPoints)
	{
		if (Cover.ReservedBy == ClaimingActor)
		{
			Cover.bIsOccupied = false;
			Cover.ReservedBy = nullptr;
		}
	}
}

void UTacticalSubsystem::UpdateGrid()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const FVector PlayerForward = PlayerPawn->GetActorForwardVector();

	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const int32 CellIdx = GetCellIndex(X, Y);
			FVector CellWorldPos = GridToWorld(X, Y);
			CellWorldPos.Z = PlayerLocation.Z;

			const float DistToPlayer = FVector::Dist2D(CellWorldPos, PlayerLocation);

			// Threat Layer
			float ThreatFactor = 0.0f;
			if (DistToPlayer < PlayerThreatRadius)
			{
				ThreatFactor = 1.0f - (DistToPlayer / PlayerThreatRadius);
			}
			Cells[CellIdx].Threat = ThreatFactor;

			// Flank Potential Layer
			FVector DirToCell = (CellWorldPos - PlayerLocation).GetSafeNormal2D();
			float ForwardDot = FVector::DotProduct(PlayerForward, DirToCell);

			float FlankScore = 0.0f;
			if (DistToPlayer >= 800.0f && DistToPlayer <= 2200.0f)
			{
				if (ForwardDot < 0.2f)
				{
					FlankScore = FMath::Clamp((0.2f - ForwardDot) / 1.2f, 0.0f, 1.0f);
				}
			}
			Cells[CellIdx].FlankPotential = FlankScore;
		}
	}

	bHasValidDebugCover = FindBestCover(
		PlayerLocation,
		PlayerLocation,
		false,
		BestDebugCover
	);

	if (bEnableDebugDraw)
	{
		DrawDebugGrid();
		DrawDebugCover();
	}
}

void UTacticalSubsystem::DrawDebugGrid()
{
	UWorld* World = GetWorld();
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		return;
	}

	const float GroundZ = PlayerPawn->GetActorLocation().Z - 90.0f;
	const FLinearColor CyanColor(0.0f, 1.0f, 1.0f, 1.0f);

	for (int32 Y = 0; Y < GridHeight; Y += 2)
	{
		for (int32 X = 0; X < GridWidth; X += 2)
		{
			const int32 CellIdx = GetCellIndex(X, Y);
			const FTacticalCell& Cell = Cells[CellIdx];

			if (Cell.Threat < 0.05f && Cell.FlankPotential < 0.05f)
			{
				continue;
			}

			FVector Center = GridToWorld(X, Y);
			Center.Z = GroundZ + 2.0f;

			FColor CellColor = FColor::White;
			if (Cell.FlankPotential > 0.3f)
			{
				CellColor = FLinearColor::LerpUsingHSV(FLinearColor::Black, CyanColor, Cell.FlankPotential).ToFColor(true);
			}
			else if (Cell.Threat > 0.05f)
			{
				CellColor = FLinearColor::LerpUsingHSV(FLinearColor::Black, FLinearColor::Red, Cell.Threat).ToFColor(true);
			}

			DrawDebugBox(
				World,
				Center,
				FVector(CellSize * 0.45f, CellSize * 0.45f, 2.0f),
				CellColor,
				false,
				UpdateInterval,
				0,
				2.0f
			);
		}
	}
}

void UTacticalSubsystem::DrawDebugCover()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FCoverPoint& Cover : CoverPoints)
	{
		FColor CoverColor = (Cover.CoverType == ECoverType::HighCover) ? FColor::Blue : FColor::Green;
		if (Cover.bIsOccupied)
		{
			CoverColor = FColor::Orange;
		}

		const float CylinderHeight = (Cover.CoverType == ECoverType::HighCover) ? 100.0f : 50.0f;

		DrawDebugCylinder(
			World,
			Cover.Location,
			Cover.Location + FVector(0.0f, 0.0f, CylinderHeight),
			25.0f,
			12,
			CoverColor,
			false,
			UpdateInterval,
			0,
			2.0f
		);

		DrawDebugDirectionalArrow(
			World,
			Cover.Location + FVector(0.0f, 0.0f, 30.0f),
			Cover.Location + FVector(0.0f, 0.0f, 30.0f) + (Cover.WallNormal * 60.0f),
			20.0f,
			FColor::Yellow,
			false,
			UpdateInterval,
			0,
			2.5f
		);
	}

	if (bHasValidDebugCover)
	{
		DrawDebugSphere(
			World,
			BestDebugCover.Location + FVector(0.0f, 0.0f, 70.0f),
			45.0f,
			16,
			FColor::Emerald,
			false,
			UpdateInterval,
			0,
			3.0f
		);

		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
		if (PlayerPawn)
		{
			DrawDebugLine(
				World,
				PlayerPawn->GetActorLocation(),
				BestDebugCover.Location + FVector(0.0f, 0.0f, 70.0f),
				FColor::Emerald,
				false,
				UpdateInterval,
				0,
				2.0f
			);
		}
	}
}