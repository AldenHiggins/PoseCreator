// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"
#include "PoseableActor.generated.h"

UCLASS()
class POSECREATOR_API APoseableActor : public AActor
{
	GENERATED_BODY()

public:	
	// Sets default values for this actor's properties
	APoseableActor(const FObjectInitializer& ObjectInitializer);

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	// The mannequin visible in game that the user will modify
	//USkeletalMeshComponent *skeletalMesh;
	UPoseableMeshComponent *poseableMesh;

	// The mesh to represent the bones
	UStaticMesh* boneMesh;

	// Reference meshes for the bones
	TArray<UStaticMeshComponent *> boneReferences;
};
