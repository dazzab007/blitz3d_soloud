Type Thing
  Field x,y#,name$
End Type
Dim grid(4,4)
Function addup(a,b,c$)
  Local t=a+b
  Return t+Len(c$)
End Function
t.Thing=New Thing
t\x=5
t\name$="hi"
grid(2,3)=addup(t\x,7,t\name$)
For i=0 To 4
  grid(i,i)=i*2
Next
Print grid(2,3)