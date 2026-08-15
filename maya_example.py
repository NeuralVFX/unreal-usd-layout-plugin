# -*- coding: utf-8 -*-

import os
import math
import maya.cmds as cmds
import maya.mel as mel
import maya.api.OpenMaya as om
from PySide6 import QtCore, QtGui, QtWidgets

# Attempt to load USD modules
try:
    from pxr import Usd, UsdGeom, Gf, Sdf
except ImportError:
    cmds.error("Could not import pxr modules. Ensure mayaUsdPlugin is loaded.")


# ==========================================
# USD EXPORT & MATH LOGIC
# ==========================================
def unreal_transform(pa, frame=None, is_camera=False):
    if frame is not None:
        loc_matrix_list = cmds.getAttr(f"{pa}.worldMatrix[0]", time=frame)
    else:
        loc_matrix_list = cmds.xform(pa, q=True, m=True, ws=True)

    loc_matrix = om.MMatrix(loc_matrix_list)

    if is_camera:
        yaw_mtx = om.MEulerRotation(0.0, math.radians(90.0), 0.0).asMatrix()
        pitch_mtx = om.MEulerRotation(math.radians(-90.0), 0.0, 0.0).asMatrix()
        cam_offset = pitch_mtx * yaw_mtx
        loc_matrix = cam_offset * loc_matrix

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

    for att in (tx, ty, tz, rx, ry, rz, sx, sy, sz):
        if len(att) > 1:
            return True
    return False


def get_unreal_asset_path(maya_obj, safe_name):
    if cmds.attributeQuery('unrealAssetPath', node=maya_obj, exists=True):
        return cmds.getAttr(f"{maya_obj}.unrealAssetPath")
    return f"/Game/Assets/{safe_name}.{safe_name}"


def export_skel_anim(target_node, filepath, start_frame, end_frame):
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
    mel.eval('FBXExportShapes -v true')
    mel.eval('FBXExportSkins -v true')
    mel.eval('FBXExportInputConnections -v false')
    mel.eval('FBXExportIncludeChildren -v true') 

    cmds.file(filepath, force=True, typ="FBX export", pr=True, es=True, pmt=False)


def get_roots_from_reference_set(set_name):
    roots = []
    if cmds.objExists(set_name):
        ref_nodes = cmds.sets(set_name, q=True) or []
        for ref_node in ref_nodes:
            try:
                ref_contents = cmds.referenceQuery(ref_node, nodes=True) or []
                assemblies = cmds.ls(ref_contents, assemblies=True, long=True)
                roots.extend(assemblies)
            except RuntimeError:
                pass
    return roots


