base$=SystemProperty("tempdir")+"bb_gx_files_test"
If FileType(base)=2 Then DeleteDir base
Print FileType(base)
CreateDir base
Print FileType(base)
f=WriteFile(base+"/a.txt")
WriteLine f,"line one"
WriteLine f,"line two"
CloseFile f
Print FileType(base+"/a.txt")
Print FileSize(base+"/a.txt")
CopyFile base+"/a.txt",base+"/b.txt"
f=ReadFile(base+"/b.txt")
Print ReadLine(f)
Print ReadLine(f)
Print Eof(f)
CloseFile f
n=0
d=ReadDir(base)
file$=NextFile(d)
While file<>""
 If file<>"." And file<>".." Then n=n+1
 file=NextFile(d)
Wend
CloseDir d
Print n
DeleteFile base+"/a.txt"
DeleteFile base+"/b.txt"
DeleteDir base
Print FileType(base)
