// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"
#include "DataStructures.h"
#include "PoseableActor.generated.h"

UCLASS()
class POSECREATOR_API APoseableActor : public AActor
{
	GENERATED_BODY()

public:	
	// Sets default values for this actor's properties
	APoseableActor(const FObjectInitializer& ObjectInitializer);

	// Reset bone position/rotations to their defaults
	UFUNCTION(BlueprintCallable, Category = "Posing")
	void resetSkeleton();

	// Save out the current pose to an animation sequence
	UFUNCTION(BlueprintCallable, Category = "Posing")
	void saveCurrentPose();

	// Overlapping bone callbacks
	UFUNCTION(BlueprintCallable, Category = "Posing")
	void overlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand);

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void endOverlapBoneReference(UStaticMeshComponent *overlappedBoneInput, UStaticMeshComponent *selectionSphereInput, bool leftHand);

	// Input button callbacks
	UFUNCTION(BlueprintCallable, Category = "Posing")
	void gripPressed(UStaticMeshComponent *selectionSphere, bool leftHand);

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void gripReleased(bool leftHand);

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void triggerPressed(UStaticMeshComponent *selectionSphere, bool leftHand);

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void triggerReleased(bool leftHand);

	UFUNCTION(BlueprintCallable, Category = "Posing")
	void rotateBoneAroundAxis(float rotationRadians);

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

private:

	// Save out an array of bone info for the current state of the skeleton
	TArray<FBoneInfo> saveCurrentBoneState();

	// Change the current bone state to that of the inputted array
	void changeBoneState(TArray<FBoneInfo> newPose);

	// The initial pose of the mannequin
	TArray<FBoneInfo> initialPose;
	
	// The mannequin visible in game that the user will modify
	UPoseableMeshComponent *poseableMesh;

	// The mesh to represent the bones
	UStaticMesh* boneMesh;

	// Reference meshes for the bones
	TArray<UStaticMeshComponent *> boneReferences;

	// Bone info
	TArray<FMeshBoneInfo> meshBoneInfo;

	// Rotation value along the bone's axis, used to rotate the bones in the only axis that you can't by just dragging them around
	float trackpadRotation;

	// Input booleans
	bool rightTriggerBeingPressed;
	bool leftTriggerBeingPressed;
	bool rightGripBeingPressed;
	bool leftGripBeingPressed;

	// Keep track of the initial position of the controller when the grip button is pressed
	FVector rightHandInitialGripPosition;
	FVector initialGripVectorBetweenControllers;
	FRotator initialActorRotation;

	// References to the right/left hand controller's location
	UStaticMeshComponent *rightHandSelectionSphere;
	UStaticMeshComponent *LeftHandSelectionSphere;

	// Whether or not the user is overlapping a bone with one of the controllers
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