def export_sets_to_usd_layout(output_filepath):
    cam_roots = cmds.ls(cmds.sets('cam_exp', q=True), long=True) if cmds.objExists('cam_exp') else []
    rig_roots = get_roots_from_reference_set('rig_exp')
    mdl_roots = get_roots_from_reference_set('mdl_exp')
    
    total_items = len(cam_roots) + len(rig_roots) + len(mdl_roots)

    if total_items == 0:
        cmds.warning("No exportable assets found in the publish sets.")
        raise ValueError("No exportable assets tagged.")

    start_frame = int(cmds.playbackOptions(q=True, ast=True))
    end_frame = int(cmds.playbackOptions(q=True, aet=True))

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

    # PROCESS CAMERAS
    for maya_obj in cam_roots:
        short_name = maya_obj.split('|')[-1]
        safe_name = short_name.replace(':', '_')
        
        prim_path = f"/Root/Cameras/{safe_name}"
        usd_camera = UsdGeom.Camera.Define(stage, Sdf.Path(prim_path))
        prim = usd_camera.GetPrim()
        
        name_attr = prim.CreateAttribute("unrealSceneName", Sdf.ValueTypeNames.String, custom=True)
        name_attr.Set(safe_name)
        
        cam_shapes = cmds.listRelatives(maya_obj, shapes=True, type='camera', fullPath=True)
        cam_shape = cam_shapes[0] if cam_shapes else None
        
        matrix_attr = prim.CreateAttribute("xformOp:transform", Sdf.ValueTypeNames.Matrix4d, custom=False)
        order_attr = prim.CreateAttribute("xformOpOrder", Sdf.ValueTypeNames.TokenArray)
        order_attr.Set(["xformOp:transform"])
        order_attr.SetVariability(Sdf.VariabilityUniform)

        focal_length_attr = usd_camera.CreateFocalLengthAttr()
        horizontal_aperture_attr = usd_camera.CreateHorizontalApertureAttr()
        vertical_aperture_attr = usd_camera.CreateVerticalApertureAttr()

        if cam_shape:
            h_aperture_inches = cmds.getAttr(f"{cam_shape}.horizontalFilmAperture")
            v_aperture_inches = cmds.getAttr(f"{cam_shape}.verticalFilmAperture")
            horizontal_aperture_attr.Set(h_aperture_inches * 25.4)
            vertical_aperture_attr.Set(v_aperture_inches * 25.4)

        for frame in range(start_frame, end_frame + 1):
            matrix_list = unreal_transform(maya_obj, frame=frame, is_camera=True)
            gf_matrix = Gf.Matrix4d(*matrix_list)
            matrix_attr.Set(gf_matrix, frame)
            if cam_shape:
                focal_val = cmds.getAttr(f"{cam_shape}.focalLength", time=frame)
                focal_length_attr.Set(focal_val, frame)

    # PROCESS RIGS
    for maya_obj in rig_roots:
        short_name = maya_obj.split('|')[-1]
        namespace = short_name.split(':')[0] if ':' in short_name else short_name.replace('_Rig_Root', '')
        safe_name = namespace
        
        if not os.path.exists(skel_anim_dir):
            os.makedirs(skel_anim_dir)
            
        skel_filename = f"{safe_name}.fbx" 
        abs_skel_path = os.path.join(skel_anim_dir, skel_filename).replace('\\', '/')
        rel_skel_path = f"{skel_anim_dir_name}/{skel_filename}"
        
        export_skel_anim(maya_obj, abs_skel_path, start_frame, end_frame)
        
        prim_path = f"/Root/SkeletalMeshes/{safe_name}"
        prim = stage.DefinePrim(Sdf.Path(prim_path), "Xform")
        prim.GetReferences().AddReference(rel_skel_path)
        
        asset_path = get_unreal_asset_path(maya_obj, short_name.replace(':', '_'))
        prim.CreateAttribute("unrealAssetPath", Sdf.ValueTypeNames.String, custom=True).Set(asset_path)
        prim.CreateAttribute("unrealSceneName", Sdf.ValueTypeNames.String, custom=True).Set(safe_name)
        
        matrix_attr = prim.CreateAttribute("xformOp:transform", Sdf.ValueTypeNames.Matrix4d, custom=True)
        matrix_attr.Set(Gf.Matrix4d(*unreal_transform(maya_obj)))
        
        order_attr = prim.CreateAttribute("xformOpOrder", Sdf.ValueTypeNames.TokenArray)
        order_attr.Set(["xformOp:transform"])
        order_attr.SetVariability(Sdf.VariabilityUniform)

    # PROCESS STATIC MESHES
    for maya_obj in mdl_roots:
        short_name = maya_obj.split('|')[-1]
        safe_name = short_name.replace(':', '_')
        animated = is_animated(maya_obj, start_frame, end_frame)
        
        prim_path = f"/Root/AnimatedStaticMeshes/{safe_name}" if animated else f"/Root/StaticMeshes/{safe_name}"
        prim = stage.DefinePrim(Sdf.Path(prim_path), "Xform")
        
        asset_path = get_unreal_asset_path(maya_obj, safe_name)
        prim.CreateAttribute("unrealAssetPath", Sdf.ValueTypeNames.String, custom=True).Set(asset_path)
        prim.CreateAttribute("unrealSceneName", Sdf.ValueTypeNames.String, custom=True).Set(safe_name)
        
        matrix_attr = prim.CreateAttribute("xformOp:transform", Sdf.ValueTypeNames.Matrix4d, custom=True)
        
        if animated:
            for frame in range(start_frame, end_frame + 1):
                matrix_attr.Set(Gf.Matrix4d(*unreal_transform(maya_obj, frame=frame)), frame)
        else:
            matrix_attr.Set(Gf.Matrix4d(*unreal_transform(maya_obj)))
            
        order_attr = prim.CreateAttribute("xformOpOrder", Sdf.ValueTypeNames.TokenArray)
        order_attr.Set(["xformOp:transform"])
        order_attr.SetVariability(Sdf.VariabilityUniform)

    clean_output_filepath = output_filepath.replace('\\', '/')
    stage.GetRootLayer().Export(clean_output_filepath)
    
    all_processed_nodes = cam_roots + rig_roots + mdl_roots
    if all_processed_nodes:
        cmds.select(all_processed_nodes, replace=True)


