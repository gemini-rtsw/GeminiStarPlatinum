// Fill out your copyright notice in the Description page of Project Settings.


#include "ObservatoryCoordinator.h"

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
