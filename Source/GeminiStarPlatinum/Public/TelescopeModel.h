// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssemblyModel.h"
#include "MotionLimitSettings.h"
#include "TelescopeModel.generated.h"

/**
 * Stores all data related to the motions and operation of the telescope model.
 * Broadcasts to listeners when setter is set to broadcast (default with SetTargets).
 * Model should always be passive; it never DIRECTLY changes something in another script/actor.
 * 
 * Author: Benjamin Bercasio
 */
UCLASS(BlueprintType)
class GEMINISTARPLATINUM_API UTelescopeModel : public UAssemblyModel
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly) float AzimTarget =  180.f;
	UPROPERTY(BlueprintReadOnly) float ElevTarget = -60.f;
	UPROPERTY(BlueprintReadOnly) float CassTarget =  120.f;

	/// <summary>
	/// Indicates whether the telescope guide laser is on or not.
	/// </summary>
	UPROPERTY(BlueprintReadOnly) bool  bLaserOn = false;
	/// <summary>
	/// State change flag, set to true when a target is changed and not yet broadcasted. Set to false after broadcasting.
	/// Refer to DomeModel::bDirty for more information on how to use this flag.
	/// </summary>
	UPROPERTY(BlueprintReadOnly) bool  bDirty   = false;

	// Motion Limit Settings reference
	UPROPERTY(BlueprintReadOnly) const UMotionLimitSettings* MotionLimitSettings;

	// Rotational limits
	UPROPERTY(EditAnywhere)      float AzimTwistMin    = -180.f,
								       AzimTwistMax    =  360.f;
	UPROPERTY(EditAnywhere)      float ElevTwistMin    = -90.f,
								       ElevTwistMax    =  0.f;
	UPROPERTY(EditAnywhere)      float CassTwistMin    = -179.9f,
								       CassTwistMax    =  180.f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable) void SetAzimTarget(float Degrees, bool BroadcastFlag);
	UFUNCTION(BlueprintCallable) void SetElevTarget(float Degrees, bool BroadcastFlag);
	UFUNCTION(BlueprintCallable) void SetCassTarget(float Degrees, bool BroadcastFlag);
	UFUNCTION(BlueprintCallable) void SetTargets(float Azim, float Elev, float Cass);
	UFUNCTION(BlueprintCallable) void ToggleLaser(bool bLaserState);
private:

	/// <summary>
	/// Maps an arbitrary angle into the azimuth cable-wrap range [-180, 360] deg.
	/// In-range values (limits inclusive) pass through untouched — the extended span exceeds 360deg,
	/// so out-of-range inputs are inherently ambiguous (e.g. 370 could mean 10 or -350) and are only
	/// canonicalized: values <= -180 map into (-180, 180], values >= 360 map into [0, 360).
	/// </summary>
	/// <param name="NewRotation">Angle in degrees, any value</param>
	/// <returns>Equivalent angle within [-180, 360] deg</returns>
	float UnwrapGeminiAz(float NewRotation);

};