# ==========================================
# STANDARD UI SETUP FUNCTIONS
# ==========================================

def get_maya_main_window():
    for widget in QtWidgets.QApplication.topLevelWidgets():
        if widget.objectName() == "MayaWindow":
            return widget
    return None
    
    
def apply_standard_ui_setup(dialog):
    dialog.setWindowFlags(QtCore.Qt.Window)
    dialog.setAttribute(QtCore.Qt.WA_DeleteOnClose)
    
    dark_style = """
        QDialog { 
            background-color: #262626; 
        }
        QLabel { 
            color: #e0e0e0; 
            font-weight: bold; 
        }
        
        /* --- UPDATED LIST WIDGET STYLING --- */
        QListWidget {
            background-color: #555555; 
            border: none;          /* Removed the black outline */
            border-radius: 5px;    /* Increased to 5px to match Unreal */
            font-weight: 600; 
            padding: 2px;          /* Gives a tiny bit of inner spacing */
        }
        /* ----------------------------------- */
        
        /* Left Side Lists: Black Text */
        QListWidget#listWidget_cams,
        QListWidget#listWidget_mdlRefs,
        QListWidget#listWidget_rigRefs {
            color: #000000;
        }

        /* Right Side Lists: Neon Green Text */
        QListWidget#listWidget_taggedCams,
        QListWidget#listWidget_taggedMdlRefs,
        QListWidget#listWidget_taggedRigRefs {
            color: #00ff00;
        }

        /* Button Styling for ALL buttons */
        QPushButton {
            background-color: #333333; 
            color: #e0e0e0;
            border: 1px solid #000000; 
            border-radius: 2px; 
            padding: 5px;
        }
        QPushButton:hover { 
            background-color: #444444; 
        }
        QPushButton:pressed { 
            background-color: #1a1a1a; 
        }
    """
    dialog.setStyleSheet(dark_style)
    
    
maya_organizer_ui_instance = None


