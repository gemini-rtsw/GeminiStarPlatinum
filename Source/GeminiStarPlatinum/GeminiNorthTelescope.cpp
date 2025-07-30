#include "GeminiNorthTelescope.h"

// Sets default values
AGeminiNorthTelescope::AGeminiNorthTelescope()
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;

    AzimTwistTarget = 90.f;
    AzimTwistStrength = 5e9f;
    AzimVelocityTarget = 0.003f;
    AzimVelocityDamping = 2e10f;
    AzimAngularThreshold = 1.5f;

    ElevSwingTarget = 20.f;
    ElevSwingStrength = 5e10f;
    ElevVelocityTarget = 1e-4f;
    ElevVelocityDamping = 2e10f;
    ElevAngularThreshold = 0.5f;

    CassTwistTarget = 120.f;
    CassTwistStrength = 2e10f;
    CassVelocityTarget = 0.003f;
    CassVelocityDamping = 2e11f;
    CassAngularThreshold = 1.5f;

    const ConstructorHelpers::FObjectFinder<UStaticMesh> GroundMesh(TEXT("/Engine/EngineMeshes/ParticleCube.ParticleCube"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> AzimuthMesh(TEXT("/Game/TelescopeModels/azi_reduced_vsp/StaticMeshes/azi_reduced_vsp.azi_reduced_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> ElevationMesh(TEXT("/Game/TelescopeModels/elevation_vsp/StaticMeshes/elevation_vsp.elevation_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> CassegrainMesh(TEXT("/Game/TelescopeModels/cass_vsp/StaticMeshes/cass_vsp.cass_vsp"));
    const ConstructorHelpers::FObjectFinder<UStaticMesh> FluxCapacitorMesh(TEXT("/Engine/EngineMeshes/ParticleCube.ParticleCube"));

    RootSceneComponent = CreateDefaultSubobject<USceneComponent>("RootSceneComponent");
    RootComponent = RootSceneComponent;

    Ground = CreateMeshComponent(
        "Ground",                 // Name
        RootSceneComponent,       // Parent attachment that ensures new component rotates relative to parent
        GroundMesh.Object,        // Reference to mesh
        FVector::ZeroVector,      // Location relative to parent
        FRotator(90.f, 0.f, 0.f), // Rotation relative to parent
        1e34f,                    // Mass override (kg)
        FVector::ZeroVector       // Center of mass offsetting UE-generated COM
    );
    Ground->SetWorldScale3D(FVector(1.f, 1.f, 1.f)); // Sets scale of entire telescope, and children inheret scale because of SetupAttachment()
    Ground->SetHiddenInGame(true);
    SetBaseLocked(true);

    Azim = CreateMeshComponent(
        "Azimuth",
        Ground,
        AzimuthMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        40.f,
        FVector::ZeroVector
    );
    Elev = CreateMeshComponent(
        "Elevation",
        Azim,
        ElevationMesh.Object,
        FVector::ZeroVector,
        FRotator(0.f, 0.f, 180.f),
        10.f,
        FVector(-700.f, 0.f, 0.f)
    );
    Cass = CreateMeshComponent(
        "Cassegrain",
        Elev,
        CassegrainMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        2.5f,
        FVector::ZeroVector
    );
    FluxCapacitor = CreateMeshComponent(
        "Flux Capacitor",
        Cass,
        FluxCapacitorMesh.Object,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        0.f,
        FVector::ZeroVector
    );

    AzimConstraint = CreateConstraintComponent(
        "Azimuth Constraint", // Name
        Ground,               // Parent attachment
        Azim,                 // Child attachment
        FVector::ZeroVector,  // Location relative to parent
        FRotator::ZeroRotator,              // Rotation relative to parent (pitch, yaw, roll)
        true                  // Twists? Otherwise it swings
    );
    ElevConstraint = CreateConstraintComponent(
        "Elevation Constraint",
        Azim,
        Elev,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        false
    );
    CassConstraint = CreateConstraintComponent(
        "Cassegrain Constraint",
        Elev,
        Cass,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        true
    );
    FluxCapacitorConstraint = CreateConstraintComponent(
        "Flux Capacitor Constraint",
        Cass,
        FluxCapacitor,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        true
    );
    FluxCapacitorConstraint->SetAngularDriveParams(0.f, 1.f, 0.f);
    FluxCapacitorConstraint->SetAngularVelocityTarget(FVector(1.f, 0.f, 0.f)); // Set to always spin so model doesn't sleep


}

UStaticMeshComponent* AGeminiNorthTelescope::CreateMeshComponent(
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
    Mesh->SetCenterOfMass(COMOffset);

    Mesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    Mesh->SetCollisionObjectType(ECC_PhysicsBody);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);

    Mesh->SetLinearDamping(0.f);
    Mesh->SetAngularDamping(0.f);
    Mesh->SetUseCCD(true);
    Mesh->SetGyroscopicTorqueEnabled(true);
    Mesh->BodyInstance.PositionSolverIterationCount = 50;
    Mesh->BodyInstance.VelocitySolverIterationCount = 30;
    Mesh->BodyInstance.CustomSleepThresholdMultiplier = 0.f;

    return Mesh;
}

UPhysicsConstraintComponent* AGeminiNorthTelescope::CreateConstraintComponent(
    FName Name,
    USceneComponent* Parent,
    USceneComponent* Child,
    FVector Location,
    FRotator Rotation,
    bool bCanTwist
) {
    auto* Constraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(Name);
    Constraint->SetupAttachment(Parent);

    auto* Prim1 = Cast<UPrimitiveComponent>(Parent);
    auto* Prim2 = Cast<UPrimitiveComponent>(Child);
    if (Prim1 && Prim2)
        Constraint->SetConstrainedComponents(Prim2, NAME_None, Prim1, NAME_None);

    Constraint->SetRelativeLocation(Location);
    Constraint->SetRelativeRotation(Rotation);

    Constraint->SetAngularSwing1Limit(ACM_Locked, 0.f); // Yaw
    Constraint->SetAngularSwing2Limit(bCanTwist ? ACM_Locked : ACM_Limited, 45.f); // Pitch
    Constraint->SetAngularTwistLimit(bCanTwist ? ACM_Free : ACM_Locked, 0.f); // Roll
    Constraint->ConstraintInstance.AngularRotationOffset = bCanTwist ? FRotator(0.f, 0.f, 0.f) : FRotator(-45.f, 0.f, 0.f);

    Constraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
    Constraint->SetAngularDriveAccelerationMode(bAccelerationMode); // You can try setting to false, but the number needs to be VERY huge. The units are slightly obscure and require number to the 12th power (documentation says force, but it should be torque?)
    Constraint->SetAngularOrientationDrive(!bCanTwist, bCanTwist);
    Constraint->SetAngularVelocityDrive(!bCanTwist, bCanTwist);

    Constraint->ConstraintInstance.SetRefFrame(EConstraintFrame::Frame1, FTransform(FRotator::ZeroRotator, Prim1->GetCenterOfMass()));
    Constraint->ConstraintInstance.SetRefFrame(EConstraintFrame::Frame2, FTransform(FRotator::ZeroRotator, Prim2->GetCenterOfMass()));

    Constraint->SetDisableCollision(true);
    Constraint->SetProjectionEnabled(true);
    Constraint->SetProjectionParams(1.f, 1.f, .1f, 1.f);
    Constraint->ConstraintInstance.ProfileInstance.ConeLimit.bSoftConstraint = false;
    Constraint->ConstraintInstance.ProfileInstance.bEnableShockPropagation = true;
    Constraint->ConstraintInstance.ProfileInstance.bUseLinearJointSolver = false;
    Constraint->ConstraintInstance.ProfileInstance.bEnableMassConditioning = false;

    return Constraint;
}

// Called when the game starts or when spawned
void AGeminiNorthTelescope::BeginPlay()
{
    Super::BeginPlay();
    DisplayCenterOfMass();
}

// Called every frame
void AGeminiNorthTelescope::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    DisplayCenterOfMass();
    CalculateCOMOffset();

    DrawDebugString(GetWorld(), Azim->GetCenterOfMass() + FVector(0.f, 0.f, -20.f), Azim->GetComponentRotation().ToString(), nullptr, FColor::Green, 0.f, true, 1.f);
    DrawDebugString(GetWorld(), Elev->GetCenterOfMass() + FVector(0.f, 0.f, -20.f), Elev->GetComponentRotation().ToString(), nullptr, FColor::Blue, 0.f, true, 1.f);
    DrawDebugString(GetWorld(), Cass->GetCenterOfMass() + FVector(0.f, 0.f, -20.f), Cass->GetComponentRotation().ToString(), nullptr, FColor::Yellow, 0.f, true, 1.f);

    TwistComponent(AzimConstraint, AzimTwistTarget, AzimTwistStrength, AzimVelocityTarget, AzimVelocityDamping, AzimAngularThreshold);
    SwingComponent(ElevConstraint, ElevSwingTarget, ElevSwingStrength, ElevVelocityTarget, ElevVelocityDamping, ElevAngularThreshold);
    TwistComponent(CassConstraint, CassTwistTarget, CassTwistStrength, CassVelocityTarget, CassVelocityDamping, CassAngularThreshold);
}

void AGeminiNorthTelescope::DisplayCenterOfMass()
{
    struct FGroupCOM { FString Label; TArray<UStaticMeshComponent*> Components; FColor Color; };
    TArray<FGroupCOM> Groups = {
        { "Pivot", { Ground }, FColor::Cyan},
        { "Arm1COM", { Azim }, FColor::Green },
        { "Arm2COM", { Elev }, FColor::Blue },
        { "Arm3COM", { Cass }, FColor::Yellow },
        { "MainCOM", { Elev, Cass }, FColor::Red }
    };

    for (const auto& Group : Groups)
    {
        FVector Sum = FVector::ZeroVector;
        float MassSum = 0.f;
        for (auto* Comp : Group.Components)
        {
            if (!Comp) continue;
            const float M = Comp->GetMass();
            Sum += Comp->GetCenterOfMass() * M;
            MassSum += M;
        }
        if (MassSum > KINDA_SMALL_NUMBER)
        {
            FVector COM = Sum / MassSum;
            DrawDebugSphere(GetWorld(), COM, 8.f, 4, Group.Color, false, 0.f, 1, 0.f); // Draw dot at COM
            DrawDebugString(GetWorld(), COM + FVector(0.f, 0.f, -20.f), Group.Label, nullptr, Group.Color, 0.f, true, 1.f); // COM label
        }
    }
}

void AGeminiNorthTelescope::CalculateCOMOffset()
{
    // Get the local transform of the elevation constraint
    const FTransform ConstraintTransform = ElevConstraint->GetComponentTransform();

    // Convert elevation and cassegrain COM to local space of the elevation constraint
    FVector RelativeCOM = ConstraintTransform.InverseTransformPosition((Elev->GetCenterOfMass() + Cass->GetCenterOfMass()) * .5f);

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
        AngleDegrees = FMath::UnwindDegrees(AngleDegrees);
    }
}

