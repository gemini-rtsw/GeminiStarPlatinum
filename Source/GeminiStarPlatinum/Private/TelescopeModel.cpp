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

void UTelescopeModel::SetAzimTarget(float Degrees)
{
	AzimTarget = FMath::Clamp(FMath::UnwindDegrees(Degrees), AzimTwistMin, AzimTwistMax);
	OnStateChanged.Broadcast();
}

void UTelescopeModel::SetElevTarget(float Degrees)
{
	ElevTarget = FMath::Clamp(FMath::UnwindDegrees(Degrees), ElevTwistMin, ElevTwistMax);
	OnStateChanged.Broadcast();
}

void UTelescopeModel::SetCassTarget(float Degrees)
{
	CassTarget = FMath::Clamp(FMath::UnwindDegrees(Degrees), CassTwistMin, CassTwistMax);
	OnStateChanged.Broadcast();
}

// TODO: See if this is actually necessary, because this just seems rough
//       Also triple broadcast might do some things
void UTelescopeModel::SetTarget(float Azim, float Elev, float Cass)
{
	SetAzimTarget(Azim);
	SetElevTarget(Elev);
	SetCassTarget(Cass);
}

void UTelescopeModel::ToggleLaser(bool bLaserState)
{
	bLaserOn = bLaserState;
}