# ==========================================
# GUI CLASS DEFINITIONS
# ==========================================
class Ui_Dialog(object):
    def setupUi(self, Dialog):
        Dialog.setObjectName("Dialog")
        Dialog.resize(480, 580)
        self.gridLayout = QtWidgets.QGridLayout(Dialog)
        self.gridLayout.setObjectName("gridLayout")
        
        # Force columns 0 and 2 to expand equally, while keeping column 1 (the arrows) tight
        self.gridLayout.setColumnStretch(0, 1)
        self.gridLayout.setColumnStretch(1, 0)
        self.gridLayout.setColumnStretch(2, 1)
        
        # --- TOP ROW: Publish Button ---
        self.pushButton_publish = QtWidgets.QPushButton(Dialog)
        self.pushButton_publish.setObjectName("pushButton_publish")
        # Make the publish button span all 3 columns
        self.gridLayout.addWidget(self.pushButton_publish, 0, 0, 1, 3)
        
        # ==========================================
        # CAMERAS SECTION
        # ==========================================
        self.label_7 = QtWidgets.QLabel(Dialog)
        self.label_7.setObjectName("label_7")
        self.gridLayout.addWidget(self.label_7, 1, 0, 1, 1)
        
        self.label_8 = QtWidgets.QLabel(Dialog)
        self.label_8.setObjectName("label_8")
        self.gridLayout.addWidget(self.label_8, 1, 2, 1, 1)
        
        self.listWidget_cams = QtWidgets.QListWidget(Dialog)
        self.listWidget_cams.setObjectName("listWidget_cams")
        self.gridLayout.addWidget(self.listWidget_cams, 2, 0, 1, 1)
        
        self.listWidget_taggedCams = QtWidgets.QListWidget(Dialog)
        self.listWidget_taggedCams.setObjectName("listWidget_taggedCams")
        self.gridLayout.addWidget(self.listWidget_taggedCams, 2, 2, 1, 1)
        
        # Middle Arrows for Cams
        self.verticalLayout_cams = QtWidgets.QVBoxLayout()
        self.verticalLayout_cams.addItem(QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding))
        
        self.pushButton_addCam = QtWidgets.QPushButton(Dialog)
        self.pushButton_addCam.setObjectName("pushButton_addCam")
        self.pushButton_addCam.setFixedSize(30, 30)
        self.verticalLayout_cams.addWidget(self.pushButton_addCam)
        
        self.pushButton_removeCam = QtWidgets.QPushButton(Dialog)
        self.pushButton_removeCam.setObjectName("pushButton_removeCam")
        self.pushButton_removeCam.setFixedSize(30, 30)
        self.verticalLayout_cams.addWidget(self.pushButton_removeCam)
        
        self.verticalLayout_cams.addItem(QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding))
        self.gridLayout.addLayout(self.verticalLayout_cams, 2, 1, 1, 1)
        
        # ==========================================
        # MDL REFERENCES SECTION
        # ==========================================
        self.label_2 = QtWidgets.QLabel(Dialog)
        self.label_2.setObjectName("label_2")
        self.gridLayout.addWidget(self.label_2, 3, 0, 1, 1)
        
        self.label_3 = QtWidgets.QLabel(Dialog)
        self.label_3.setObjectName("label_3")
        self.gridLayout.addWidget(self.label_3, 3, 2, 1, 1)
        
        self.listWidget_mdlRefs = QtWidgets.QListWidget(Dialog)
        self.listWidget_mdlRefs.setObjectName("listWidget_mdlRefs")
        self.gridLayout.addWidget(self.listWidget_mdlRefs, 4, 0, 1, 1)
        
        self.listWidget_taggedMdlRefs = QtWidgets.QListWidget(Dialog)
        self.listWidget_taggedMdlRefs.setObjectName("listWidget_taggedMdlRefs")
        self.gridLayout.addWidget(self.listWidget_taggedMdlRefs, 4, 2, 1, 1)
        
        # Middle Arrows for MDLs
        self.verticalLayout_mdl = QtWidgets.QVBoxLayout()
        self.verticalLayout_mdl.addItem(QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding))
        
        self.pushButton_addMdlRef = QtWidgets.QPushButton(Dialog)
        self.pushButton_addMdlRef.setObjectName("pushButton_addMdlRef")
        self.pushButton_addMdlRef.setFixedSize(30, 30)
        self.verticalLayout_mdl.addWidget(self.pushButton_addMdlRef)
        
        self.pushButton_removeMdlRef = QtWidgets.QPushButton(Dialog)
        self.pushButton_removeMdlRef.setObjectName("pushButton_removeMdlRef")
        self.pushButton_removeMdlRef.setFixedSize(30, 30)
        self.verticalLayout_mdl.addWidget(self.pushButton_removeMdlRef)
        
        self.verticalLayout_mdl.addItem(QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding))
        self.gridLayout.addLayout(self.verticalLayout_mdl, 4, 1, 1, 1)
        
        # ==========================================
        # RIG REFERENCES SECTION
        # ==========================================
        self.label_5 = QtWidgets.QLabel(Dialog)
        self.label_5.setObjectName("label_5")
        self.gridLayout.addWidget(self.label_5, 5, 0, 1, 1)
        
        self.label_6 = QtWidgets.QLabel(Dialog)
        self.label_6.setObjectName("label_6")
        self.gridLayout.addWidget(self.label_6, 5, 2, 1, 1)
        
        self.listWidget_rigRefs = QtWidgets.QListWidget(Dialog)
        self.listWidget_rigRefs.setObjectName("listWidget_rigRefs")
        self.gridLayout.addWidget(self.listWidget_rigRefs, 6, 0, 1, 1)
        
        self.listWidget_taggedRigRefs = QtWidgets.QListWidget(Dialog)
        self.listWidget_taggedRigRefs.setObjectName("listWidget_taggedRigRefs")
        self.gridLayout.addWidget(self.listWidget_taggedRigRefs, 6, 2, 1, 1)
        
        # Middle Arrows for RIGs
        self.verticalLayout_rig = QtWidgets.QVBoxLayout()
        self.verticalLayout_rig.addItem(QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding))
        
        self.pushButton_addRigRef = QtWidgets.QPushButton(Dialog)
        self.pushButton_addRigRef.setObjectName("pushButton_addRigRef")
        self.pushButton_addRigRef.setFixedSize(30, 30)
        self.verticalLayout_rig.addWidget(self.pushButton_addRigRef)
        
        self.pushButton_removeRigRef = QtWidgets.QPushButton(Dialog)
        self.pushButton_removeRigRef.setObjectName("pushButton_removeRigRef")
        self.pushButton_removeRigRef.setFixedSize(30, 30)
        self.verticalLayout_rig.addWidget(self.pushButton_removeRigRef)
        
        self.verticalLayout_rig.addItem(QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding))
        self.gridLayout.addLayout(self.verticalLayout_rig, 6, 1, 1, 1)

        self.retranslateUi(Dialog)
        QtCore.QMetaObject.connectSlotsByName(Dialog)

    def retranslateUi(self, Dialog):
        _translate = QtCore.QCoreApplication.translate
        Dialog.setWindowTitle(_translate("Dialog", "USD Publish Organizer"))
        
        self.pushButton_publish.setText(_translate("Dialog", "Publish USD"))
        
        self.label_7.setText(_translate("Dialog", "Exportable Cameras:"))
        self.label_8.setText(_translate("Dialog", "Tagged For Export:"))
        
        self.label_2.setText(_translate("Dialog", "Exportable Static Meshes:"))
        self.label_3.setText(_translate("Dialog", "Tagged For Export:"))
        
        self.label_5.setText(_translate("Dialog", "Exportable Skeletal Meshes:"))
        self.label_6.setText(_translate("Dialog", "Tagged For Export:"))

        # Set the text of the buttons to arrows instead of full phrases
        # ▶ (Right pointing triangle) pushes items to the "Tagged" list
        # ◀ (Left pointing triangle) removes them back to the "Exportable" list
        self.pushButton_addCam.setText(_translate("Dialog", "▶"))
        self.pushButton_removeCam.setText(_translate("Dialog", "◀"))
        
        self.pushButton_addMdlRef.setText(_translate("Dialog", "▶"))
        self.pushButton_removeMdlRef.setText(_translate("Dialog", "◀"))
        
        self.pushButton_addRigRef.setText(_translate("Dialog", "▶"))
        self.pushButton_removeRigRef.setText(_translate("Dialog", "◀"))


