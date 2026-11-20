; graphics3d
Function Near(c,want,tol=6)
 Return Abs(((c Shr 16) And 255)-((want Shr 16) And 255))<=tol And Abs(((c Shr 8) And 255)-((want Shr 8) And 255))<=tol And Abs((c And 255)-(want And 255))<=tol
End Function

Graphics3D 64,48,0,2
SetBuffer BackBuffer()
cam=CreateCamera()
CameraClsColor cam,0,0,60
CameraRange cam,0.5,100
cube=CreateCube()
EntityColor cube,255,0,0
PositionEntity cube,0,0,4
light=CreateLight(1)
TurnEntity light,0,0,0
AmbientLight 60,60,60
RenderWorld
DebugLog "clear: "+Near(ReadPixel(1,1),$00003c)
DebugLog "cube drawn: "+Near(ReadPixel(32,24),$ff0000,40)
DebugLog "tris: "+TrisRendered()
; fullbright shows the pure colour
EntityFX cube,1
RenderWorld
DebugLog "fullbright: "+Near(ReadPixel(32,24),$ff0000)
; hiding the entity leaves only the background
HideEntity cube
RenderWorld
DebugLog "hidden: "+Near(ReadPixel(32,24),$00003c)
ShowEntity cube
; alpha blending over the background
EntityAlpha cube,0.5
EntityBlend cube,1
RenderWorld
c=ReadPixel(32,24)
DebugLog "alpha: "+(((c Shr 16) And 255)>90 And ((c Shr 16) And 255)<170 And (c And 255)>20)
EntityAlpha cube,1
; the camera viewport
CameraViewport cam,32,0,32,48
Cls
RenderWorld
DebugLog "viewport left empty: "+Near(ReadPixel(8,24),$000000)
DebugLog "viewport right drawn: "+Near(ReadPixel(48,24),$ff0000)
; wireframe leaves the middle of a face empty
CameraViewport cam,0,0,64,48
WireFrame True
RenderWorld
n=0
For y=19 To 29 : For x=27 To 37
 If Near(ReadPixel(x,y),$00003c) Then n=n+1
Next : Next
DebugLog "wireframe hollow: "+(n>40)
WireFrame False
; fog
CameraFogMode cam,1
CameraFogColor cam,0,255,0
CameraFogRange cam,1,6
RenderWorld
c=ReadPixel(32,24)
DebugLog "fogged: "+(((c Shr 8) And 255)>60)
FreeEntity cube
