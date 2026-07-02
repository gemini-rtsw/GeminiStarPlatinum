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

	// Motion Limit Settings reference
	UPROPERTY(BlueprintReadOnly) const UMotionLimitSettings* MotionLimitSettings;

	// Rotational limits -- TODO: Use DataAsset to store these limits somewhere centralized
	UPROPERTY(EditAnywhere)      float AzimTwistMin    = -180.f,
								       AzimTwistMax    =  180.f;
	UPROPERTY(EditAnywhere)      float ElevTwistMin    = -180.f,
								       ElevTwistMax    =  180.f;
	UPROPERTY(EditAnywhere)      float CassTwistMin    = -180.f,
								       CassTwistMax    =  180.f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable) void SetAzimTarget(float Degrees);
	UFUNCTION(BlueprintCallable) void SetElevTarget(float Degrees);
	UFUNCTION(BlueprintCallable) void SetCassTarget(float Degrees);
	UFUNCTION(BlueprintCallable) void SetTarget(float Azim, float Elev, float Cass);
	UFUNCTION(BlueprintCallable) void ToggleLaser(bool bLaserState);
};
