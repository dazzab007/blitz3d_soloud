; graphics3d
Function Near(c,want,tol=6)
 Return Abs(((c Shr 16) And 255)-((want Shr 16) And 255))<=tol And Abs(((c Shr 8) And 255)-((want Shr 8) And 255))<=tol And Abs((c And 255)-(want And 255))<=tol
End Function

Graphics3D 64,48,0,2
SetBuffer BackBuffer()
cam=CreateCamera()
CameraClsColor cam,0,0,0
; a camera-facing quad built by hand: u runs left to right, v top to bottom
quad=CreateMesh()
s=CreateSurface(quad)
v0=AddVertex(s,-2,-1.5,0,0,1)
v1=AddVertex(s,-2, 1.5,0,0,0)
v2=AddVertex(s, 2, 1.5,0,1,0)
v3=AddVertex(s, 2,-1.5,0,1,1)
AddTriangle s,v0,v1,v2
AddTriangle s,v0,v2,v3
UpdateNormals quad
PositionEntity quad,0,0,4
EntityFX quad,1
DebugLog "verts "+CountVertices(s)+" tris "+CountTriangles(s)
; a 4x4 texture: left half red, right half green
tex=CreateTexture(4,4,1+8)
SetBuffer TextureBuffer(tex)
Color 255,0,0 : Rect 0,0,2,4,1
Color 0,255,0 : Rect 2,0,2,4,1
SetBuffer BackBuffer()
DebugLog "texture size "+TextureWidth(tex)+"x"+TextureHeight(tex)
EntityTexture quad,tex
RenderWorld
DebugLog "left red: "+Near(ReadPixel(22,24),$ff0000)
DebugLog "right green: "+Near(ReadPixel(42,24),$00ff00)
; scale and offset the texture: shift by half a texture
PositionTexture tex,0.5,0
RenderWorld
DebugLog "shifted left green: "+Near(ReadPixel(22,24),$00ff00)
PositionTexture tex,0,0
; a non power-of-two texture is rounded up
t2=CreateTexture(5,3)
DebugLog "npot "+TextureWidth(t2)+"x"+TextureHeight(t2)
; vertex colours on a second, untextured quad
FreeEntity quad
quad=CreateMesh()
s=CreateSurface(quad)
v0=AddVertex(s,-2,-1.5,0)
v1=AddVertex(s,-2, 1.5,0)
v2=AddVertex(s, 2, 1.5,0)
v3=AddVertex(s, 2,-1.5,0)
AddTriangle s,v0,v1,v2
AddTriangle s,v0,v2,v3
PositionEntity quad,0,0,4
For k=0 To 3 : VertexColor s,k,255,0,0 : Next
EntityFX quad,1+2
RenderWorld
DebugLog "vertex colour: "+Near(ReadPixel(32,24),$ff0000)
DebugLog "vertex "+Int(VertexX(s,2))+","+Int(VertexY(s,2))
FreeEntity quad
FreeTexture tex
