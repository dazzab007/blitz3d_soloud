Type Thing
 Field id
 Field name$
End Type
For i=1 To 4
 t.Thing=New Thing : t\id=i : t\name="n"+i
Next
For t.Thing=Each Thing : Print t\id+" "+t\name : Next
t=First Thing : Delete t
n=0 : For t.Thing=Each Thing : n=n+1 : Next : Print n
Dim a(5)
For i=0 To 5 : a(i)=i*i : Next
Print a(5)
Local v$[2] : v[0]="x" : v[1]="y" : Print v[0]+v[1]
Read q : Read r$ : Print q+r
Data 41,"!"
