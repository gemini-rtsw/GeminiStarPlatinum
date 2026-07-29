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

/// <summary>
/// Maps an arbitrary angle into the azimuth cable-wrap range [-180, 360] deg.
/// In-range values (limits inclusive) pass through untouched — the extended span exceeds 360deg,
/// so out-of-range inputs are inherently ambiguous (e.g. 370 could mean 10 or -350) and are only
/// canonicalized: values <= -180 map into (-180, 180], values >= 360 map into [0, 360).
/// </summary>
/// <param name="NewRotation">Angle in degrees, any value</param>
/// <returns>Equivalent angle within [-180, 360] deg</returns>
float UTelescopeModel::UnwrapGeminiAz(float NewRotation)
{
	if (NewRotation >= -180.f && NewRotation <= 360.f) // Within typical range
	{
		return NewRotation;
	}
	else if (NewRotation <= -180.f) 
	{
		return FMath::Fmod(360.f + FMath::Fmod(NewRotation + 180.f, 360.f), 360.f) - 180.f;
	}
	else
	{
		return FMath::Fmod(360.f + FMath::Fmod(NewRotation, 360.f), 360.f);
	}
}

void UTelescopeModel::SetAzimTarget(float Degrees, bool BroadcastFlag)
{
	auto temp = FMath::Clamp(UnwrapGeminiAz(Degrees), AzimTwistMin, AzimTwistMax);
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