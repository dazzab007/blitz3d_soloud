Type T
 Field a
 Field b
End Type
Print Add(40,2)
Print Scale(1.5,4)
Print MixU(1,0.5,2,0.25)
Print Sum9(1,2,3,4,5,6,7,8,9)
Print Greet("Mac")
Print Len(Greet("abc"))
b=CreateBank(8)
For i=0 To 7 : PokeByte b,i,i+1 : Next
Print SumBytes(b,8)
FillBytes b,4,7
Print PeekByte(b,3)+PeekByte(b,4)
t.T=New T : t\a=30 : t\b=12
Print ObjSum(t)