# ==========================================
# ASSET DETECTION & SET LOGIC
# ==========================================
def classify_reference(ref_node):
    try:
        if not cmds.referenceQuery(ref_node, isLoaded=True):
            return None
        nodes = cmds.referenceQuery(ref_node, nodes=True) or []
        if not nodes:
            return None
        if cmds.ls(nodes, type='joint'):
            return 'rig'
        if cmds.ls(nodes, type='mesh'):
            return 'mdl'
    except RuntimeError:
        pass
    return None

def get_publish_namespaces(asset_type):
    namespaces = {}
    refs = cmds.ls(references=True)
    for ref in refs:
        detected_type = classify_reference(ref)
        if detected_type == asset_type:
            try:
                rig_namespace = cmds.referenceQuery(ref, namespace=True)
                if rig_namespace.startswith(':'):
                    rig_namespace = rig_namespace[1:]
                ref_path = cmds.referenceQuery(ref, filename=True)
                
                # Fetch naming logic if utils available, else fallback
                try:
                    pub_info = utils.get_pub_info(ref_path)
                    asset_name = utils.get_root_name(pub_info)
                except NameError:
                    asset_name = os.path.basename(ref_path).split('.')[0]
                
                root = f"{rig_namespace}:{asset_name}"
                namespaces[rig_namespace] = {
                    'assetName': asset_name,
                    'root': root,
                    'refPath': ref_path,
                    'namespace': rig_namespace,
                    'refNode': ref
                }
            except Exception:
                pass
    return namespaces

