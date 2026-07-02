// Fill out your copyright notice in the Description page of Project Settings.


#include "TelescopeModel.h"

void UTelescopeModel::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Load motion limit settings from the config file
	MotionLimitSettings = GetDefault<UMotionLimitSettings>();
	if (MotionLimitSettings)
	{
		AzimTwistMin = MotionLimitSettings->AzimTwistMin;
		AzimTwistMax = MotionLimitSettings->AzimTwistMax;
		ElevTwistMin = MotionLimitSettings->ElevTwistMin;
		ElevTwistMax = MotionLimitSettings->ElevTwistMax;
		CassTwistMin = MotionLimitSettings->CassTwistMin;
		CassTwistMax = MotionLimitSettings->CassTwistMax;
	}
}

void UTelescopeModel::SetAzimTarget(float Degrees, bool BroadcastFlag)
{
	auto temp = FMath::Clamp(FMath::UnwindDegrees(Degrees), AzimTwistMin, AzimTwistMax);
	if (temp == AzimTarget || !FMath::IsFinite(temp)) return; // No change, do nothing. 
	                                // Notably, prevents broadcasting regardless of bDirty state.

	AzimTarget = temp;
	bDirty = true;

	if (!BroadcastFlag) return;

	OnStateChanged.Broadcast();
	bDirty = false;                      // Reset dirty flag after broadcasting, since the change has been communicated.
}

void UTelescopeModel::SetElevTarget(float Degrees, bool BroadcastFlag)
{
	auto temp = FMath::Clamp(FMath::UnwindDegrees(Degrees), ElevTwistMin, ElevTwistMax);
	if (temp == ElevTarget || !FMath::IsFinite(temp)) return;

	ElevTarget = temp;
	bDirty = true;

	if (!BroadcastFlag) return;

	OnStateChanged.Broadcast();
	bDirty = false;                      
}

void UTelescopeModel::SetCassTarget(float Degrees, bool BroadcastFlag)
{
	auto temp = FMath::Clamp(FMath::UnwindDegrees(Degrees), CassTwistMin, CassTwistMax);
	if (temp == CassTarget || !FMath::IsFinite(temp)) return;

	CassTarget = temp;
	bDirty = true;

	if (!BroadcastFlag) return;

	OnStateChanged.Broadcast();
	bDirty = false;
}

void UTelescopeModel::SetTargets(float Azim, float Elev, float Cass)
{
	SetAzimTarget(Azim, false);
	SetElevTarget(Elev, false);
	SetCassTarget(Cass, false);
	if (!bDirty) return;

	OnStateChanged.Broadcast();
	bDirty = false;
}

void UTelescopeModel::ToggleLaser(bool bLaserState)
{
	bLaserOn = bLaserState;
}