// Fill out your copyright notice in the Description page of Project Settings.


#include "MotionLimitSettings.h"

UMotionLimitSettings::UMotionLimitSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CategoryName = TEXT("Project");
	SectionName = TEXT("MotionLimitSettings");
}
