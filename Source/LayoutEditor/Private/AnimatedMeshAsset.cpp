// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#include "AnimatedMeshAsset.h"
#include "Helpers.h"
#include "MovieSceneTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieScene.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/ObjectLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h" 
#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"
#include "Runtime/LevelSequence/Public/LevelSequence.h"
#include "Tracks/MovieScene3DTransformTrack.h"


void UAnimatedMeshAsset::Init(ALevelSequenceActor* CurrentSequencer, pxr::UsdStageRefPtr NewStage, pxr::UsdPrim NewPrim)
{

	// Check the name or path of the child
	pxr::UsdAttribute AssetPathAttr = NewPrim.GetAttribute(pxr::TfToken("unrealAssetPath"));
	std::string AssetPathStr;
	AssetPathAttr.Get(&AssetPathStr);

	pxr::UsdAttribute SceneNameAttr = NewPrim.GetAttribute(pxr::TfToken("unrealSceneName"));
	std::string SceneNameStr;
	SceneNameAttr.Get(&SceneNameStr);

	FString NewSceneName = UTF8_TO_TCHAR(SceneNameStr.c_str());
	FString NewContentName = UTF8_TO_TCHAR(AssetPathStr.c_str());

	SceneName = NewSceneName;
	ContentName = NewContentName;
	Prim = NewPrim;
	Stage = NewStage;
	Sequencer = CurrentSequencer;
	World = Sequencer->GetWorld();
	Type = "Animated StaticMesh";

	CutName = UHelpers::MakePrettyContentName(ContentName);
}


void UAnimatedMeshAsset::Load()
{
	if (!IsLoaded())
	{
		// Add to Sequencer
		ULevelSequence* Sequence = UHelpers::GetSequence(Sequencer);
		if (Sequence)
		{
			// Spawn actor
			AStaticMeshActor* SpawnedActor = MakeActor();
			
			FGuid Guid;
			Guid = Sequence->MovieScene->AddPossessable(SpawnedActor->GetActorLabel(), SpawnedActor->GetClass());
			Sequence->BindPossessableObject(Guid, *SpawnedActor, SpawnedActor->GetWorld());

			// Add track and section to sequencer
			UMovieScene3DTransformTrack* NewTrack = Cast<UMovieScene3DTransformTrack>(Sequence->MovieScene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), Guid));
			UMovieSceneSection* Section = NewTrack->CreateNewSection();

			// Check that keyframes exist
			pxr::UsdAttribute TransformAttr = Prim.GetAttribute(pxr::TfToken("xformOp:transform"));
			pxr::GfMatrix4d UsdMatrix;
			
			std::vector<double> TimeSamples;
			TransformAttr.GetTimeSamples(&TimeSamples);

			if (TimeSamples.empty())
			{
				UE_LOG(LogTemp, Error,
					TEXT("xformOp:transform has no time samples on prim: %s"),
					*FString(Prim.GetPath().GetText()));
				return;
			}


			double StartFrame = TimeSamples.front();
			double EndFrame = TimeSamples.back();


			Section->SetStartFrame(UHelpers::GetFrameNumberTick(Sequencer, StartFrame, false));
			Section->SetEndFrame(UHelpers::GetFrameNumberTick(Sequencer, EndFrame, false));

			NewTrack->AddSection(*Cast<UMovieScene3DTransformSection>(Section));

			// Fetch keyframe object from section (UE5 uses DoubleChannels for transforms)
			const FMovieSceneChannelProxy& channelProxy = Section->GetChannelProxy();
			auto ChannelArray = channelProxy.GetChannels<FMovieSceneDoubleChannel>();

			FMovieSceneDoubleChannel* TxChannel = ChannelArray[0];
			FMovieSceneDoubleChannel* TyChannel = ChannelArray[1];
			FMovieSceneDoubleChannel* TzChannel = ChannelArray[2];
			FMovieSceneDoubleChannel* RxChannel = ChannelArray[3];
			FMovieSceneDoubleChannel* RyChannel = ChannelArray[4];
			FMovieSceneDoubleChannel* RzChannel = ChannelArray[5];
			FMovieSceneDoubleChannel* SxChannel = ChannelArray[6];
			FMovieSceneDoubleChannel* SyChannel = ChannelArray[7];
			FMovieSceneDoubleChannel* SzChannel = ChannelArray[8];


			for (double TimeKey : TimeSamples)
			{

				    FFrameNumber Frame = UHelpers::GetFrameNumberTick(Sequencer, TimeKey, false);

					TransformAttr.Get(&UsdMatrix,TimeKey);

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

					FTransform Transform(UETransformMatrix);

					// Extract components
					FVector Translation = Transform.GetTranslation();
					FRotator Rotation = Transform.GetRotation().Rotator(); // Or Transform.GetRotation() for FQuat
					FVector Scale = Transform.GetScale3D();

					TxChannel->AddCubicKey(Frame, Translation.X, ERichCurveTangentMode::RCTM_Auto);
					TyChannel->AddCubicKey(Frame, Translation.Y, ERichCurveTangentMode::RCTM_Auto);
					TzChannel->AddCubicKey(Frame, Translation.Z, ERichCurveTangentMode::RCTM_Auto);

					RxChannel->AddCubicKey(Frame, Rotation.Roll, ERichCurveTangentMode::RCTM_Auto);
					RyChannel->AddCubicKey(Frame, Rotation.Pitch, ERichCurveTangentMode::RCTM_Auto);
					RzChannel->AddCubicKey(Frame, Rotation.Yaw, ERichCurveTangentMode::RCTM_Auto);

					SxChannel->AddCubicKey(Frame, Scale.X, ERichCurveTangentMode::RCTM_Auto);
					SyChannel->AddCubicKey(Frame, Scale.Y, ERichCurveTangentMode::RCTM_Auto);
					SzChannel->AddCubicKey(Frame, Scale.Z, ERichCurveTangentMode::RCTM_Auto);
			}
			

		}

		
	}
}
