; graphics
Graphics 160,60,0,2
SetBuffer BackBuffer()
Color 255,255,255
Text 4,4,"Hello"
n=0
For y=0 To 20 : For x=0 To 60 : If ReadPixel(x,y)=$ffffffff Then n=n+1
Next : Next
DebugLog n>10
DebugLog StringWidth("Hello")+" "+FontHeight()
Locate 0,30
Print "line"
SetBuffer FrontBuffer()		;Print always draws on the front buffer
n=0
For y=28 To 45 : For x=0 To 40 : If ReadPixel(x,y)=$ffffffff Then n=n+1
Next : Next
DebugLog n>5
SetBuffer BackBuffer()
f=LoadFont("Arial",20)
SetFont f
DebugLog FontHeight()>16
DebugLog StringWidth("iii")<StringWidth("WWW")
FreeFont f
Flip
DebugLog "ok"
