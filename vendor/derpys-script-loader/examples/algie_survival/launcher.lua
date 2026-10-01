gBlip = -1

function MissionSetup()
	-- BlipAddXYZ is one of very few functions that don't work properly in dsl scripts
	-- using a proxy script means that a native script will be used to call a function
	-- without this, blips don't show up on the ground properly
	UseProxyScriptForFunction("main.lua","BlipAddXYZ")
	UseProxyScriptForFunction("main.lua","BlipRemove")
end
function MissionCleanup()
	if gBlip ~= -1 then
		BlipRemove(gBlip) -- since the blip technically belongs to main.lua, it is *very* important we remove it when our script ends
	end
end
function main()
	while not SystemIsReady() do
		Wait(0) -- wait until system is ready
	end
	while true do
		local x,y,z = 574.274,-472.992,4.468
		gBlip = BlipAddXYZ(x,y,z,24,1,10)
		while gBlip ~= -1 do
			if PlayerIsInAreaXYZ(x,y,z,1) and PedMePlaying(gPlayer,"DEFAULT_KEY",true) then -- DEFAULT_KEY basically means the ped isn't "doing" anything
				if IsButtonBeingPressed(9,0) then
					BlipRemove(gBlip)
					gBlip = -1
					play_script()
					break -- immediately jump to the end of the while loop
				end
				draw_prompt()
			end
			Wait(0)
		end
		Wait(1000)
	end
end
function draw_prompt()
	SetTextFont("Georgia")
	SetTextBold()
	SetTextColor(200,200,200,255)
	SetTextShadow()
	SetTextScale(1.49)
	SetTextAlign("LEFT","BOTTOM")
	SetTextPosition(0.188/GetDisplayAspectRatio(),0.935) -- x is divided by aspect ratio so it is the same distance from the right on all aspect ratios
	DrawText("Start \"Algie Survival\"")
end
function play_script()
	local script = StartScript("survival.lua")
	if script then
		while IsScriptRunning(script) do
			Wait(0) -- wait for the survival script to finish
		end
	end
end