def get_cams():
    bad_cam = ['frontShape', 'perspShape', 'perspShape1', 'sideShape', 'topShape']
    cam_list = list(set(cmds.ls(type='camera')) - set(bad_cam))
    return [cmds.listRelatives(cam, p=True)[0] for cam in cam_list if cmds.listRelatives(cam, p=True)]

def stamp_attr(node, attr, value_):
    check = cmds.attributeQuery(attr, n=node, ex=True)
    attrname = f"{node}.{attr}"
    if check:
        cmds.setAttr(attrname, l=False)
    else:
        if isinstance(value_, list):
            cmds.addAttr(node, ln=attr, dt='stringArray')
        elif isinstance(value_, bool):
            cmds.addAttr(node, ln=attr, at='bool')
        else:
            cmds.addAttr(node, ln=attr, dt='string')
    if isinstance(value_, list):
        cmds.setAttr(attrname, len(value_), *value_, type='stringArray')
    elif isinstance(value_, bool):
        cmds.setAttr(attrname, value_)
    else:
        cmds.setAttr(attrname, str(value_), type='string')
    cmds.setAttr(attrname, l=True)

class PubSet(object):
    def __init__(self, task):
        self.task = task
        self.nodeSet = f"{task}_exp"
        self.nodeType = 'reference'
        if not cmds.objExists(self.nodeSet):
            cmds.sets(n=self.nodeSet, em=True)
            stamp_attr(self.nodeSet, 'pxm_set_pub_type', self.nodeType)

    def list_exp_ref_nodes(self):
        exp_nodes_set = cmds.sets(self.nodeSet, q=True) or []
        name_space_dict = get_publish_namespaces(self.task)
        exp_spaces = [a for a in name_space_dict if name_space_dict[a]['refNode'] in exp_nodes_set]
        ref_spaces = [a for a in name_space_dict if name_space_dict[a]['refNode'] not in exp_nodes_set]
        return ref_spaces, exp_spaces

    def remove_ref(self, rig_namespace_list):
        name_space_dict = get_publish_namespaces(self.task)
        ref_node_list = [name_space_dict[ns]['refNode'] for ns in rig_namespace_list if ns in name_space_dict]
        if ref_node_list: cmds.sets(ref_node_list, rm=self.nodeSet)

    def add_ref(self, rig_namespace_list):
        name_space_dict = get_publish_namespaces(self.task)
        ref_node_list = [name_space_dict[ns]['refNode'] for ns in rig_namespace_list if ns in name_space_dict]
        if ref_node_list: cmds.sets(ref_node_list, fe=self.nodeSet)

