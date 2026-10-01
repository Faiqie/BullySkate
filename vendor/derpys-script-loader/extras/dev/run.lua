-- run any lua chunk with /lua
-- or just type it straight into the console

local function run(text)
	if text then
		local f,m = loadstring(text,"@command")
		if f then
			setfenv(f,GetScriptEnvironment())
			PrintOutput(text)
			CreateThread(f)
		else
			PrintError(m)
		end
	else
		PrintError("expected lua chunk")
	end
end
SetCommand("lua",run,true,"Usage: lua <chunk>\nRun a lua chunk that runs in the environment of "..GetScriptCollection().."/run.lua. You can force cleanup by restarting "..GetScriptCollection()..".")
SetCommand("/",run,true) -- default command
