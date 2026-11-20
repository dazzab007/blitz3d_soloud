; graphics
Graphics 64,48,0,2
Color 255,0,0 : Rect 2,2,10,10,1
Color 0,255,0 : Oval 20,20,16,16,1
Color 0,0,255 : Line 0,47,63,47
DebugLog Hex(ReadPixel(5,5))
DebugLog Hex(ReadPixel(1,1))
DebugLog Hex(ReadPixel(28,28))
DebugLog Hex(ReadPixel(30,47))
DebugLog GraphicsWidth()+"x"+GraphicsHeight()+" "+GraphicsDepth()
Cls
DebugLog Hex(ReadPixel(5,5))
ClsColor 10,20,30 : Cls
DebugLog Hex(ReadPixel(0,0))
Origin 10,10 : Color 255,255,255 : Plot 0,0
DebugLog Hex(ReadPixel(0,0))
Origin 0,0
Viewport 20,20,5,5 : Cls : Color 255,255,0 : Rect 0,0,64,48,1
n=0 : Viewport 0,0,64,48
For y=0 To 47 : For x=0 To 63 : If ReadPixel(x,y)=$ffffff00 Then n=n+1
Next : Next
DebugLog n
WritePixel 3,3,$ff123456
DebugLog Hex(ReadPixel(3,3))
DebugLog Hex(ReadPixel(-5,-5)=0)
