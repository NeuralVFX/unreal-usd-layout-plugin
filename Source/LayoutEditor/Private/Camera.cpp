// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#include "Camera.h"
#include "Helpers.h"

// Sequencer & Tracks
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"
#include "Runtime/LevelSequence/Public/LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"

// Actors & Components
#include "Engine/World.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"

// USD
#include "pxr/usd/usdGeom/camera.h"

void UCamera::Init(ALevelSequenceActor* CurrentSequencer, pxr::UsdStageRefPtr NewStage, pxr::UsdPrim NewPrim)
{
	pxr::UsdAttribute SceneNameAttr = NewPrim.GetAttribute(pxr::TfToken("unrealSceneName"));
	std::string SceneNameStr;
	SceneNameAttr.Get(&SceneNameStr);

	SceneName = UTF8_TO_TCHAR(SceneNameStr.c_str());
	ContentName = "NA";

	Prim = NewPrim;
	Stage = NewStage;
	Sequencer = CurrentSequencer;
	World = Sequencer->GetWorld();
	Type = "Camera";
}

ACineCameraActor* UCamera::MakeActor()
{
	// Spawn the Actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACineCameraActor* SpawnedActor = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	SpawnedActor->SetActorLabel(SceneName);

	// Fetch USD Camera data
	pxr::UsdGeomCamera UsdCam(Prim);
	if (UsdCam)
	{
		float SensorWidth = 36.0f; // Fallback defaults
		float SensorHeight = 24.0f;

		// Grab the static filmback we baked in Python (already in mm)
		if (UsdCam.GetHorizontalApertureAttr().HasValue())
		{
			UsdCam.GetHorizontalApertureAttr().Get(&SensorWidth);
		}
		if (UsdCam.GetVerticalApertureAttr().HasValue())
		{
			UsdCam.GetVerticalApertureAttr().Get(&SensorHeight);
		}

		UCineCameraComponent* CamComponent = SpawnedActor->GetCineCameraComponent();
		if (CamComponent)
		{
			CamComponent->Filmback.SensorWidth = SensorWidth;
			CamComponent->Filmback.SensorHeight = SensorHeight;
			CamComponent->LensSettings.MinFStop = 0.0f;
			CamComponent->LensSettings.MaxFStop = 1000.0f;
		}
	}

	return SpawnedActor;
}

