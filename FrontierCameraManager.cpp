// Copyright Alex Nye.  Released as open source under the MiT License.

#include "FrontierCameraManager.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"

#include "TacticalCamera.h"

void AFrontierCameraManager::OnConstruction(const FTransform& Transform) {
    Super::OnConstruction(Transform);

    Camera = GetWorld()->SpawnActor<ATacticalCamera>(ATacticalCamera::StaticClass());
    // The editor preview calls `UpdateCamera`, but there's no pawn yet, so we need to put in a dummy value.
    FollowedActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
}

void AFrontierCameraManager::BeginPlay() {
    Super::BeginPlay();

    FollowedActor = GetWorld()->GetFirstPlayerController()->GetPawn();

    auto Yaw = TargetYaw;
    auto PlanarDistance = DISTANCE * cos(FMath::DegreesToRadians(PITCH));
    auto X = PlanarDistance * cos(FMath::DegreesToRadians(Yaw));
    auto Y = PlanarDistance * sin(FMath::DegreesToRadians(Yaw));
    auto OffsetFromTarget = FVector(X, Y, -1 * HEIGHT);
    auto Destination = FollowedActor->GetActorLocation() - OffsetFromTarget;

    Camera->SetActorLocationAndRotation(Destination, FRotator(PITCH, 0.f, 0.f));
    SetViewTarget(Camera);
}

// TODO: move the tactical camera logic to the tactical camera class.
// There may come a time when we want to add e.g. a cinematic camera to battles.

// The camera:
//  - follows the TargetActor at DISTANCE and HEIGHT,
//  - moves toward the next target when the FollowedActor changes,
//  - does not automatically rotate with the target, but can rotate around the target,
//  - and eases through all of these transitions.
void AFrontierCameraManager::UpdateCamera(float DeltaTime)
{
    Super::UpdateCamera(DeltaTime);
    
    RotationTimeElapsed += DeltaTime;
    TranslationTimeElapsed += DeltaTime;

    auto NewRotation = FMath::RInterpTo(
        GetCameraRotation(),
        FRotator(PITCH, TargetYaw, 0.f),
        DeltaTime,
        Ease(RotationTimeElapsed) * 5.f
    );

    auto Source = GetCameraLocation();
    auto Destination = FollowedActor->GetActorLocation() - OffsetFromTarget(NewRotation.Yaw);
    auto Speed = Ease(TranslationTimeElapsed);
    auto NewLocation = FMath::Lerp(Source, Destination, Speed);

    Camera->SetActorLocationAndRotation(NewLocation, NewRotation);
}

// Tells the camera manager our goal target, doesn't immediately go to.
void AFrontierCameraManager::Target(AActor* NewTarget) {
    TranslationTimeElapsed = 0.f;
    FollowedActor = NewTarget;
}

// Direction is -1 or 1.
// Tells the camera manager our goal rotation, doesn't immediately rotate.
void AFrontierCameraManager::Rotate(int32 Direction) {
    if (RotationTimeElapsed < 1.f) return;
    auto DeltaYaw = 90 * Direction;
    TargetYaw = TargetYaw + DeltaYaw;

    // Normalize to the expected range of -180 to 180.
    if (TargetYaw > 180) {
        TargetYaw -= 360;
    } else if (TargetYaw < -180) {
        TargetYaw += 360;
    }

    RotationTimeElapsed = 0.f;
}

// Ease in out cubic.  Speed is slow near zero and 1, but fast near 0.5.
// Produces 1 second constant travel time to a stationary target.
float AFrontierCameraManager::Ease(float Progress) {
    auto Scale = 4.f;
    Progress = FMath::Clamp(Progress / Scale, 0.0f, 1.0f);
    return Progress < 0.5 ? 4 * Progress * Progress * Progress : 1 - pow(-2 * Progress + 2, 3) / 2;
}