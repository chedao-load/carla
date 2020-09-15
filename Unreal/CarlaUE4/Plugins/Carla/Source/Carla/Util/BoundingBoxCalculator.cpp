// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla.h"
#include "Carla/Util/BoundingBoxCalculator.h"

#include "Carla/Traffic/TrafficSignBase.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

#include "Rendering/SkeletalMeshRenderData.h"


namespace crp = carla::rpc;

static FBoundingBox ApplyTransformToBB(
    const FBoundingBox& InBoundingBox,
    const FTransform& Transform)
{
  const FRotator Rotation = Transform.GetRotation().Rotator();
  const FVector Translation = Transform.GetLocation();
  const FVector Scale = Transform.GetScale3D();

  FBoundingBox BoundingBox = InBoundingBox;
  BoundingBox.Origin *= Scale;
  BoundingBox.Origin = Rotation.RotateVector(BoundingBox.Origin) + Translation;
  BoundingBox.Extent *= Scale;
  BoundingBox.Rotation = Rotation;

  return BoundingBox;
}

FBoundingBox UBoundingBoxCalculator::GetActorBoundingBox(const AActor *Actor, uint8 InTagQueried)
{
  if (Actor != nullptr)
  {
    // Vehicle.
    auto Vehicle = Cast<ACarlaWheeledVehicle>(Actor);
    if (Vehicle != nullptr)
    {
      FVector Origin = Vehicle->GetVehicleBoundingBoxTransform().GetTranslation();
      FVector Extent = Vehicle->GetVehicleBoundingBoxExtent();
      return {Origin, Extent};
    }
    // Walker.
    auto Character = Cast<ACharacter>(Actor);
    if (Character != nullptr)
    {
      auto Capsule = Character->GetCapsuleComponent();
      if (Capsule != nullptr)
      {
        const auto Radius = Capsule->GetScaledCapsuleRadius();
        const auto HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        // Characters have the pivot point centered.
        FVector Origin = {0.0f, 0.0f, 0.0f};
        FVector Extent = {Radius, Radius, HalfHeight};
        return {Origin, Extent};
      }
    }
    // Traffic sign.
    auto TrafficSign = Cast<ATrafficSignBase>(Actor);
    if (TrafficSign != nullptr)
    {
      auto TriggerVolume = TrafficSign->GetTriggerVolume();
      if (TriggerVolume != nullptr)
      {
        FVector Origin = TriggerVolume->GetRelativeTransform().GetTranslation();
        FVector Extent = TriggerVolume->GetScaledBoxExtent();
        return {Origin, Extent};
      }
      else
      {
        UE_LOG(LogCarla, Warning, TEXT("Traffic sign missing trigger volume: %s"), *Actor->GetName());
        return {};
      }
    }


  }
  return {};
}

// TODO: update to calculate current animation pose
FBoundingBox UBoundingBoxCalculator::GetSkeletalMeshBoundingBox(const USkeletalMesh* SkeletalMesh)
{
  if(!SkeletalMesh)
  {
    UE_LOG(LogCarla, Error, TEXT("GetSkeletalMeshBoundingBox no SkeletalMesh"));
    return {};
  }

  // Get Vertex postion information from LOD 0 of the Skeletal Mesh
  FSkeletalMeshRenderData* SkeletalMeshRenderData = SkeletalMesh->GetResourceForRendering();
  FSkeletalMeshLODRenderData& LODRenderData = SkeletalMeshRenderData->LODRenderData[0];
  FStaticMeshVertexBuffers& StaticMeshVertexBuffers = LODRenderData.StaticVertexBuffers;
  FPositionVertexBuffer& FPositionVertexBuffer = StaticMeshVertexBuffers.PositionVertexBuffer;
  uint32 NumVertices = FPositionVertexBuffer.GetNumVertices();

  // Look for Skeletal Mesh bounds (vertex perfect)
  FVector MaxVertex(TNumericLimits<float>::Min());
  FVector MinVertex(TNumericLimits<float>::Max());
  for(uint32 i = 0; i < NumVertices; i++)
  {
    FVector& Pos = FPositionVertexBuffer.VertexPosition(i);
    MaxVertex.X = (Pos.X > MaxVertex.X) ? Pos.X : MaxVertex.X;
    MaxVertex.Y = (Pos.Y > MaxVertex.Y) ? Pos.Y : MaxVertex.Y;
    MaxVertex.Z = (Pos.Z > MaxVertex.Z) ? Pos.Z : MaxVertex.Z;
    MinVertex.X = (Pos.X < MinVertex.X) ? Pos.X : MinVertex.X;
    MinVertex.Y = (Pos.Y < MinVertex.Y) ? Pos.Y : MinVertex.Y;
    MinVertex.Z = (Pos.Z < MinVertex.Z) ? Pos.Z : MinVertex.Z;
  }

  // Calculate middle point
  FVector Origin (
    (MaxVertex.X + MinVertex.X) * 0.5f,
    (MaxVertex.Y + MinVertex.Y) * 0.5f,
    (MaxVertex.Z + MinVertex.Z) * 0.5f
  );

  // Calculate box extent
  FVector Extent (
    (MaxVertex.X - MinVertex.X) * 0.5f,
    (MaxVertex.Y - MinVertex.Y) * 0.5f,
    (MaxVertex.Z - MinVertex.Z) * 0.5f
  );

  return {Origin, Extent};
}

