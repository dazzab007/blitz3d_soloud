; graphics3d
Graphics3D 64,48,0,2
SetBuffer BackBuffer()
cam=CreateCamera()
cube=CreateCube()
PositionEntity cube,0,0,5
other=CreateSphere(8)
PositionEntity other,3,0,5
EntityPickMode cube,2
EntityPickMode other,1
; projection of a world point to the screen
CameraProject cam,0,0,5
DebugLog "centre: "+Int(ProjectedX())+","+Int(ProjectedY())
CameraProject cam,2,0,5
DebugLog "right of centre: "+(ProjectedX()>32)
; picking
DebugLog "pick centre: "+(CameraPick(cam,32,24)=cube)
DebugLog "pick other: "+(CameraPick(cam,51,24)<>0)
DebugLog "pick empty: "+(CameraPick(cam,2,2)=0)
CameraPick cam,32,24
DebugLog "picked z: "+Int(PickedZ())
; parenting moves the child with the parent
pivot=CreatePivot()
EntityParent other,pivot
PositionEntity pivot,0,1,0
DebugLog "child world y: "+Int(EntityY(other,1))
; entities in view
DebugLog "cube in view: "+EntityInView(cube,cam)
PositionEntity cube,0,0,-20
DebugLog "cube behind: "+EntityInView(cube,cam)
DebugLog "distance: "+Int(EntityDistance(cube,cam))
RenderWorld
DebugLog "rendered "+(TrisRendered()>0)
ClearWorld
DebugLog "cleared"
