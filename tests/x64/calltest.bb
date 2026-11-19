; x64 commit 6: call classes in the IR (bbdump).
; User functions: Blitz convention (no class string).
; Runtime functions and helpers: "C" + one class per arg ('i' GP, 'f' XMM).
Function mix#(a,b#,c$)
	Return a+b+Len(c)
End Function

x#=mix(1,2.5,"abc")
s$=x
y#=Sin(x)
z#=x^2
w#=Abs(y)
v#=x Mod 3
Print Left(s,2)