FBoundingBox UBoundingBoxCalculator::GetStaticMeshBoundingBox(const UStaticMesh* StaticMesh)
{
  if(!StaticMesh)
  {
    UE_LOG(LogCarla, Error, TEXT("GetStaticMeshBoundingBox no StaticMesh"));
    return {};
  }

  FBox Box = StaticMesh->GetBoundingBox();
  return {Box.GetCenter(), Box.GetExtent()};

}

void UBoundingBoxCalculator::GetISMBoundingBox(
    UInstancedStaticMeshComponent* ISMComp,
    TArray<FBoundingBox>& OutBoundingBox)
{
  if(!ISMComp)
  {
    UE_LOG(LogCarla, Error, TEXT("GetISMBoundingBox no ISMComp"));
    return;
  }

  const UStaticMesh *Mesh = ISMComp->GetStaticMesh();
  const FBoundingBox SMBoundingBox = GetStaticMeshBoundingBox(Mesh);

  if(SMBoundingBox.Extent.IsZero())
  {
    UE_LOG(LogCarla, Error, TEXT("%s has no SM assigned to the ISM"), *ISMComp->GetOwner()->GetName());
    return;
  }

  const TArray<FInstancedStaticMeshInstanceData>& PerInstanceSMData =  ISMComp->PerInstanceSMData;

  for(auto& InstSMIData : PerInstanceSMData)
  {
    const FTransform Transform = FTransform(InstSMIData.Transform);
    FBoundingBox BoundingBox = ApplyTransformToBB(SMBoundingBox, Transform);
    OutBoundingBox.Add(BoundingBox);
  }

}

void UBoundingBoxCalculator::GetBBsOfStaticMeshComponents(
    const TArray<UStaticMeshComponent*>& StaticMeshComps,
    TArray<FBoundingBox>& OutBB,
    uint8 InTagQueried)
{
  crp::CityObjectLabel TagQueried = (crp::CityObjectLabel)InTagQueried;
  bool FilterByTagEnabled = (TagQueried != crp::CityObjectLabel::None);

  for(UStaticMeshComponent* Comp : StaticMeshComps)
  {
    // Filter by tag
    crp::CityObjectLabel Tag = ATagger::GetTagOfTaggedComponent(*Comp);
    if(FilterByTagEnabled && Tag != TagQueried) continue;

    UStaticMesh* StaticMesh = Comp->GetStaticMesh();
    FBoundingBox BoundingBox = GetStaticMeshBoundingBox(StaticMesh);

    if(BoundingBox.Extent.IsZero())
    {
      UE_LOG(LogCarla, Error, TEXT("%s has no SM assigned"), *Comp->GetOwner()->GetName());
    }
    else
    {
      // Component-to-world transform for this component
      const FTransform& CompToWorldTransform = Comp->GetComponentTransform();

      BoundingBox = ApplyTransformToBB(BoundingBox, CompToWorldTransform);
      OutBB.Add(BoundingBox);
    }
  }
}

void UBoundingBoxCalculator::GetBBsOfSkeletalMeshComponents(
    const TArray<USkeletalMeshComponent*>& SkeletalMeshComps,
    TArray<FBoundingBox>& OutBB,
    uint8 InTagQueried)
{
  crp::CityObjectLabel TagQueried = (crp::CityObjectLabel)InTagQueried;
  bool FilterByTagEnabled = (TagQueried != crp::CityObjectLabel::None);

  for(USkeletalMeshComponent* Comp : SkeletalMeshComps)
  {
    // Filter by tag
    crp::CityObjectLabel Tag = ATagger::GetTagOfTaggedComponent(*Comp);
    if(FilterByTagEnabled && Tag != TagQueried) continue;

    USkeletalMesh* SkeletalMesh = Comp->SkeletalMesh;
    FBoundingBox BoundingBox = GetSkeletalMeshBoundingBox(SkeletalMesh);
    if(BoundingBox.Extent.IsZero())
    {
      UE_LOG(LogCarla, Error, TEXT("%s has no SKM assigned"), *Comp->GetOwner()->GetName());
    }
    else
    {
      // Component-to-world transform for this component
      const FTransform& CompToWorldTransform = Comp->GetComponentTransform();
      BoundingBox = ApplyTransformToBB(BoundingBox, CompToWorldTransform);
      OutBB.Add(BoundingBox);
    }
  }
}

