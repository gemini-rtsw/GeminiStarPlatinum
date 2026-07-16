// Fill out your copyright notice in the Description page of Project Settings.


#include "ObservatoryCoordinator.h"
#include "Components/InstancedStaticMeshComponent.h"

void UObservatoryCoordinator::HandleFeedStatusChanged(EFeedStatus NewStatus)
{
	if (NewStatus == FeedStatus) return;
	FeedStatus = NewStatus;
	OnFeedStatusChanged.Broadcast(FeedStatus);
}

void UObservatoryCoordinator::HandleDataQualityChanged(bool NewStale, float NewAge)
{
	if (bDataStaleness == NewStale && DataAgeSeconds == NewAge) return;
	bDataStaleness = NewStale;
	DataAgeSeconds = NewAge;
	OnDataQualityChanged.Broadcast(NewStale, NewAge);
}

void UObservatoryCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Create the live data feed up front and give it access to the GameInstance so it can
	// resolve the telescope/dome model subsystems. Previously Feed was never assigned, so
	// switching to Live mode would null-deref.
	Feed = NewObject<ULiveDataFeed>(this);
	Feed->Initialize(GetGameInstance());
	Feed->OnStatusChanged.AddUObject(this, &UObservatoryCoordinator::HandleFeedStatusChanged);
	Feed->OnDataQualityChanged.AddUObject(this, &UObservatoryCoordinator::HandleDataQualityChanged);
}

void UObservatoryCoordinator::Deinitialize()
{
	if (Feed)
	{
		Feed->Disconnect();
		Feed = nullptr;
	}
	Super::Deinitialize();
}

void UObservatoryCoordinator::SetControlMode(EControlMode NewMode)
{
	if (NewMode == Mode) return;
	Mode = NewMode;
	if (Mode == EControlMode::Live) Feed->Connect();
	else                            Feed->Disconnect();
	OnControlModeChanged.Broadcast(Mode);
}

float UObservatoryCoordinator::GetTimeUntilReconnect() const
{
	return Feed ? Feed->GetTimeUntilReconnect() : 0.f;
}

int32 UObservatoryCoordinator::GetReconnectAttempts() const
{
	return Feed ? Feed->GetReconnectAttempts() : 0;
}

float UObservatoryCoordinator::MapAltToElevTarget(float AltTarget)
{
	// ElevTarget frame: 0 = zenith, -90 = horizon (see UTelescopeModel::SetElevTarget), so
	// altitude above the horizon maps as Alt - 90 (e.g. Alt 30 -> -60). No clamp here:
	// SetElevTarget clamps to the config-driven [ElevTwistMin, ElevTwistMax] limits.
	return AltTarget - 90.f + ElevZeroOffset;
}

void UObservatoryCoordinator::SlewToStar(const FString& Name)
{
	if (!CelestialVault.IsValid())
		CelestialVault = 
			Cast<ACelestialVaultDaySequenceActor>(
				UGameplayStatics::GetActorOfClass(GetGameInstance()->GetWorld(), 
				ACelestialVaultDaySequenceActor::StaticClass()));
	if (!Telescope.IsValid())
		Telescope = 
			Cast<AMovingTelescope>(
				UGameplayStatics::GetActorOfClass(GetGameInstance()->GetWorld(), 
				AMovingTelescope::StaticClass()));

	FStarInfo StarInfo;
	if (!FindStarByName(Name, StarInfo)) return;

	ToggleTracking(false);

	FTransform ISMInstanceTransform;
	CelestialVault->StarsComponent->GetInstanceTransform(StarInfo.ISMInstanceIndex, ISMInstanceTransform, true);

	FVector Dir = (ISMInstanceTransform.GetLocation() - Telescope->GetActorTransform().GetLocation()).GetSafeNormal();
	const FVector DirInBase = Telescope->GetActorTransform().InverseTransformVectorNoScale(Dir);

	float AzimDeg = FMath::RadiansToDegrees(FMath::Atan2(DirInBase.Y, DirInBase.X));
	float ElevDeg = FMath::RadiansToDegrees(FMath::Atan2(DirInBase.Z,
		FMath::Sqrt(DirInBase.X * DirInBase.X + DirInBase.Y * DirInBase.Y)));

	AzimDeg = AzimDeg + AzimZeroOffset;
	ElevDeg = MapAltToElevTarget(ElevDeg);

	if (ElevDeg > 0.f || ElevDeg < -90.f) return;

	if (auto* M = GetGameInstance()->GetSubsystem<UTelescopeModel>())
	{
		M->SetTargets(AzimDeg, ElevDeg, M->CassTarget);
	}
}

