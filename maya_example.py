import maya.cmds as cmds
import maya.api.OpenMaya as om
import os
import maya.mel as mel

try:
    from pxr import Usd, UsdGeom, Gf, Sdf
except ImportError:
    cmds.error("Could not import pxr modules. Ensure mayaUsdPlugin is loaded.")
import math # Make sure this is at the top of your script, or right here


def unreal_transform(pa, frame=None, is_camera=False):
    """
    Extract a Matrix from a Maya transform and convert it to Unreal Space.
    """
    if frame is not None:
        loc_matrix_list = cmds.getAttr(f"{pa}.worldMatrix[0]", time=frame)
    else:
        loc_matrix_list = cmds.xform(pa, q=True, m=True, ws=True)
        
    loc_matrix = om.MMatrix(loc_matrix_list)
    
    # --- CAMERA OFFSET FIX (MAYA LOCAL SPACE) ---
    if is_camera:

        yaw_mtx = om.MEulerRotation(0.0, math.radians(90.0), 0.0).asMatrix()
        pitch_mtx = om.MEulerRotation(math.radians(-90.0),0.0 , 0.0).asMatrix()

        cam_offset = pitch_mtx*yaw_mtx
        
        # Apply this offset to the camera BEFORE the Unreal coordinate swap
        loc_matrix = cam_offset * loc_matrix
    # --------------------------------------------
    
    rot = om.MMatrix([
        1.0, 0.0, 0.0, 0.0, 
        0.0, 0.0, 1.0, 0.0, 
        0.0, -1.0, 0.0, 0.0, 
        0.0, 0.0, 0.0, 1.0
    ])
    
    unreal_space = om.MMatrix([
        1.0, 0.0, 0.0, 0.0, 
        0.0, 0.0, 1.0, 0.0, 
        0.0, 1.0, 0.0, 0.0, 
        0.0, 0.0, 0.0, 1.0
    ])
    
    ans = unreal_space * (rot * loc_matrix)
    ans = ans * unreal_space
    
    return list(ans)
    

def is_animated(pa, start_frame, end_frame):
    """
    Determine if a transform is animated by sampling attributes.
    """
    tx, ty, tz = set(), set(), set()
    rx, ry, rz = set(), set(), set()
    sx, sy, sz = set(), set(), set()
    
    for i in range(int(start_frame), int(end_frame) + 1, 3):
        t = cmds.getAttr(f"{pa}.translate", time=i)[0]
        r = cmds.getAttr(f"{pa}.rotate", time=i)[0]
        s = cmds.getAttr(f"{pa}.scale", time=i)[0]
        
        tx.add(round(t[0], 4)); ty.add(round(t[1], 4)); tz.add(round(t[2], 4))
        rx.add(round(r[0], 4)); ry.add(round(r[1], 4)); rz.add(round(r[2], 4))
        sx.add(round(s[0], 4)); sy.add(round(s[1], 4)); sz.add(round(s[2], 4))
        
    attr_lists = (tx, ty, tz, rx, ry, rz, sx, sy, sz)
    for att in attr_lists:
        if len(att) > 1:
            return True
            
    return False


def get_unreal_asset_path(maya_obj, safe_name):
    """
    Retrieves the Unreal content directory path. 
    """
    if cmds.attributeQuery('unrealAssetPath', node=maya_obj, exists=True):
        return cmds.getAttr(f"{maya_obj}.unrealAssetPath")
    
    return f"/Game/Assets/{safe_name}.{safe_name}"


def export_skel_anim(target_node, filepath, start_frame, end_frame):
    """
    Exports the animation naturally, avoiding the group1 synthetic root issue.
    """
    cmds.select(target_node, replace=True)
    
    cmds.FBXResetExport()
    
    unit = cmds.currentUnit(q=True, linear=True)
    try:
        unit_conv = mel.eval(f'FBXExportConvertUnitString "{unit}"')
        mel.eval(f'FBXExportScaleFactor {unit_conv}')
    except:
        pass 
        
    mel.eval('FBXExportBakeComplexAnimation -v true')
    mel.eval(f'FBXExportBakeComplexStart -v {start_frame}')
    mel.eval(f'FBXExportBakeComplexEnd -v {end_frame}')
    
    # Export everything naturally under the top node
    mel.eval('FBXExportShapes -v true')
    mel.eval('FBXExportSkins -v true')
    mel.eval('FBXExportInputConnections -v false')
    mel.eval('FBXExportIncludeChildren -v true') 

    cmds.file(filepath, force=True, typ="FBX export", pr=True, es=True, pmt=False)
    
    
