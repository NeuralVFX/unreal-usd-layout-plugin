
// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.
#include "StaticMeshAsset.h"
#include "Helpers.h"
#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"
#include "Runtime/LevelSequence/Public/LevelSequence.h"
#include "Engine/StaticMeshActor.h"


void UStaticMeshAsset::Init(ALevelSequenceActor* CurrentSequencer, pxr::UsdStageRefPtr NewStage, pxr::UsdPrim NewPrim)
{

	// Check the name or path of the child
	pxr::UsdAttribute AssetPathAttr = NewPrim.GetAttribute(pxr::TfToken("unrealAssetPath"));
	std::string AssetPathStr;
	AssetPathAttr.Get(&AssetPathStr);

	pxr::UsdAttribute SceneNameAttr = NewPrim.GetAttribute(pxr::TfToken("unrealSceneName"));
	std::string SceneNameStr;
	SceneNameAttr.Get(&SceneNameStr);

	pxr::UsdAttribute TransformAttr = NewPrim.GetAttribute(pxr::TfToken("xformOp:transform"));
	pxr::GfMatrix4d UsdMatrix;
	TransformAttr.Get(&UsdMatrix);

	FMatrix UETransformMatrix;
	UETransformMatrix.M[0][0] = UsdMatrix[0][0];
	UETransformMatrix.M[0][1] = UsdMatrix[0][1];
	UETransformMatrix.M[0][2] = UsdMatrix[0][2];
	UETransformMatrix.M[0][3] = UsdMatrix[0][3];

	UETransformMatrix.M[1][0] = UsdMatrix[1][0];
	UETransformMatrix.M[1][1] = UsdMatrix[1][1];
	UETransformMatrix.M[1][2] = UsdMatrix[1][2];
	UETransformMatrix.M[1][3] = UsdMatrix[1][3];

	UETransformMatrix.M[2][0] = UsdMatrix[2][0];
	UETransformMatrix.M[2][1] = UsdMatrix[2][1];
	UETransformMatrix.M[2][2] = UsdMatrix[2][2];
	UETransformMatrix.M[2][3] = UsdMatrix[2][3];

	UETransformMatrix.M[3][0] = UsdMatrix[3][0];
	UETransformMatrix.M[3][1] = UsdMatrix[3][1];
	UETransformMatrix.M[3][2] = UsdMatrix[3][2];
	UETransformMatrix.M[3][3] = UsdMatrix[3][3];

	FString NewSceneName = UTF8_TO_TCHAR(SceneNameStr.c_str());
	FString NewContentName = UTF8_TO_TCHAR(AssetPathStr.c_str());
	FTransform NewTransform(UETransformMatrix);

	Stage = NewStage;
	Prim = NewPrim;
	SceneName = NewSceneName;
	ContentName = NewContentName;
	Transform = NewTransform;
	Sequencer = CurrentSequencer;
	World = Sequencer->GetWorld();
	Type = "StaticMesh";

	CutName = UHelpers::MakePrettyContentName(ContentName);
}


void UStaticMeshAsset::Load()
{
	if (!IsLoaded())
	{
		// Add to Sequencer
		ULevelSequence * Sequence = UHelpers::GetSequence(Sequencer);
		if (Sequence)
		{
			// Spawn Actor
			AStaticMeshActor*SpawnedActor = MakeActor();

			CachedActor = Cast<AActor>(SpawnedActor);

			FGuid Guid;
			Guid = Sequence->MovieScene->AddPossessable(SpawnedActor->GetActorLabel(), SpawnedActor->GetClass());
			Sequence->BindPossessableObject(Guid, *SpawnedActor, SpawnedActor->GetWorld());

			// Set actor transform
			SpawnedActor->SetActorTransform(Transform);
		}
	}
}
