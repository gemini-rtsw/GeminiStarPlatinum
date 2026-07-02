// Fill out your copyright notice in the Description page of Project Settings.


#include "DomeModel.h"

void UDomeModel::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Load motion limit settings from the config file
	MotionLimitSettings = GetDefault<UMotionLimitSettings>();
    if (MotionLimitSettings)
    {
		DomeTwistMin       = MotionLimitSettings->DomeTwistMin;
		DomeTwistMax       = MotionLimitSettings->DomeTwistMax;
		TopShutterSwingMin = MotionLimitSettings->TopShutterSwingMin;
		TopShutterSwingMax = MotionLimitSettings->TopShutterSwingMax;
		BotShutterSwingMin = MotionLimitSettings->BotShutterSwingMin;
		BotShutterSwingMax = MotionLimitSettings->BotShutterSwingMax;
		VentSlideMin       = MotionLimitSettings->VentSlideMin;
		VentSlideMax       = MotionLimitSettings->VentSlideMax;
    }
}

void UDomeModel::SetDomeTwistTarget(float Degrees, bool BroadcastFlag)
{
	auto temp = FMath::Clamp(
        FMath::UnwindDegrees(Degrees), DomeTwistMin, DomeTwistMax);
	if (temp == DomeTwistTarget || !FMath::IsFinite(temp)) return; // No change, do nothing. 
																   // Notably, prevents broadcasting regardless of bDirty.
	
	DomeTwistTarget = temp;
	bDirty = true;
	
	if (!BroadcastFlag) return;

	OnStateChanged.Broadcast();
	bDirty = false;                      // Reset dirty flag after broadcasting, since the change has been communicated.
}

void UDomeModel::SetTopShutterTarget(float Degrees, bool BroadcastFlag)
{
	auto temp = FMath::Clamp(
        FMath::UnwindDegrees(Degrees), TopShutterSwingMin, TopShutterSwingMax);
	if (temp == TopSSwingTarget || !FMath::IsFinite(temp)) return;
	
	TopSSwingTarget = temp;
	bDirty = true;
	
	if (!BroadcastFlag) return;
	
	OnStateChanged.Broadcast();
	bDirty = false;
}

void UDomeModel::SetBotShutterTarget(float Degrees, bool BroadcastFlag)
{
	auto temp = FMath::Clamp(
        FMath::UnwindDegrees(Degrees), BotShutterSwingMin, BotShutterSwingMax);
	if (temp == BotSSwingTarget || !FMath::IsFinite(temp)) return;
	
	BotSSwingTarget = temp;
	bDirty = true;
	
	if (!BroadcastFlag) return;
	
	OnStateChanged.Broadcast();
	bDirty = false;
}

void UDomeModel::SetVentTarget(float SlideAmount, bool BroadcastFlag)
{
	auto temp = FMath::Clamp(SlideAmount, VentSlideMin, VentSlideMax);
	if (temp == VentSlideTarget || !FMath::IsFinite(temp)) return;
	
	VentSlideTarget = temp;
	bDirty = true;
	
	if (!BroadcastFlag) return;
	
	OnStateChanged.Broadcast();
	bDirty = false;
}

void UDomeModel::SetTargets(float DomeTwist, float TopSSwing, float BotSSwing, float VentSlide)
{
	// Set all targets at once, clamping to the limits defined in MotionLimitSettings
	// Does not broadcast until all targets are set, and only broadcasts if any target changed
	SetDomeTwistTarget(DomeTwist, false);
	SetTopShutterTarget(TopSSwing, false);
	SetBotShutterTarget(BotSSwing, false);
	SetVentTarget(VentSlide, false);
	if (!bDirty) return;
	
	OnStateChanged.Broadcast();
	bDirty = false;
}

void UDomeModel::SetOpen(bool bNewOpen)
{
	if (bOpen == bNewOpen) return; // No change, do nothing
	bOpen = bNewOpen;
    // Update targets based on whether the dome is open or closed
    if (bOpen)
    {
        SetTopShutterTarget(83.f, false); // Yea the targets are calculated from Logan's numbers, but I don't remember how I got them or where the orignal numbers are
        SetBotShutterTarget(-13.f, false);
        SetVentTarget(500.f, false);
    }
    else
    {
        SetTopShutterTarget(-7.f, false);
        SetBotShutterTarget(-3.5f, false);
        SetVentTarget(0.f, false);
    }
    OnStateChanged.Broadcast();
}