class PubGenSet(object):
    def __init__(self, get_func, set_name, node_type):
        self.get_func = get_func
        self.nodeSet = set_name
        self.nodeType = node_type
        if not cmds.objExists(self.nodeSet):
            cmds.sets(n=self.nodeSet, em=True)
            stamp_attr(self.nodeSet, 'pxm_set_pub_type', self.nodeType)

    def list_exp_ref_nodes(self):
        exp_nodes_set = cmds.sets(self.nodeSet, q=True)
        current_objects = self.get_func()
        if exp_nodes_set:
            remove_list = list(set(exp_nodes_set) - set(current_objects))
            if remove_list: cmds.sets(remove_list, rm=self.nodeSet)
        if not exp_nodes_set: exp_nodes_set = []
        exp_spaces = [a for a in current_objects if a in exp_nodes_set]
        ref_spaces = [a for a in current_objects if a not in exp_nodes_set]
        return ref_spaces, exp_spaces

    def remove_ref(self, rig_namespace_list):
        cams = self.get_func()
        ref_node_list = [cam for cam in cams if cam in rig_namespace_list]
        if ref_node_list: cmds.sets(ref_node_list, rm=self.nodeSet)

    def add_ref(self, rig_namespace_list):
        cams = self.get_func()
        ref_node_list = [cam for cam in cams if cam in rig_namespace_list]
        if ref_node_list: cmds.sets(ref_node_list, fe=self.nodeSet)


