// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"


// OpenUSD Includes
#include "USDIncludesStart.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/vt/array.h"
#include "USDIncludesEnd.h"

#include "USDLayout.generated.h"

class AStaticMeshActor;
class ALevelSequenceActor;

UCLASS()
class LAYOUTEDITOR_API UUSDLayout : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Write USD file describing layout and animation of the scene.
	 * @param USDFilePath - File path for the USD file.
	 * @param ActorArray - Array of actors to be written into the USD stage.
	 * @param Sequencer - Sequencer containing any keyframes to be stored as time samples.
	 * @return Whether operation is successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "USD")
	static bool WriteUSDLayoutData(FString USDFilePath, TArray<AStaticMeshActor*> ActorArray, ALevelSequenceActor* Sequencer);

	/**
	 * Read USD file describing layout and animation of the scene.
	 * @param USDFilePath - File path for the USD file.
	 * @return Whether operation is successful.
	 */
	static pxr::UsdStageRefPtr ReadUSDLayoutData(FString USDFilePath);


	/**
	 * Create pxr::UsdPrim with Actor layout information.
	 * @param Actor - Actor in scene.
	 * @param Stage - pxr::UsdStageRefPtr 
	 * @return pxr::UsdPrim containing object transfom.
	 */
	static pxr::UsdPrim MakeStaticMesh(AStaticMeshActor* Actor, pxr::UsdStageRefPtr Stage);

	/**
	 * Create pxr::UsdPrim with Actor animation information.
	 * @param Sequencer - Sequence Actor with objects animation.
	 * @param Actor - Actor in scene.
	 * @param Guid - Guid of object in supplied sequencer.
	 * @param Stage - pxr::UsdStageRefPtr 
	 * @return pxr::UsdPrim containing objects animation curves.
	 */
	static pxr::UsdPrim MakeAnimatedStaticMesh(class ALevelSequenceActor* Sequencer, AStaticMeshActor* Actor, FGuid Guid, pxr::UsdStageRefPtr Stage);

};