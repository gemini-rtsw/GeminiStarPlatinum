// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssemblyModel.h"
#include "MotionLimitSettings.h"
#include "DomeModel.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class GEMINISTARPLATINUM_API UDomeModel : public UAssemblyModel
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly) float DomeTwistTarget =  0.f;
	UPROPERTY(BlueprintReadOnly) float TopSSwingTarget = -7.f;
	UPROPERTY(BlueprintReadOnly) float BotSSwingTarget = -3.5f;
	UPROPERTY(BlueprintReadOnly) float VentSlideTarget =  0.f;

	/// <summary>
	/// Indicates whether the dome shutter and vents are open or not
	/// </summary>
	UPROPERTY(BlueprintReadOnly) bool  bOpen           =  false;
	/// <summary>
	/// State change flag, set to true when a target is changed and not yet broadcasted. Set to false after broadcasting.
	/// Setters should handle this once individually; SetTargets() should handle this once for all targets.
	/// DO NOT MIX AND MATCH BROADCAST FLAGS (i.e. having True for one target and False for another);
	/// Early-returns in setters prevent broadcasting if target is unchanged regardless of bDirty state.
	/// INVARIANT: bDirty should always be false between operations, w/ setters clearing it and SetTargets() flushing it.
	/// </summary>
	UPROPERTY(BlueprintReadOnly) bool  bDirty          =  false;

	// Stores motion limits for everything in the observatory. 
	UPROPERTY(BlueprintReadOnly) const UMotionLimitSettings* MotionLimitSettings;

	// Rotational limits, retrieved from MotionLimitSettings and kept here for less indirection.
	UPROPERTY(EditAnywhere)      float DomeTwistMin        = -179.9f, 
		                               DomeTwistMax        =  180.f;
	UPROPERTY(EditAnywhere)      float TopShutterSwingMin  = -7.f,
									   TopShutterSwingMax  =  83.f;
	UPROPERTY(EditAnywhere)      float BotShutterSwingMin  = -13.f,
									   BotShutterSwingMax  = -3.5f;
	UPROPERTY(EditAnywhere)      float VentSlideMin        =  0.f,
									   VentSlideMax        =  500.f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/// <summary>
	/// Sets where the dome will point/twist to in degrees
	/// </summary>
	/// <param name="Degrees">Angular twist in degrees (float)</param>
	UFUNCTION(BlueprintCallable) void SetDomeTwistTarget(float Degrees, bool BroadcastFlag);
	/// <summary>
	/// Sets where the top shutter will point/twist to in degrees
	/// </summary>
	/// <param name="Degrees">Angular swing in degrees (float)</param>
	UFUNCTION(BlueprintCallable) void SetTopShutterTarget(float Degrees, bool BroadcastFlag);
	/// <summary>
	/// Sets where the bottom shutter will point/twist to in degrees
	/// </summary>
	/// <param name="Degrees">Angular swing in degrees (float)</param>
	UFUNCTION(BlueprintCallable) void SetBotShutterTarget(float Degrees, bool BroadcastFlag);
	/// <summary>
	/// Sets how far the vents will slide open in Unreal world units
	/// </summary>
	/// <param name="SlideAmount">Slide in world units, Range 0 - 500.0 (float)</param>
	UFUNCTION(BlueprintCallable) void SetVentTarget(float SlideAmount, bool BroadcastFlag);
	/// <summary>
	/// Sets all targets at once, clamping to the limits defined in MotionLimitSettings
	/// </summary>
	/// <param name="DomeTwist">Angular twist in degrees (float)</param>
	/// <param name="TopSSwing">Angular swing in degrees (float)</param>
	/// <param name="BotSSwing">Angular swing in degrees (float)</param>
	/// <param name="VentSlide">Slide in world units, Range 0 - 500.0 (float)</param>
	UFUNCTION(BlueprintCallable) void SetTargets(float DomeTwist, float TopSSwing, float BotSSwing, float VentSlide);

	/// <summary>
	/// Toggles bOpen, controls dome shutter and vents open/closed state
	/// </summary>
	/// <param name="bNewOpen">Open(true) or closed(false) state</param>
	UFUNCTION(BlueprintCallable) void SetOpen(bool bNewOpen);
};
