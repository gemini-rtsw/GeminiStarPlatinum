#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "DrawDebugHelpers.h"
#include "GeminiNorthTelescope.generated.h"

UCLASS()
class GEMINISTARPLATINUM_API AGeminiNorthTelescope : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AGeminiNorthTelescope();
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Azimuth center of mass offset relative to the center of mass calculated from static mesh
    UPROPERTY(BlueprintReadWrite)
    FVector AzimCOMOffset;
    // Elevation center of mass offset relative to the center of mass calculated from static mesh
    UPROPERTY(BlueprintReadWrite)
    FVector ElevCOMOffset;
    // Cassegrain center of mass offset relative to the center of mass calculated from static mesh
    UPROPERTY(BlueprintReadWrite)
    FVector CassCOMOffset;

    // Azimuth target rotation (roll)
    UPROPERTY(BlueprintReadWrite)
    float AzimTwistTarget;
    // Spring strength of azimuth target orientation (kg*cm*s^-2 or cm*s^-2)
    UPROPERTY(BlueprintReadWrite)
    float AzimTwistStrength;
    // Azimuth rotational velocity target (revolutions per second)
    UPROPERTY(BlueprintReadWrite)
    float AzimVelocityTarget;
    // Azimuth rotational target damping (kg*cm*s^-1 pr cm*s^-1)
    UPROPERTY(BlueprintReadWrite)
    float AzimVelocityDamping;
    // Threshold to activate azimuth orientation target spring
    UPROPERTY(BlueprintReadWrite)
    float AzimAngularThreshold;

    // Elevation target rotation (pitch)
    UPROPERTY(BlueprintReadWrite)
    float ElevSwingTarget;
    // Spring strength of elevation target orientation (kg*cm*s^-2 or cm*s^-2)
    UPROPERTY(BlueprintReadWrite)
    float ElevSwingStrength;
    // Elevation rotational velocity target (revolutions per second)
    UPROPERTY(BlueprintReadWrite)
    float ElevVelocityTarget;
    // Elevation rotational target damping (kg*cm*s^-1 or cm*s^-1)
    UPROPERTY(BlueprintReadWrite)
    float ElevVelocityDamping;
    // Threshold to activate elevation orientation target spring
    UPROPERTY(BlueprintReadWrite)
    float ElevAngularThreshold;

    // Cassegrain target rotation (roll)
    UPROPERTY(BlueprintReadWrite)
    float CassTwistTarget;
    // Spring strength of cassegrain target orientation (kg*cm*s^-2  or cm*s^-2)
    UPROPERTY(BlueprintReadWrite)
    float CassTwistStrength;
    // Cassegrain rotational velocity target (revolutions per second)
    UPROPERTY(BlueprintReadWrite)
    float CassVelocityTarget;
    // Cassegrain rotational target damping (kg*cm*s^-1  or cm*s^-1)
    UPROPERTY(BlueprintReadWrite)
    float CassVelocityDamping;
    // Threshold to activate cassegrain orientation target spring
    UPROPERTY(BlueprintReadWrite)
    float CassAngularThreshold;

    // Drives will ignore mass and use acceleration instead
    UPROPERTY(BlueprintReadWrite)
    bool bAccelerationMode = true;

    // Lock ground in place
    void SetBaseLocked(bool bLocked);

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Root component
    UPROPERTY(VisibleAnywhere)
    USceneComponent* RootSceneComponent;
    // Ground acting as Earth
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Ground;
    // Azimuth component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Azim;
    // Elevation component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Elev;
    // Cassegrain component
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Cass;
    // Silly component that rotates forever, tricking the actor to never sleep, which allows for slow rotations
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* FluxCapacitor;

    // Connects ground to azimuth
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* AzimConstraint;
    // Connects azimuth to elevation
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* ElevConstraint;
    // Connects elevation to cassegrain
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* CassConstraint;
    // Connects cassegrain to flux capacitor
    UPROPERTY(VisibleAnywhere)
    UPhysicsConstraintComponent* FluxCapacitorConstraint;

    // Display COM dot each frame
    void DisplayCenterOfMass();
    // Calculate center of mass offset needed to balance the telescope at a certain angle
    void CalculateCOMOffset();

    // Determine constraint twist drive each frame
    void TwistComponent(
        UPhysicsConstraintComponent* Constraint,
        float TwistTarget,
        float TwistStrength,
        float VelocityTarget,
        float VelocityDamping,
        float AngularThreshold
    );
    // Determine constraint swing drive each frame
    void SwingComponent(
        UPhysicsConstraintComponent* Constraint,
        float SwingTarget,
        float SwingStrength,
        float VelocityTarget,
        float VelocityDamping,
        float AngularThreshold
    );

    // Create new component connected to parent 
    UStaticMeshComponent* CreateMeshComponent(
        FName Name,
        USceneComponent* Parent,
        UStaticMesh* MeshAsset,
        FVector Location,
        FRotator Rotation,
        float Mass,
        FVector COMOffset
    );
    // Create new constraint connecting parent and child connected to parent
    UPhysicsConstraintComponent* CreateConstraintComponent(
        FName Name,
        USceneComponent* Parent,
        USceneComponent* Child,
        FVector Location,
        FRotator Rotation,
        bool bCanTwist
    );
};
