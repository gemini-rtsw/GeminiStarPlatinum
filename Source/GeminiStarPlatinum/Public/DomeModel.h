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
	UPROPERTY(BlueprintReadOnly) bool  bOpen           =  false;

	// Rotational imits, temporary until a data asset is created representing real life physical limitations
	UPROPERTY(EditAnywhere)      float TwistMin        = -180.f, 
		                               TwistMax        =  180.f;

	UFUNCTION(BlueprintCallable) void SetDomeTwistTarget(float Degrees);
	UFUNCTION(BlueprintCallable) void SetOpen(bool bNewOpen);
};
