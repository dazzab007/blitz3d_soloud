; graphics
Graphics 64,48,0,2
img=CreateImage(8,8)
SetBuffer ImageBuffer(img)
Color 255,0,255 : Rect 0,0,8,8,1
Color 255,255,255 : Rect 2,2,4,4,1
MaskImage img,255,0,255
SetBuffer BackBuffer()
Color 0,0,255 : Cls
DrawImage img,10,10
DebugLog Hex(ReadPixel(10,10))
DebugLog Hex(ReadPixel(13,13))
DebugLog ImageWidth(img)+"x"+ImageHeight(img)
other=CreateImage(8,8)
SetBuffer ImageBuffer(other)
Color 255,255,255 : Rect 0,0,8,8,1
MaskImage other,0,0,0
SetBuffer BackBuffer()
DebugLog ImagesCollide(img,0,0,0,other,3,3,0)
DebugLog ImagesCollide(img,0,0,0,other,20,20,0)
DebugLog ImageRectCollide(img,0,0,0,3,3,2,2)
Cls
DrawImageRect img,20,20,2,2,4,4
DebugLog Hex(ReadPixel(21,21))
GrabImage other,0,0
DebugLog Hex(ReadPixel(5,5))
HandleImage img,4,4
DebugLog ImageXHandle(img)+","+ImageYHandle(img)
Cls : DrawImage img,10,10
DebugLog Hex(ReadPixel(8,8))
CopyRect 0,0,10,10,30,30
FreeImage img
DebugLog "done"
SetBuffer FrontBuffer()
Flip
