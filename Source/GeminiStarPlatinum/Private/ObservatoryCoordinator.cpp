// Fill out your copyright notice in the Description page of Project Settings.


#include "ObservatoryCoordinator.h"

void UObservatoryCoordinator::SetControlMode(EControlMode NewMode)
{
	if (NewMode == Mode) return;
	Mode = NewMode;
	if (Mode == EControlMode::Live) Feed->Connect();
	else                            Feed->Disconnect();
	OnControlModeChanged.Broadcast(Mode);
}