#include "MovingThing.h"

// Sets default values
AMovingThing::AMovingThing()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMovingThing::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMovingThing::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

inline float angle_norm(float x) {
    x = fmod(x + 180.f, 360.f);
    if (x < 0)
        x += 360.f;
    return x - 180.f;
}

// Create new component and initializes properties usually for precise movement
UStaticMeshComponent* AMovingThing::CreateMeshComponent(
    FName Name,
    USceneComponent* Parent,
    UStaticMesh* MeshAsset,
    FVector Location,
    FRotator Rotation,
    float Mass,
    FVector COMOffset
) {
    auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Mesh->SetupAttachment(Parent);
    Mesh->SetStaticMesh(MeshAsset);

    Mesh->SetRelativeLocation(Location);
    Mesh->SetRelativeRotation(Rotation);

    Mesh->SetSimulatePhysics(true);
    Mesh->SetMassOverrideInKg(NAME_None, Mass);
    Mesh->SetEnableGravity(false);
    Mesh->SetCenterOfMass(COMOffset);

    Mesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    Mesh->SetCollisionObjectType(ECC_PhysicsBody);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);

    Mesh->SetLinearDamping(0.f);
    Mesh->SetAngularDamping(0.f);
	Mesh->SetUseCCD(false); // Continuous collision detection is not needed for slow movement, but can be enabled if needed
	Mesh->SetGyroscopicTorqueEnabled(true); // If you spin it really fast, you can see the gyroscopic effect :)
    Mesh->BodyInstance.PositionSolverIterationCount = 50; // Numbers do not need to be this high, but CPU isn't a concern at the moment
    Mesh->BodyInstance.VelocitySolverIterationCount = 30;
    Mesh->BodyInstance.ProjectionSolverIterationCount = 100;
	Mesh->BodyInstance.CustomSleepThresholdMultiplier = 0.f; // Prevents the component from sleeping, so it can be moved precisely
    Mesh->BodyInstance.StabilizationThresholdMultiplier = 0.f;

    return Mesh;
}

// Create new constraint component connecting parent and child focusing on precision
UPhysicsConstraintComponent* AMovingThing::CreateConstraintComponent(
    FName Name,
    USceneComponent* Parent,
    USceneComponent* Child,
    FVector Location,
    FRotator Rotation,
    bool bCanTwist,
    bool bCanSwing,
    bool bCanSlide,
    float SwingAngle,
    float SwingLimit
) {
    auto* Constraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(Name);
	Constraint->SetupAttachment(Parent); // This ensures the constraint rotates relattive with the parent component

    auto* Prim1 = Cast<UPrimitiveComponent>(Parent);
    auto* Prim2 = Cast<UPrimitiveComponent>(Child);
    if (Prim1 && Prim2)
        Constraint->SetConstrainedComponents(Prim2, NAME_None, Prim1, NAME_None);

    Constraint->SetRelativeLocation(Location);
    Constraint->SetRelativeRotation(Rotation);

    Constraint->SetAngularSwing1Limit(ACM_Locked, 0.f); // Yaw
    Constraint->SetAngularSwing2Limit(bCanSwing ? ACM_Limited : ACM_Locked, SwingLimit); // Pitch
    Constraint->SetAngularTwistLimit(bCanTwist ? ACM_Free : ACM_Locked, 0.f); // Roll
	Constraint->ConstraintInstance.AngularRotationOffset = bCanTwist ? FRotator(0.f, 0.f, 0.f) : FRotator(SwingAngle, 0.f, 0.f); // This is the offset to set the direction of the constraint, so the limit is +/- angle from the offset

    Constraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
    Constraint->SetAngularDriveAccelerationMode(bAccelerationMode); // You can try setting to false, but the number needs to be VERY huge.  The units are slightly obscure and require number to the 12th power (documentation says force, but it should be torque? Where's the force applied?)
    Constraint->SetAngularOrientationDrive(bCanSwing, bCanTwist);
    Constraint->SetAngularVelocityDrive(bCanSwing, bCanTwist);

    Constraint->SetLinearXLimit(bCanSlide ? LCM_Limited : LCM_Locked, 500.f); // X axis
    Constraint->SetLinearDriveAccelerationMode(bAccelerationMode);
    Constraint->SetLinearVelocityDrive(bCanSlide, false, false);

	Constraint->SetDisableCollision(true); // Prevents collision between parent and child components
	Constraint->SetProjectionEnabled(true); // Enables projection to prevent constraint violations
    Constraint->SetProjectionParams(1.f, 1.f, 0.1f, 0.01f);
    Constraint->ConstraintInstance.ProfileInstance.ConeLimit.bSoftConstraint = false;
    Constraint->ConstraintInstance.ProfileInstance.bEnableShockPropagation = true;
    Constraint->ConstraintInstance.ProfileInstance.bUseLinearJointSolver = false;
	Constraint->ConstraintInstance.ProfileInstance.bEnableMassConditioning = false; // This slightly messes with the movement, so it is disabled

    return Constraint;
}