void UObservatoryCoordinator::TrackStar(const FString& Name)
{
	if (!CelestialVault.IsValid())
		CelestialVault =
		Cast<ACelestialVaultDaySequenceActor>(
			UGameplayStatics::GetActorOfClass(GetGameInstance()->GetWorld(),
				ACelestialVaultDaySequenceActor::StaticClass()));
	if (!Telescope.IsValid())
		Telescope =
		Cast<AMovingTelescope>(
			UGameplayStatics::GetActorOfClass(GetGameInstance()->GetWorld(),
				AMovingTelescope::StaticClass()));

	FStarInfo StarInfo;
	if (!FindStarByName(Name, StarInfo)) return;

	if (StarInfo.Name == TrackedStar.Name) return; // Early exit if the target star is the same as current star
	TrackedStar = StarInfo;

	if (bTracking)
		GetGameInstance()->GetTimerManager().ClearTimer(TrackingTimer);

	bTracking = true;
	GetGameInstance()->GetTimerManager().SetTimer(TrackingTimer, this, &UObservatoryCoordinator::SolveTrackingMovement, 0.5f, true);
}

void UObservatoryCoordinator::SolveTrackingMovement()
{
	if (!bTracking) return;

	if (!CelestialVault.IsValid())
		CelestialVault =
		Cast<ACelestialVaultDaySequenceActor>(
			UGameplayStatics::GetActorOfClass(GetGameInstance()->GetWorld(),
				ACelestialVaultDaySequenceActor::StaticClass()));
	if (!Telescope.IsValid())
		Telescope =
		Cast<AMovingTelescope>(
			UGameplayStatics::GetActorOfClass(GetGameInstance()->GetWorld(),
				AMovingTelescope::StaticClass()));

	FTransform ISMInstanceTransform;
	CelestialVault->StarsComponent->GetInstanceTransform(TrackedStar.ISMInstanceIndex, ISMInstanceTransform, true);

	FVector Dir = (ISMInstanceTransform.GetLocation() - Telescope->GetActorTransform().GetLocation()).GetSafeNormal();
	const FVector DirInBase = Telescope->GetActorTransform().InverseTransformVectorNoScale(Dir);

	float AzimDeg = FMath::RadiansToDegrees(FMath::Atan2(DirInBase.Y, DirInBase.X));
	float ElevDeg = FMath::RadiansToDegrees(FMath::Atan2(DirInBase.Z,
		FMath::Sqrt(DirInBase.X * DirInBase.X + DirInBase.Y * DirInBase.Y)));

	AzimDeg = AzimDeg + AzimZeroOffset;
	ElevDeg = MapAltToElevTarget(ElevDeg);

	if (ElevDeg > 0.f || ElevDeg < -90.f)
	{
		GetGameInstance()->GetTimerManager().ClearTimer(TrackingTimer);
		bTracking = false;
		return;
	}

	if (auto* M = GetGameInstance()->GetSubsystem<UTelescopeModel>())
	{
		M->SetTargets(AzimDeg, ElevDeg, M->CassTarget);
	}
}

void UObservatoryCoordinator::ToggleTracking(const bool NewState)
{
	if (bTracking == NewState) return;
	
	bTracking = NewState;
	if (!bTracking)
	{
		GetGameInstance()->GetTimerManager().ClearTimer(TrackingTimer);
		TrackedStar = FStarInfo();
	}
	else
	{
		GetGameInstance()->GetTimerManager().SetTimer(TrackingTimer, this, &UObservatoryCoordinator::SolveTrackingMovement, 0.5f, true);
	}
}

bool UObservatoryCoordinator::FindStarByName(const FString& Name, FStarInfo& Out) const
{
	if (!(CelestialVault.IsValid())) return false;
	if (!(CelestialVault->bKeepStarsInfo)) return false;

	const auto& StarsInfo = CelestialVault->StarsInfo;
	int32 StarIndex = StarsInfo.IndexOfByPredicate([Name](const FStarInfo& Star) {
		return Star.Name == Name;
		});

	if (StarIndex == INDEX_NONE)
		return false;

	Out = StarsInfo[StarIndex];
	return true;
}

