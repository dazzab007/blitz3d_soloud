t=MilliSecs()
Delay 100
d=MilliSecs()-t
Print d>=95 And d<1000
tm=CreateTimer(50)
n=WaitTimer(tm)
Print n>=1
FreeTimer tm
