// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssemblyModel.h"
#include "MotionLimitSettings.h"
#include "TelescopeModel.generated.h"

/**
 * 
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
	UPROPERTY(EditAnywhere)      float AzimTwistMin    = -179.9f,
								       AzimTwistMax    =  180.f;
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
};