void AGeminiNorthTelescope::TwistComponent(
    UPhysicsConstraintComponent* Constraint,
    float TwistTarget,
    float TwistStrength,
    float VelocityTarget,
    float VelocityDamping,
    float AngularThreshold
) {
    float RelativeTwist = TwistTarget - Constraint->GetCurrentTwist();

    if (FMath::Abs(RelativeTwist) > AngularThreshold)
    {
        Constraint->SetAngularVelocityTarget(FVector(FMath::Sign(RelativeTwist) * VelocityTarget, 0.f, 0.f));
        Constraint->SetAngularDriveParams(0.f, VelocityDamping, 0.f);
    }
    else
    {
        Constraint->SetAngularOrientationTarget(FRotator(0.f, 0.f, -TwistTarget));
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(TwistStrength, VelocityDamping, 0.f);
    }
    Constraint->SetAngularDriveAccelerationMode(bAccelerationMode);
}

void AGeminiNorthTelescope::SwingComponent(
    UPhysicsConstraintComponent* Constraint,
    float SwingTarget,
    float SwingStrength,
    float VelocityTarget,
    float VelocityDamping,
    float AngularThreshold
) {
    float RelativeSwing = FMath::UnwindDegrees(45 - SwingTarget - Constraint->GetCurrentSwing2()); // I don't understand why UE is not consistent with rotations, but this works if range is 0-90
    DrawDebugString(GetWorld(), Constraint->GetComponentLocation(), FString::SanitizeFloat(RelativeSwing), nullptr, FColor::Red, 0.f, true, 1.f); // COM label

    if (FMath::Abs(RelativeSwing) > AngularThreshold)
    {
        Constraint->SetAngularVelocityTarget(FVector(0.f, FMath::Sign(RelativeSwing) * VelocityTarget, 0.f));
        Constraint->SetAngularDriveParams(0.f, VelocityDamping, 0.f);
    }
    else
    {
        Constraint->SetAngularOrientationTarget(FRotator(SwingTarget - 45, 0.f, 0.f));
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(SwingStrength, VelocityDamping, 0.f);
    }
    Constraint->SetAngularDriveAccelerationMode(bAccelerationMode);
}

void AGeminiNorthTelescope::SetBaseLocked(bool bLocked)
{
    Ground->BodyInstance.bLockXTranslation = bLocked;
    Ground->BodyInstance.bLockYTranslation = bLocked;
    Ground->BodyInstance.bLockZTranslation = bLocked;
    Ground->BodyInstance.bLockXRotation = bLocked;
    Ground->BodyInstance.bLockYRotation = bLocked;
    Ground->BodyInstance.bLockZRotation = bLocked;
}
