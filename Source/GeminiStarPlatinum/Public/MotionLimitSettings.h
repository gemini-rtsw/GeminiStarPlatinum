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
	float DomeTwistMin = -270.f;
	UPROPERTY(Config, EditAnywhere, Category = "Dome Motion Limit Settings")
	float DomeTwistMax = 270.f;

	// The following three properties were determined from Logan's numbers. Not sure where they came from but trust.
	// There's another copy of these motion limits on the python side, so update both when modifying one
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
	float AzimTwistMin = -180.f;
	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float AzimTwistMax = 360.f;

	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float ElevTwistMin = -90.f;
	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float ElevTwistMax =  0.f;

	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float CassTwistMin = -179.9f;
	UPROPERTY(Config, EditAnywhere, Category = "Telescope Motion Limit Settings")
	float CassTwistMax = 180.f;
};
