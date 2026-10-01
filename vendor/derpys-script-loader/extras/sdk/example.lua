-- put this file in "packages" so it can be loaded by a script calling require
-- see dslpackage.h for information on making DSL packages

RequireSystemAccess() -- check that we'll be able to call loadlib

local f,m = loadlib(GetPackageFilePath("example.dll"),"luaopen_package") -- get the open function for the package
if not f then
	error(m) -- if an error occurrs just raise an error for our package
end
f() -- otherwise call the open function for our package
