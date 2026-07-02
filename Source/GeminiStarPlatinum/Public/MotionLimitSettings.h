// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MotionLimitSettings.generated.h"

/**
 * 
 */
UCLASS(config=Game, defaultconfig)
class GEMINISTARPLATINUM_API UMotionLimitSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMotionLimitSettings(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(Config, EditAnywhere, Category="Dome Motion Limit Settings")
	float DomeTwistMin = -179.9f;
	UPROPERTY(Config, EditAnywhere, Category = "Dome Motion Limit Settings")
	float DomeTwistMax = 180.f;

	// The following three properties were determined from Logan's numbers. Not sure where they came from but trust.
	UPROPERTY(Config, EditAnywhere, Category = "Dome Motion Limit Settings")
	float TopShutterSwingMin = -7.f;
	UPROPERTY(Config, EditAnywhere, Category = "Dome Motion Limit Settings")
	float TopShutterSwingMax = 83.f;

	UPROPERTY(Config, EditAnywhere, Category = "Dome Motion Limit Settings")
	float BotShutterSwingMin = -13.f;
	UPROPERTY(Config, EditAnywhere, Category = "Dome Motion Limit Settings")
	float BotShutterSwingMax = -3.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Dome Motion Limit Settings")
	float VentSlideMin = 0.f;
	UPROPERTY(Config, EditAnywhere, Category = "Dome Motion Limit Settings")
	float VentSlideMax = 500.f;



	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float AzimTwistMin = -179.9f;
	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float AzimTwistMax = 180.f;

	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float ElevTwistMin = -90.f;
	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float ElevTwistMax = 0.f;

	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float CassTwistMin = -179.9f;
	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float CassTwistMax = 180.f;
};
