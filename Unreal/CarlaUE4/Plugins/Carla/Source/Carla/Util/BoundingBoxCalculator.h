// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Carla/Util/BoundingBox.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "BoundingBoxCalculator.generated.h"

class AActor;

UCLASS()
class CARLA_API UBoundingBoxCalculator : public UBlueprintFunctionLibrary
{
  GENERATED_BODY()

public:

  /// Compute the bounding box of the given Carla actor.
  ///
  /// @warning If the actor type is not supported a default initialized bounding
  /// box is returned.
  ///
  /// @warning Traffic signs return its trigger box instead.
  UFUNCTION(Category = "Carla Actor", BlueprintCallable)
  static FBoundingBox GetActorBoundingBox(const AActor *Actor);

  UFUNCTION(Category = "Carla Util", BlueprintCallable)
  static FBoundingBox GetSkeletalMeshBoundingBox(const USkeletalMesh* SkeletalMesh);

  UFUNCTION(Category = "Carla Util", BlueprintCallable)
  static FBoundingBox GetStaticMeshBoundingBox(const UStaticMesh* StaticMesh);

  UFUNCTION(Category = "Carla Util", BlueprintCallable)
  static void GetHISMBoundingBox(
    UHierarchicalInstancedStaticMeshComponent* HISMComp,
    TArray<FBoundingBox>& OutBoundingBox);

  UFUNCTION(Category = "Carla Util", BlueprintCallable)
  static void GetFolliageBoundingBox(
    AInstancedFoliageActor* InstancedFolliageActor,
    TArray<FBoundingBox>& OutBoundingBox);

  UFUNCTION(Category = "Carla Util", BlueprintCallable)
  static TArray<FBoundingBox> GetBoundingBoxOfActors(const TArray<AActor*>& Actors);

};