def export_selected_to_usd_layout(output_filepath):
    selection = cmds.ls(selection=True, type='transform', long=True)
    if not selection:
        cmds.warning("Please select at least one transform object.")
        return

    start_frame = int(cmds.playbackOptions(q=True, ast=True))
    end_frame = int(cmds.playbackOptions(q=True, aet=True))

    print(f"Building USD Layout Stage with {len(selection)} items (Timeline: {start_frame}-{end_frame})...")

    output_dir = os.path.dirname(output_filepath)
    skel_anim_dir_name = "skel_anim"
    skel_anim_dir = os.path.join(output_dir, skel_anim_dir_name)

    stage = Usd.Stage.CreateInMemory()
    stage.SetStartTimeCode(start_frame)
    stage.SetEndTimeCode(end_frame)
    
    stage.DefinePrim(Sdf.Path("/Root"))
    stage.DefinePrim(Sdf.Path("/Root/StaticMeshes"), "Scope")
    stage.DefinePrim(Sdf.Path("/Root/AnimatedStaticMeshes"), "Scope")
    stage.DefinePrim(Sdf.Path("/Root/SkeletalMeshes"), "Scope")
    stage.DefinePrim(Sdf.Path("/Root/Cameras"), "Scope")

    for maya_obj in selection:
        short_name = maya_obj.split('|')[-1]
        
        # Determine node types
        joints = cmds.listRelatives(maya_obj, allDescendents=True, type='joint', fullPath=True)
        shapes = cmds.listRelatives(maya_obj, shapes=True, fullPath=True) or []
        is_camera = any(cmds.nodeType(s) == 'camera' for s in shapes)
        
        if is_camera:
            # === PURE USD CAMERA EXPORT (BAKED TO WORLD SPACE) ===
            safe_name = short_name.replace(':', '_')
            
            # 1. Define the prim as a native USD Camera instead of an Xform
            prim_path = f"/Root/Cameras/{safe_name}"
            usd_camera = UsdGeom.Camera.Define(stage, Sdf.Path(prim_path))
            prim = usd_camera.GetPrim()
            
            name_attr = prim.CreateAttribute("unrealSceneName", Sdf.ValueTypeNames.String, custom=True)
            name_attr.Set(safe_name)
            
            # Find the actual camera shape node to query lens data
            cam_shapes = cmds.listRelatives(maya_obj, shapes=True, type='camera', fullPath=True)
            cam_shape = cam_shapes[0] if cam_shapes else None
            
            # 2. Setup the transform attribute
            matrix_attr = prim.CreateAttribute("xformOp:transform", Sdf.ValueTypeNames.Matrix4d, custom=False)
            order_attr = prim.CreateAttribute("xformOpOrder", Sdf.ValueTypeNames.TokenArray)
            order_attr.Set(["xformOp:transform"])
            order_attr.SetVariability(Sdf.VariabilityUniform)

            # 3. Setup Camera-specific attributes (Lens and Filmback)
            focal_length_attr = usd_camera.CreateFocalLengthAttr()
            horizontal_aperture_attr = usd_camera.CreateHorizontalApertureAttr()
            vertical_aperture_attr = usd_camera.CreateVerticalApertureAttr()

            # --- STATIC FILMBACK (Extracted once) ---
            if cam_shape:
                # Maya returns inches, USD/Unreal expects mm. Multiply by 25.4.
                # Set without a frame argument so it applies as a static baseline
                h_aperture_inches = cmds.getAttr(f"{cam_shape}.horizontalFilmAperture")
                v_aperture_inches = cmds.getAttr(f"{cam_shape}.verticalFilmAperture")
                horizontal_aperture_attr.Set(h_aperture_inches * 25.4)
                vertical_aperture_attr.Set(v_aperture_inches * 25.4)

            # 4. Loop through the timeline and bake animated attributes per-frame
            for frame in range(start_frame, end_frame + 1):
                # Get WORLD SPACE transform converted to Unreal coordinates
                matrix_list = unreal_transform(maya_obj, frame=frame, is_camera=True)
                gf_matrix = Gf.Matrix4d(*matrix_list)
                matrix_attr.Set(gf_matrix, frame)
                
                if cam_shape:
                    # Focal Length CAN animate (zooms), so we keep it in the loop
                    focal_val = cmds.getAttr(f"{cam_shape}.focalLength", time=frame)
                    focal_length_attr.Set(focal_val, frame)
                    
            # 5. Add custom layout metadata
            name_attr = prim.CreateAttribute("unrealSceneName", Sdf.ValueTypeNames.String, custom=True)
            name_attr.Set(safe_name)
            
            print(f"Exported Baked USD Camera: {prim_path}")

        elif joints:
            # === RIGGED ASSET EXPORT ===
            if ':' in short_name:
                namespace = short_name.split(':')[0]
            else:
                namespace = short_name.replace('_Rig_Root', '')
                
            safe_name = namespace
            
            if not os.path.exists(skel_anim_dir):
                os.makedirs(skel_anim_dir)
                
            skel_filename = f"{safe_name}.fbx" 
            
            abs_skel_path = os.path.join(skel_anim_dir, skel_filename).replace('\\', '/')
            rel_skel_path = f"{skel_anim_dir_name}/{skel_filename}"
            
            export_skel_anim(maya_obj, abs_skel_path, start_frame, end_frame)
            print(f"Exported Skeletal Animation to: {abs_skel_path}")
            
            prim_path = f"/Root/SkeletalMeshes/{safe_name}"
            prim = stage.DefinePrim(Sdf.Path(prim_path), "Xform")
            
            prim.GetReferences().AddReference(rel_skel_path)
            
            asset_path = get_unreal_asset_path(maya_obj, short_name.replace(':', '_'))
            asset_attr = prim.CreateAttribute("unrealAssetPath", Sdf.ValueTypeNames.String, custom=True)
            asset_attr.Set(asset_path)
            
            name_attr = prim.CreateAttribute("unrealSceneName", Sdf.ValueTypeNames.String, custom=True)
            name_attr.Set(safe_name)
            
            matrix_attr = prim.CreateAttribute("xformOp:transform", Sdf.ValueTypeNames.Matrix4d, custom=True)
            matrix_list = unreal_transform(maya_obj)
            gf_matrix = Gf.Matrix4d(*matrix_list)
            matrix_attr.Set(gf_matrix)
            
            order_attr = prim.CreateAttribute("xformOpOrder", Sdf.ValueTypeNames.TokenArray)
            order_attr.Set(["xformOp:transform"])
            order_attr.SetVariability(Sdf.VariabilityUniform)

        else:
            # === STATIC / ANIMATED MESH EXPORT ===
            safe_name = short_name.replace(':', '_')
            animated = is_animated(maya_obj, start_frame, end_frame)
            
            if animated:
                prim_path = f"/Root/AnimatedStaticMeshes/{safe_name}"
            else:
                prim_path = f"/Root/StaticMeshes/{safe_name}"
                
            prim = stage.DefinePrim(Sdf.Path(prim_path), "Xform")
            
            asset_path = get_unreal_asset_path(maya_obj, safe_name)
            asset_attr = prim.CreateAttribute("unrealAssetPath", Sdf.ValueTypeNames.String, custom=True)
            asset_attr.Set(asset_path)
            
            name_attr = prim.CreateAttribute("unrealSceneName", Sdf.ValueTypeNames.String, custom=True)
            name_attr.Set(safe_name)
            
            matrix_attr = prim.CreateAttribute("xformOp:transform", Sdf.ValueTypeNames.Matrix4d, custom=True)
            
            if animated:
                for frame in range(start_frame, end_frame + 1):
                    matrix_list = unreal_transform(maya_obj, frame=frame)
                    gf_matrix = Gf.Matrix4d(*matrix_list)
                    matrix_attr.Set(gf_matrix, frame)
            else:
                matrix_list = unreal_transform(maya_obj)
                gf_matrix = Gf.Matrix4d(*matrix_list)
                matrix_attr.Set(gf_matrix)
                
            order_attr = prim.CreateAttribute("xformOpOrder", Sdf.ValueTypeNames.TokenArray)
            order_attr.Set(["xformOp:transform"])
            order_attr.SetVariability(Sdf.VariabilityUniform)

    clean_output_filepath = output_filepath.replace('\\', '/')
    stage.GetRootLayer().Export(clean_output_filepath)
    
    cmds.select(selection, replace=True)
    print(f"Successfully exported main USD layout to: {clean_output_filepath}")
    
    
    
# --- Execution Example ---
output_path = r"C:\my_stuff\unreal_projects\EditorUI\Plugins\unreal-json-layout-plugin\Saved\maya_export_cam.usda" 
export_selected_to_usd_layout(output_path)