void UCamera::Load()
{
	if (!IsLoaded())
	{
		// Get Sequencer
		ULevelSequence* Sequence = UHelpers::GetSequence(Sequencer);
		if (Sequence)
		{
			// Spawn actor and cache it for Unload()
			ACineCameraActor* SpawnedActor = MakeActor();
			CachedActor = Cast<AActor>(SpawnedActor);

			// Add to Sequencer
			FGuid Guid;
			Guid = Sequence->MovieScene->AddPossessable(SpawnedActor->GetActorLabel(), SpawnedActor->GetClass());
			Sequence->BindPossessableObject(Guid, *SpawnedActor, SpawnedActor->GetWorld());

			// ==========================================
			// 1. TRANSFORM TRACK (xformOp:transform)
			// ==========================================
			UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Sequence->MovieScene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), Guid));
			UMovieSceneSection* TransformSection = TransformTrack->CreateNewSection();

			pxr::UsdAttribute TransformAttr = Prim.GetAttribute(pxr::TfToken("xformOp:transform"));
			std::vector<double> TimeSamples;
			TransformAttr.GetTimeSamples(&TimeSamples);

			if (TimeSamples.empty())
			{
				UE_LOG(LogTemp, Error, TEXT("Camera xformOp:transform has no time samples on prim: %s"), *FString(Prim.GetPath().GetText()));
				return;
			}

			double StartFrame = TimeSamples.front();
			double EndFrame = TimeSamples.back();

			TransformSection->SetStartFrame(UHelpers::GetFrameNumberTick(Sequencer, StartFrame, false));
			TransformSection->SetEndFrame(UHelpers::GetFrameNumberTick(Sequencer, EndFrame, false));
			TransformTrack->AddSection(*Cast<UMovieScene3DTransformSection>(TransformSection));

			const FMovieSceneChannelProxy& ChannelProxy = TransformSection->GetChannelProxy();
			auto ChannelArray = ChannelProxy.GetChannels<FMovieSceneDoubleChannel>();

			FMovieSceneDoubleChannel* TxChannel = ChannelArray[0];
			FMovieSceneDoubleChannel* TyChannel = ChannelArray[1];
			FMovieSceneDoubleChannel* TzChannel = ChannelArray[2];
			FMovieSceneDoubleChannel* RxChannel = ChannelArray[3];
			FMovieSceneDoubleChannel* RyChannel = ChannelArray[4];
			FMovieSceneDoubleChannel* RzChannel = ChannelArray[5];

			pxr::GfMatrix4d UsdMatrix;
			for (double TimeKey : TimeSamples)
			{
				FFrameNumber Frame = UHelpers::GetFrameNumberTick(Sequencer, TimeKey, false);

				TransformAttr.Get(&UsdMatrix, TimeKey);

				FMatrix UETransformMatrix;
				for (int i = 0; i < 4; ++i)
				{
					for (int j = 0; j < 4; ++j)
					{
						UETransformMatrix.M[i][j] = UsdMatrix[i][j];
					}
				}

				FTransform Transform(UETransformMatrix);
				FVector Translation = Transform.GetTranslation();
				FRotator Rotation = Transform.GetRotation().Rotator();

				TxChannel->AddCubicKey(Frame, Translation.X, ERichCurveTangentMode::RCTM_Auto);
				TyChannel->AddCubicKey(Frame, Translation.Y, ERichCurveTangentMode::RCTM_Auto);
				TzChannel->AddCubicKey(Frame, Translation.Z, ERichCurveTangentMode::RCTM_Auto);
				RxChannel->AddCubicKey(Frame, Rotation.Roll, ERichCurveTangentMode::RCTM_Auto);
				RyChannel->AddCubicKey(Frame, Rotation.Pitch, ERichCurveTangentMode::RCTM_Auto);
				RzChannel->AddCubicKey(Frame, Rotation.Yaw, ERichCurveTangentMode::RCTM_Auto);
			}

			// ==========================================
			// 2. FOCAL LENGTH TRACK 
			// ==========================================
			pxr::UsdGeomCamera UsdCam(Prim);
			pxr::UsdAttribute FocalLengthAttr = UsdCam.GetFocalLengthAttr();

			std::vector<double> FocalTimeSamples;
			FocalLengthAttr.GetTimeSamples(&FocalTimeSamples);

			if (!FocalTimeSamples.empty())
			{
				UMovieSceneFloatTrack* FocalTrack = Cast<UMovieSceneFloatTrack>(Sequence->MovieScene->AddTrack(UMovieSceneFloatTrack::StaticClass(), Guid));

				// Bind directly to the CameraComponent's property via path
				FocalTrack->SetPropertyNameAndPath(TEXT("CurrentFocalLength"), TEXT("CameraComponent.CurrentFocalLength"));

				UMovieSceneSection* FocalSection = FocalTrack->CreateNewSection();
				FocalSection->SetStartFrame(UHelpers::GetFrameNumberTick(Sequencer, FocalTimeSamples.front(), false));
				FocalSection->SetEndFrame(UHelpers::GetFrameNumberTick(Sequencer, FocalTimeSamples.back(), false));
				FocalTrack->AddSection(*FocalSection);

				const FMovieSceneChannelProxy& FocalProxy = FocalSection->GetChannelProxy();
				auto FloatChannels = FocalProxy.GetChannels<FMovieSceneFloatChannel>();
				FMovieSceneFloatChannel* FocalChannel = FloatChannels[0];

				for (double TimeKey : FocalTimeSamples)
				{
					FFrameNumber Frame = UHelpers::GetFrameNumberTick(Sequencer, TimeKey, false);

					// USD Camera Focal Length is typically natively stored as a float
					float FocalVal = 35.0f;
					FocalLengthAttr.Get(&FocalVal, TimeKey);

					FocalChannel->AddCubicKey(Frame, FocalVal, ERichCurveTangentMode::RCTM_Auto);
				}
			}
		}
	}
}


void UCamera::Unload()
{
	if (IsLoaded())
	{


		FGuid Guid = GetGuid();
		FGuid CompGuid = GetCompGuid();

		ULevelSequence* Sequence = UHelpers::GetSequence(Sequencer);
		if (Sequence && Sequence->GetMovieScene())
		{
			UMovieScene* MovieScene = Sequence->GetMovieScene();

			// 1. Unbind and Remove Tracks
			if (CompGuid.IsValid())
			{
				Sequence->UnbindPossessableObjects(CompGuid);
				MovieScene->RemovePossessable(CompGuid);
			}

			if (Guid.IsValid())
			{
				Sequence->UnbindPossessableObjects(Guid);
				MovieScene->RemovePossessable(Guid);
			}

			ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
		}

		Guid.Invalidate();

		// TODO: This part CRASHES
		if (CachedActor.IsValid())
		{
			CachedActor.Get()->Destroy();
			CachedActor.Reset(); // Clear the weak pointer out
		}
	}
}


FGuid UCamera::GetCompGuid()
{
	FGuid ParentGuid = GetGuid();
	ULevelSequence* Sequence = UHelpers::GetSequence(Sequencer);

	if (Sequence && Sequence->GetMovieScene())
	{
		// Find any hidden component possessables attached to our main Camera Actor
		for (int32 i = 0; i < Sequence->GetMovieScene()->GetPossessableCount(); ++i)
		{
			FMovieScenePossessable& Possessable = Sequence->GetMovieScene()->GetPossessable(i);
			if (Possessable.GetParent() == ParentGuid)
			{
				return Possessable.GetGuid();
			}
		}
	}
	return FGuid();
}