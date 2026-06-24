// Fill out your copyright notice in the Description page of Project Settings.


#include "DomeModel.h"

void UDomeModel::SetDomeTwistTarget(float Degrees)
{
	DomeTwistTarget = FMath::Clamp(FMath::UnwindDegrees(Degrees), TwistMin, TwistMax);
	OnStateChanged.Broadcast();
}

void UDomeModel::SetTopShutterTarget(float Degrees)
{
	TopSSwingTarget = Degrees;
	OnStateChanged.Broadcast();
}

void UDomeModel::SetBotShutterTarget(float Degrees)
{
	BotSSwingTarget = Degrees;
	OnStateChanged.Broadcast();
}

void UDomeModel::SetVentTarget(float SlideAmount)
{
	VentSlideTarget = SlideAmount;
	OnStateChanged.Broadcast();
}

void UDomeModel::SetOpen(bool bNewOpen)
{
	bOpen = bNewOpen;
    // Update targets based on whether the dome is open or closed
    if (bOpen)
    {
        SetTopShutterTarget(83.f); // Yea the targets are calculated from Logan's numbers, but I don't remember how I got them or where the orignal numbers are
        SetBotShutterTarget(-13.f);
        SetVentTarget(500.f);
    }
    else
    {
        SetTopShutterTarget(-7.f);
        SetBotShutterTarget(-3.5f);
        SetVentTarget(0.f);
    }
    OnStateChanged.Broadcast();
}