# ==========================================
# MAIN DIALOG CLASS
# ==========================================
class AnimOrganizeUi(QtWidgets.QDialog):
    def __init__(self, parent=None):
        self.FSL = {}
        self.parent = parent or get_maya_main_window()

        super(AnimOrganizeUi, self).__init__(self.parent)
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)
        apply_standard_ui_setup(self)

        self.ui.listWidget_cams.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self.ui.listWidget_taggedCams.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self.ui.pushButton_addCam.clicked.connect(self.tag_cams)
        self.ui.pushButton_removeCam.clicked.connect(self.untag_cams)

        self.ui.listWidget_mdlRefs.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self.ui.listWidget_taggedMdlRefs.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self.ui.pushButton_addMdlRef.clicked.connect(self.tag_mdl_refs)
        self.ui.pushButton_removeMdlRef.clicked.connect(self.untag_mdl_refs)

        self.ui.listWidget_rigRefs.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self.ui.listWidget_taggedRigRefs.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self.ui.pushButton_addRigRef.clicked.connect(self.tag_rig_refs)
        self.ui.pushButton_removeRigRef.clicked.connect(self.untag_rig_refs)

        #self.ui.pushButton_cleanNamespaces.clicked.connect(self.clean_ns)
        self.ui.pushButton_publish.clicked.connect(self.publish_data)
        
        self.draw_cams()
        self.draw_mdl_refs()
        self.draw_rig_refs()

    def select_top_nodes(self, item_names, asset_type):
        nodes_to_select = []
        if asset_type == 'cam':
            nodes_to_select = [cam for cam in item_names if cmds.objExists(cam)]
        else:
            namespaces_dict = get_publish_namespaces(asset_type)
            for ns in item_names:
                if ns in namespaces_dict:
                    ref_node = namespaces_dict[ns]['refNode']
                    try:
                        ref_contents = cmds.referenceQuery(ref_node, nodes=True) or []
                        assemblies = cmds.ls(ref_contents, assemblies=True, long=True)
                        nodes_to_select.extend(assemblies)
                    except RuntimeError:
                        pass
        if nodes_to_select:
            cmds.select(nodes_to_select, replace=True)
        else:
            cmds.select(clear=True)

    def tag_cams(self):
        ref_set = PubGenSet(get_cams, 'cam_exp', 'camera')
        refs = [item.text() for item in self.ui.listWidget_cams.selectedItems()]
        if refs:
            ref_set.add_ref(refs)
            self.draw_cams()
            self.select_top_nodes(refs, 'cam')

    def untag_cams(self):
        ref_set = PubGenSet(get_cams, 'cam_exp', 'camera')
        refs = [item.text() for item in self.ui.listWidget_taggedCams.selectedItems()]
        if refs:
            ref_set.remove_ref(refs)
            self.draw_cams()

    def draw_cams(self):
        ref_set = PubGenSet(get_cams, 'cam_exp', 'camera')
        self.ui.listWidget_cams.clear()
        self.ui.listWidget_taggedCams.clear()
        scene_refs, tagged_refs = ref_set.list_exp_ref_nodes()
        self.ui.listWidget_cams.addItems(sorted(scene_refs))
        self.ui.listWidget_taggedCams.addItems(sorted(tagged_refs))

    def tag_mdl_refs(self):
        ref_set = PubSet('mdl')
        refs = [item.text() for item in self.ui.listWidget_mdlRefs.selectedItems()]
        if refs:
            ref_set.add_ref(refs)
            self.draw_mdl_refs()
            self.select_top_nodes(refs, 'mdl')

    def untag_mdl_refs(self):
        ref_set = PubSet('mdl')
        refs = [item.text() for item in self.ui.listWidget_taggedMdlRefs.selectedItems()]
        if refs:
            ref_set.remove_ref(refs)
            self.draw_mdl_refs()

    def draw_mdl_refs(self):
        ref_set = PubSet('mdl')
        self.ui.listWidget_mdlRefs.clear()
        self.ui.listWidget_taggedMdlRefs.clear()
        scene_refs, tagged_refs = ref_set.list_exp_ref_nodes()
        self.ui.listWidget_mdlRefs.addItems(sorted(scene_refs))
        self.ui.listWidget_taggedMdlRefs.addItems(sorted(tagged_refs))

    def tag_rig_refs(self):
        ref_set = PubSet('rig')
        refs = [item.text() for item in self.ui.listWidget_rigRefs.selectedItems()]
        if refs:
            ref_set.add_ref(refs)
            self.draw_rig_refs()
            self.select_top_nodes(refs, 'rig')

    def untag_rig_refs(self):
        ref_set = PubSet('rig')
        refs = [item.text() for item in self.ui.listWidget_taggedRigRefs.selectedItems()]
        if refs:
            ref_set.remove_ref(refs)
            self.draw_rig_refs()

    def draw_rig_refs(self):
        ref_set = PubSet('rig')
        self.ui.listWidget_rigRefs.clear()
        self.ui.listWidget_taggedRigRefs.clear()
        scene_refs, tagged_refs = ref_set.list_exp_ref_nodes()
        self.ui.listWidget_rigRefs.addItems(sorted(scene_refs))
        self.ui.listWidget_taggedRigRefs.addItems(sorted(tagged_refs))

    def clean_ns(self):
        self.draw_cams()
        self.draw_mdl_refs()
        self.draw_rig_refs()

    def publish_data(self):
        # Open a standard Maya File Dialog to let the user choose where to save the USD
        filepaths = cmds.fileDialog2(
            fileFilter="USD Ascii (*.usda);;USD Binary (*.usd)", 
            dialogStyle=2, 
            fileMode=0, # 0 = Save a single file
            caption="Save USD Layout"
        )
        
        # If the user selected a file and didn't hit cancel
        if filepaths and len(filepaths) > 0:
            output_path = filepaths[0]
            
            # Ensure proper extension
            if not (output_path.endswith('.usda') or output_path.endswith('.usd')):
                output_path += '.usda'
                
            # Fire the Export Logic
            try:
                export_sets_to_usd_layout(output_path)
                cmds.confirmDialog(
                    title="Export Successful", 
                    message=f"Successfully exported USD Layout to:\n{output_path}", 
                    button=["OK"]
                )
            except Exception as e:
                cmds.confirmDialog(
                    title="Export Error", 
                    message=f"An error occurred during export:\n{str(e)}", 
                    button=["OK"]
                )
        else:
            cmds.warning("USD Layout Export Cancelled.")


def run():
    global maya_organizer_ui_instance
    if maya_organizer_ui_instance is not None:
        try:
            maya_organizer_ui_instance.close()
            maya_organizer_ui_instance.deleteLater()
        except Exception:
            pass
    maya_organizer_ui_instance = AnimOrganizeUi()
    maya_organizer_ui_instance.show()
    
run()