// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshAsset.h"
#include "CoreMinimal.h"
#include "Math/Transform.h" 

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


#include "StaticMeshAsset.generated.h" 

/** Child of MeshAsset Class - Represents Non Moving Static Mesh Loaded From Json */
UCLASS()
class LAYOUTEDITOR_API UStaticMeshAsset : public UMeshAsset
{
	GENERATED_BODY()

public:

	pxr::UsdPrim Prim;
	pxr::UsdStageRefPtr Stage;
	/** Transform of object in scene */
	FTransform Transform;

public:

	/**
	* Initiate the variables of the asset.
	* @param CurrentSequencer - Sequence Actor which will load this asset.
	* @param NewStage - pxr::UsdStageRefPtr  containing animated keyframes for sequencer.
	* @param NewPrim - pxr::UsdPrim containing animated keyframes for sequencer.
	*/
	virtual void Init(ALevelSequenceActor *CurrentSequencer, pxr::UsdStageRefPtr NewStage, pxr::UsdPrim NewPrim);

	/**
	* Create the Actor associated with this asset, add to the sequencer, and set transform.
	*/
	virtual void Load() override;

};
