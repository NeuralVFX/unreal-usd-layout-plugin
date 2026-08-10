// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshAsset.h"

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

#include "AnimatedMeshAsset.generated.h"

/** Child of MeshAsset Class - Represents Non Moving Static Mesh Loaded From Json */
UCLASS()
class LAYOUTEDITOR_API UAnimatedMeshAsset : public UMeshAsset
{
	GENERATED_BODY()

public:

	/** Animated transforms stored in a struct */
	pxr::UsdPrim Prim;
	pxr::UsdStageRefPtr Stage;

public:

	/**
	* Initiate the variables of the asset.
	* @param CurrentSequencer - Sequence Actor which will load this asset.
	* @param NewStage - pxr::UsdStageRefPtr  containing animated keyframes for sequencer.
	* @param NewPrim - pxr::UsdPrim containing animated keyframes for sequencer.
	*/
	virtual void Init(ALevelSequenceActor *CurrentSequencer, pxr::UsdStageRefPtr NewStage, pxr::UsdPrim NewPrim);

	/**
	* Create the Actor associated with this asset, add to the sequencer, and add keys to sequencer.
	*/
	virtual void Load() override;
	
};
