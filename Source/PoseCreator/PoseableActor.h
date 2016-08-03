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

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void overlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand);

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void endOverlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand);

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void gripPressed();

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void gripReleased();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	// The mannequin visible in game that the user will modify
	UPoseableMeshComponent *poseableMesh;

	// The mesh to represent the bones
	UStaticMesh* boneMesh;

	// Reference meshes for the bones
	TArray<UStaticMeshComponent *> boneReferences;

	// Bone info
	TArray<FMeshBoneInfo> meshBoneInfo;

private:

	bool gripBeingPressed;

	bool boneReferenceOverlappingLeft;
	bool boneReferenceOverlappingRight;

	// The starting difference vector between the left and right hand bones
	FVector startingLeftToRightVector;
	FRotator startingBoneRotation;

	// The currently overlapped bone references
	UStaticMeshComponent *overlappedBoneLeftHand;
	UStaticMeshComponent *overlappedBoneRightHand;

	// The name of the current overlapped bones
	FName overlappedBoneNameLeftHand;
	FName overlappedBoneNameRightHand;

	FMeshBoneInfo overlappedBoneRighttHandInfo;
	FName overlappedBoneParentName;

	// The selection sphere currently overlapping bone references
	UStaticMeshComponent *selectionSphereLeftHand;
	UStaticMeshComponent *selectionSphereRightHand;
};
