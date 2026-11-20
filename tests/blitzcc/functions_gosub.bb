Function fact(n)
 If n<=1 Then Return 1
 Return n*fact(n-1)
End Function
Function avg#(a#,b#)
 Return (a+b)/2
End Function
Print fact(10)
Print avg(1,2)
Gosub sub1
Gosub sub1
Goto done
.sub1
Print "sub"
Return
.done
