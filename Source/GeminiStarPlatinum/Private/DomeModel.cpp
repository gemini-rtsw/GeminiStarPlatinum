// Fill out your copyright notice in the Description page of Project Settings.


#include "DomeModel.h"

void UDomeModel::SetDomeTwistTarget(float Degrees)
{
	DomeTwistTarget = FMath::Clamp(FMath::UnwindDegrees(Degrees), TwistMin, TwistMax);
	OnStateChanged.Broadcast();
}

void UDomeModel::SetOpen(bool bNewOpen)
{
	bOpen = bNewOpen;
    // Update targets based on whether the dome is open or closed
    if (bOpen)
    {
        TopSSwingTarget = 83.f; // Yea the targets are calculated from Logan's numbers, but I don't remember how I got them or where the orignal numbers are
        BotSSwingTarget = -13.f;
        VentSlideTarget = 500.f;
    }
    else
    {
        TopSSwingTarget = -7.f;
        BotSSwingTarget = 3.5f;
        VentSlideTarget = 0.f;
    }
    OnStateChanged.Broadcast();
}