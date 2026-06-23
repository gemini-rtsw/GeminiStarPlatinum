// Fill out your copyright notice in the Description page of Project Settings.


#include "ObservatoryCoordinator.h"

void UObservatoryCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Create the live data feed up front and give it access to the GameInstance so it can
	// resolve the telescope/dome model subsystems. Previously Feed was never assigned, so
	// switching to Live mode would null-deref.
	Feed = NewObject<ULiveDataFeed>(this);
	Feed->Initialize(GetGameInstance());
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
