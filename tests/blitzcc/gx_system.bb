SetEnv "BB_GX_TEST","hello"
Print GetEnv("BB_GX_TEST")
Print GetEnv("BB_GX_NOT_SET")+"|"
Print Left(SystemProperty("os"),5)
Print SystemProperty("cpu")
Print Right(SystemProperty("tempdir"),1)
Print CommandLine()+"|"
AppTitle "My Program"
Print "a"
End
Print "b"
