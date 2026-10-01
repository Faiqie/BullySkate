-- SIMPLE WINDOW MOD - derpy54320

config = GetScriptConfig()
state = GetPersistentDataTable()
if state.windowed == nil then
	state.windowed = not GetConfigBoolean(config,"start_borderless")
end

RegisterLocalEventHandler("WindowGoingFullscreen",function()
	return true
end)
RegisterLocalEventHandler("WindowMinimizing",function()
	if GetConfigBoolean(config,"disable_minimize") then
		return true
	end
end)
RegisterLocalEventHandler("WindowUpdating",function(window)
	local w,h = GetInternalResolution()
	if state.windowed then
		window.width = GetConfigNumber(config,"windowed_width",w)
		window.height = GetConfigNumber(config,"windowed_height",h)
		w,h = GetScreenResolution()
		window.x = GetConfigNumber(config,"windowed_x",(w-window.width)/2)
		window.y = GetConfigNumber(config,"windowed_y",(h-window.height)/2)
		window.style = {
			WS_OVERLAPPED = true,
			WS_CAPTION = true,
			WS_SYSMENU = true,
			WS_THICKFRAME = true
		}
	else
		window.x = GetConfigNumber(config,"borderless_x",0)
		window.y = GetConfigNumber(config,"borderless_y",0)
		window.width = GetConfigNumber(config,"borderless_width",w)
		window.height = GetConfigNumber(config,"borderless_height",h)
		window.style = {}
	end
	return true
end)
CreateSystemThread(function()
	while true do
		if IsKeyPressed("LALT") or IsKeyPressed("RALT") then
			if IsKeyBeingPressed("F4") then
				QuitGame()
			elseif IsKeyBeingPressed("RETURN") then
				state.windowed = not state.windowed
				ForceWindowUpdate()
			end
		end
		Wait(0)
	end
end)
