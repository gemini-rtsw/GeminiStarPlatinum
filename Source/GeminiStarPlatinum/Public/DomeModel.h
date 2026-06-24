// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssemblyModel.h"
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

	// Rotational imits, temporary until a data asset is created representing real life physical limitations
	UPROPERTY(EditAnywhere)      float TwistMin        = -180.f, 
		                               TwistMax        =  180.f;

	/// <summary>
	/// Sets where the dome will point/twist to in degrees
	/// </summary>
	/// <param name="Degrees">Angular twist in degrees (float)</param>
	UFUNCTION(BlueprintCallable) void SetDomeTwistTarget(float Degrees);

	/// <summary>
	/// Sets where the top shutter will point/twist to in degrees
	/// </summary>
	/// <param name="Degrees">Angular swing in degrees (float)</param>
	UFUNCTION(BlueprintCallable) void SetTopShutterTarget(float Degrees);
	/// <summary>
	/// Sets where the bottom shutter will point/twist to in degrees
	/// </summary>
	/// <param name="Degrees">Angular swing in degrees</param>
	UFUNCTION(BlueprintCallable) void SetBotShutterTarget(float Degrees);
	/// <summary>
	/// Sets how far the vents will slide open in Unreal world units
	/// </summary>
	/// <param name="Degrees">Slide in world units</param>
	UFUNCTION(BlueprintCallable) void SetVentTarget(float Degrees);

	/// <summary>
	/// Toggles bOpen, controls dome shutter and vents open/closed state
	/// </summary>
	/// <param name="bNewOpen">Open(true) or closed(false) state</param>
	UFUNCTION(BlueprintCallable) void SetOpen(bool bNewOpen);
};