// TODO: Add tag to the querry
// TODO: Dynamic vehicle, avoid SM of collision
TArray<FBoundingBox> UBoundingBoxCalculator::GetBoundingBoxOfActors(
  const TArray<AActor*>& Actors,
  uint8 InTagQueried)
{
  TArray<FBoundingBox> Result;
  crp::CityObjectLabel TagQueried = (crp::CityObjectLabel)InTagQueried;
  bool FilterByTagEnabled = (TagQueried != crp::CityObjectLabel::None);

  for(AActor* Actor : Actors)
  {
    FString ClassName = Actor->GetClass()->GetName();
    //UE_LOG(LogCarla, Warning, TEXT("%s %s"), *Actor->GetName() , *ClassName );

    // Avoid the BP_Procedural_Building to avoid duplication with their child actors
    // When improved the BP_Procedural_Building this maybe should be removed
    // Note: We don't use casting here because the base class is a BP and is easier to do it this way,
    //       than getting the UClass of the BP to cast the actor.
    if(ClassName.Contains("BP_Procedural_Bulding")) continue;


    // The vehicle's BP has a low-polystatic mesh for collisions, we should avoid it
    ACarlaWheeledVehicle* Vehicle = Cast<ACarlaWheeledVehicle>(Actor);
    if (Vehicle)
    {
      UActorComponent *ActorComp = Vehicle->GetComponentByClass(USkeletalMeshComponent::StaticClass());
      USkeletalMeshComponent* Comp = Cast<USkeletalMeshComponent>(ActorComp);

      // Filter by tag
      crp::CityObjectLabel Tag = ATagger::GetTagOfTaggedComponent(*Comp);
      if(FilterByTagEnabled && Tag != TagQueried) continue;

      USkeletalMesh* SkeletalMesh = Comp->SkeletalMesh;
      FBoundingBox BoundingBox = GetSkeletalMeshBoundingBox(SkeletalMesh);
      if(BoundingBox.Extent.IsZero())
      {
        UE_LOG(LogCarla, Error, TEXT("%s has no SKM assigned"), *Actor->GetName());
      }
      else
      {
        // Component-to-world transform for this component
        const FTransform& CompToWorldTransform = Comp->GetComponentTransform();
        BoundingBox = ApplyTransformToBB(BoundingBox, CompToWorldTransform);
        Result.Add(BoundingBox);
      }
      continue;
    }

    // Pedestrians, we just use the capsule component at the moment.
    ACharacter* Character = Cast<ACharacter>(Actor);
    if (Character)
    {
      UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
      if (Capsule)
      {
        const float Radius = Capsule->GetScaledCapsuleRadius();
        const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        FBoundingBox BoundingBox;
        // Characters have the pivot point centered.
        BoundingBox.Origin = {0.0f, 0.0f, 0.0f};
        BoundingBox.Extent = {Radius, Radius, HalfHeight};
        // Component-to-world transform for this component
        const FTransform& CompToWorldTransform = Capsule->GetComponentTransform();
        BoundingBox = ApplyTransformToBB(BoundingBox, CompToWorldTransform);
        Result.Add(BoundingBox);
        continue;
      }
    }

    // Calculate FBoundingBox of SM
    TArray<UStaticMeshComponent*> StaticMeshComps;
    Actor->GetComponents<UStaticMeshComponent>(StaticMeshComps);
    GetBBsOfStaticMeshComponents(StaticMeshComps, Result, InTagQueried);

    // Calculate FBoundingBox of SK_M
    TArray<USkeletalMeshComponent*> SkeletalMeshComps;
    Actor->GetComponents<USkeletalMeshComponent>(SkeletalMeshComps);
    GetBBsOfSkeletalMeshComponents(SkeletalMeshComps, Result, InTagQueried);

    // Calculate FBoundingBox of ISM
    TArray<UInstancedStaticMeshComponent *> ISMComps;
    Actor->GetComponents<UInstancedStaticMeshComponent>(ISMComps);
    for(UInstancedStaticMeshComponent* Comp: ISMComps)
    {
      // Filter by tag
      crp::CityObjectLabel Tag = ATagger::GetTagOfTaggedComponent(*Comp);
      if(FilterByTagEnabled && Tag != TagQueried) continue;

      GetISMBoundingBox(Comp, Result);
    }

  }

  return Result;
}
