Type Node
  Field v
  Field nxt.Node
  Field name$
End Type
Dim names$(3)
Dim nodes.Node(3)
Function mk.Node(v)
  n.Node=New Node
  n\v=v
  n\name$=Str$(v)
  Return n
End Function
Function label$(n.Node)
  Return "node"+n\name$
End Function
a.Node=mk(1)
a\nxt=mk(2)
names$(1)=label$(a)
nodes(2)=a\nxt
For n.Node=Each Node
  Print Left$(n\name$,1)
Next
h=Handle(a)
b.Node=Object.Node(h)
Delete a\nxt
Print names$(1)