/// <summary>
/// Handles Twisting motion of a component in a constraint
/// </summary>
/// <param name="Constraint">Unreal physics constraint</param>
/// <param name="TwistTarget">Angle in degrees that component will twist to</param>
/// <param name="TwistStrength">Strength of twisting motion</param>
/// <param name="VelocityTarget">Target velocity when component is twisting to target</param>
/// <param name="VelocityDamping">Strength of velocity damping or control</param>
/// <param name="AngularThreshold">Margin of error when rotating before snapping</param>
void AMovingThing::TwistComponent(
    UPhysicsConstraintComponent* Constraint,
    float TwistTarget,
    float TwistStrength,
    float VelocityTarget,
    float VelocityDamping,
    float AngularThreshold
) {
    float RelativeTwist = FMath::UnwindDegrees(TwistTarget) - Constraint->GetCurrentTwist(); // Twist target is locked to [-180deg, 180deg] range before processing
                                                                                             // Leads to nonoptimal motions, but may reflect actual telescope rotational limits better

	if (FMath::Abs(RelativeTwist) > AngularThreshold) // If the twist is outside the threshold, apply velocity
    {
        Constraint->SetAngularVelocityTarget(FVector(FMath::Sign(RelativeTwist) * VelocityTarget, 0.f, 0.f));
        Constraint->SetAngularDriveParams(0.f, VelocityDamping, 0.f);
    }
	else // If the twist is within the threshold, set the orientation and strength (almost like a "snap" to the target)
    {
        Constraint->SetAngularOrientationTarget(FRotator(0.f, 0.f, -TwistTarget));
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(TwistStrength, VelocityDamping, 0.f);
    }
	Constraint->SetAngularDriveAccelerationMode(bAccelerationMode); // Set each tick in case the mode is changed
}

// Handles swinging movement of a component
void AMovingThing::SwingComponent(
    UPhysicsConstraintComponent* Constraint,
    float SwingTarget,
    float SwingStrength,
    float VelocityTarget,
    float VelocityDamping,
    float AngularThreshold,
    float SwingOffset
) {
    float CurrentSwing = -(Constraint->GetCurrentSwing2()); // GetCurrentSwing2 returns the angle negative, so negate it to match the target
    float RelativeSwing = FMath::UnwindDegrees(CurrentSwing + SwingOffset - SwingTarget);

	if (FMath::Abs(RelativeSwing) > AngularThreshold) // If the swing is outside the threshold, apply velocity
    {
        Constraint->SetAngularVelocityTarget(FVector(0.f, FMath::Sign(RelativeSwing) * VelocityTarget, 0.f));
        Constraint->SetAngularDriveParams(0.f, VelocityDamping, 0.f);
    }
	else // If the swing is within the threshold, set the orientation and strength (almost like a "snap" to the target)
    {
        Constraint->SetAngularOrientationTarget(FRotator(SwingTarget - SwingOffset, 0.f, 0.f));
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(SwingStrength, VelocityDamping, 0.f);
    }
	Constraint->SetAngularDriveAccelerationMode(bAccelerationMode); // Set each tick in case the mode is changed
}

// FIXME: Doesn't calculate anything useful :/ Math idea seems correct though
void AMovingThing::CalculateCOMOffset(UPhysicsConstraintComponent* Constraint, UStaticMeshComponent* Component1, UStaticMeshComponent* Component2)
{
    // Get the local transform of the elevation constraint
    const FTransform ConstraintTransform = Constraint->GetComponentTransform();

    // Convert elevation and cassegrain COM to local space of the elevation constraint
    FVector RelativeCOM = ConstraintTransform.InverseTransformPosition((Component1->GetCenterOfMass() + Component2->GetCenterOfMass()) * .5f);

    // Flatten the point to the XZ or Swing2 plane
    FVector2D FlatPoint(RelativeCOM.X, RelativeCOM.Z);

    // Avoid division by zero. Uses KINDA_SMALL_NUMBER
    if (FlatPoint.IsNearlyZero())
    {
        // Convert magnitude of vector to 1
        FlatPoint.Normalize();

        // The forward X direction in the XZ plane
        FVector2D ForwardVector(1.f, 0.f);

        // Calculate dot product and cross product
        float Dot = FVector2D::DotProduct(FlatPoint, ForwardVector); // Cosine of angle
        float Cross = FlatPoint.X * ForwardVector.Y - FlatPoint.Y * ForwardVector.X; // Determinant of vectors

        // Calculate angle in radians
        float AngleRadians = FMath::Atan2(Cross, Dot);

        // Convert to degrees
        float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);

        // Normalize to [-180, 180]
        PivotAngularOffset = FMath::UnwindDegrees(AngleDegrees);
    }
}

// Locks the base component in place, preventing any movement
void AMovingThing::SetBaseLocked(UStaticMeshComponent* Base, bool bLocked)
{
    Base->BodyInstance.bLockXTranslation = bLocked;
    Base->BodyInstance.bLockYTranslation = bLocked;
    Base->BodyInstance.bLockZTranslation = bLocked;
    Base->BodyInstance.bLockXRotation = bLocked;
    Base->BodyInstance.bLockYRotation = bLocked;
    Base->BodyInstance.bLockZRotation = bLocked;
}
