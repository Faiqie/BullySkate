gOpenKey = GetConfigString(GetScriptConfig(),"open_key","F5") -- get the "open_key" setting from config.txt or default to "F5"
gShowing = false -- showing menu
gDisablePunish = false

-- main stuff
function MissionSetup()
	-- make sure the key is valid
	if not IsKeyValid(gOpenKey) then
		PrintWarning("bad open key: "..gOpenKey)
		StopCurrentScriptCollection() -- if it's not valid then kill the whole script collection
		return
	end
	while not SystemIsReady() do -- wait for system to be ready
		Wait(0)
	end
	CreateThread(function() -- make a thread to draw some text so it doesn't hold up the rest of the script
		local draw_until = GetTimer() + 5000
		repeat
			SetTextFont("Georgia") -- text is drawn by first setting up the way you want the text formatted, *then* calling DrawText each frame you want to draw
			SetTextBold()
			SetTextColor(200,200,200,255)
			SetTextShadow()
			SetTextPosition(0.5,0) -- by default text is center-aligned on the x-axis, and top-aligned on the y-axis, so this'll look good at the top
			DrawText("Press "..gOpenKey.." to open the menu.")
			Wait(0)
		until GetTimer() >= draw_until
	end)
end
function MissionCleanup()
	if gDisablePunish then
		DisablePunishmentSystem(false)
	end
end
function main()
	local thread
	local menus = {setup_main_menu(),n = 1} -- table of menus. we use "n" for the count because Lua automatically updates it when we insert / remove
	while true do
		if IsKeyBeingPressed(gOpenKey) then
			gShowing = not gShowing
		elseif gShowing then
			local menu = menus[menus.n] -- the last menu in the menus table is the active one
			if IsKeyBeingPressed("LEFT") then
				if menus.n > 1 then
					table.remove(menus) -- remove a menu from the menu stack
				else
					gShowing = false -- or just stop showing the menu if we're on the main menu
				end
			elseif IsKeyBeingPressed("RIGHT") then
				local option = menu[menu.i]
				if option.func then
					option.func(option) -- run the function for the current option in the current menu if it is set
				end
				if option.menu then
					table.insert(menus,option.menu) -- add the sub-menu to the menu stack if it is set
				end
			elseif IsKeyBeingPressed("UP") then
				menu.i = menu.i - 1 -- go back
				if menu.i < 1 then
					menu.i = menu.n -- wrap around
				end
			elseif IsKeyBeingPressed("DOWN") then
				menu.i = menu.i + 1 -- go forward
				if menu.i > menu.n then
					menu.i = 1 -- wrap around
				end
			end
		end
		if gShowing then
			draw_menu(menus[menus.n]) -- draw the active menu
		end
		Wait(0)
	end
end

-- setup menus
function setup_main_menu()
	return setup_menu({
		-- a menu table is just an array of options. each option table can have "name", "func", and "menu" in it.
		{
			name = "Player Options",
			menu = { -- sub-menu (that will get added to the menu stack in main if we select it)
				{
					name = "Revive Player",
					func = revive_player, -- these functions exist farther down in the script
				},
				{
					name = "Give $500",
					func = give_cash,
				},
				{
					name = "Disable Punishment [OFF]",
					func = toggle_punish,
				},
			},
		},
		{
			name = "World Settings",
			menu = {
				{
					name = "Set Chapter",
					menu = {
						{name = "Chapter 1",func = function() ChapterSet(0) end}, -- some things are simple so we can just put the function right in here
						{name = "Chapter 2",func = function() ChapterSet(1) end},
						{name = "Chapter 3",func = function() ChapterSet(2) end},
						{name = "Chapter 4",func = function() ChapterSet(3) end},
						{name = "Chapter 5",func = function() ChapterSet(4) end},
						{name = "Chapter 6",func = function() ChapterSet(5) end},
					},
				},
				{
					name = "Cycle Weather [0]",
					weather = 0, -- this field isn't used by the main menu control logic, but our whole table is passed to func so we can use it there
					func = function(option)
						option.weather = option.weather + 1
						if option.weather == 5 then
							option.weather = 0
						end
						WeatherSet(option.weather,true)
						option.name = "Cycle Weather ["..option.weather.."]"
					end,
				},
			},
		},
		{
			name = "Teleport Areas",
			menu = {
				{name = "Business District",func = function() teleport_area(0,458.590,-80.737,5.917,0.0) end},
				{name = "Carnival",func = function() teleport_area(0,226.200,412.600,5.000,150.0) end},
				{name = "Cemetery",func = function() teleport_area(0,616.307,238.970,19.000,90.0) end},
				{name = "Industrial Area",func = function() teleport_area(0,355.000,-432.000,3.000,0.0) end},
				{name = "Nerd Fort",func = function() teleport_area(0,50.256,-135.007,11.905,180.0) end},
				{name = "Poor Area",func = function() teleport_area(0,499.367,-245.193,1.948,-90.0) end},
				{name = "Rich Area",func = function() teleport_area(0,338.800,113.800,7.800,90.0) end},
				{name = "School Grounds",func = function() teleport_area(0,272.680,-73.131,6.969,90.0) end},
				{name = "School Rooftop",func = function() teleport_area(0,185.780,-65.800,26.090,90.0) end},
				{name = "Principal (Front Desk)",func = function() teleport_area(2,-633.348,-292.075,6.415,90.0) end},
				{name = "School Hallways",func = function() teleport_area(2,-628.700,-326.300,0.000,90.0) end},
				{name = "Chemistry",func = function() teleport_area(4,-599.199,326.327,34.411,0.0) end},
				{name = "Principal (Office)",func = function() teleport_area(5,-701.084,213.482,31.957,90.0) end},
				{name = "Biology",func = function() teleport_area(6,-709.749,312.360,33.574,0.0) end},
				{name = "Janitors Room",func = function() teleport_area(8,-746.935,-51.183,9.301,0.0) end},
				{name = "Library",func = function() teleport_area(9,-784.875,203.098,90.310,0.0) end},
				{name = "Pool and Gym",func = function() teleport_area(13,-623.211,-72.821,60.119,0.0) end},
				{name = "Boys Dorm",func = function() teleport_area(14,-502.470,309.606,31.963,0.0) end},
				{name = "Classroom",func = function() teleport_area(15,-563.082,316.990,-1.800,0.0) end},
				{name = "Classroom 2",func = function() teleport_area(15,-563.082,316.990,6.174,0.0) end},
				{name = "Tattoo Trailer",func = function() teleport_area(16,-654.824,80.927,0.345,90.0) end},
				{name = "Art Room",func = function() teleport_area(17,-536.998,376.584,14.180,0.0) end},
				{name = "Auto shop",func = function() teleport_area(18,-427.469,365.065,81.100,0.0) end},
				{name = "Auditorium",func = function() teleport_area(19,-778.496,292.221,77.951,0.0) end},
				{name = "Chemical Plant",func = function() teleport_area(20,-759.167,96.995,31.413,0.0) end},
				{name = "Staff Room",func = function() teleport_area(23,-655.347,228.989,0.000,0.0) end},
				{name = "General Store",func = function() teleport_area(26,-573.884,388.877,0.213,90.0) end},
				{name = "Boxing Club",func = function() teleport_area(27,-702.445,372.796,293.937,252.5) end},
				{name = "Bike Shop",func = function() teleport_area(29,-785.601,380.055,0.730,0.0) end},
				{name = "Comic Shop",func = function() teleport_area(30,-724.707,12.604,1.696,0.0) end},
				{name = "Prep House",func = function() teleport_area(32,-569.538,133.330,46.300,0.0) end},
				{name = "Clothing (Rich)",func = function() teleport_area(33,-707.636,257.073,0.300,0.0) end},
				{name = "Clothing (Poor)",func = function() teleport_area(34,-647.592,255.706,1.436,0.0) end},
				{name = "Girls Dorm",func = function() teleport_area(35,-454.000,311.000,3.500,0.0) end},
				{name = "Tenements",func = function() teleport_area(36,-544.463,-48.881,32.009,0.0) end},
				{name = "Funhouse",func = function() teleport_area(37,-704.700,-537.944,8.269,0.0) end},
				{name = "Asylum",func = function() teleport_area(38,-736.295,422.404,2.500,30.0) end},
				{name = "Barber Shop",func = function() teleport_area(39,-655.340,121.404,4.602,90.0) end},
				{name = "Observatory",func = function() teleport_area(40,-696.615,61.633,21.089,90.0) end},
				{name = "GoKart Track 2",func = function() teleport_area(42,-338.426,494.663,3.008,90.0) end},
				{name = "Junk Yard",func = function() teleport_area(43,-588.417,-632.785,5.000,0.0) end},
				{name = "Ball Toss",func = function() teleport_area(45,-792.618,89.306,10.340,90.0) end},
				{name = "Shooting Gallery",func = function() teleport_area(45,-792.507,75.569,10.340,90.0) end},
				{name = "Hair Salon",func = function() teleport_area(46,-766.037,17.693,3.714,0.0) end},
				{name = "Souvenir Shop",func = function() teleport_area(50,-794.680,45.840,7.256,90.0) end},
				{name = "Arcade Race LV 1",func = function() teleport_area(51,-31.612,68.692,27.516,0.0) end},
				{name = "Arcade Race LV 2",func = function() teleport_area(52,-94.936,-72.928,0.990,0.0) end},
				{name = "Arcade Race LV 3",func = function() teleport_area(53,-9.347,62.253,63.251,0.0) end},
				{name = "Warehouse",func = function() teleport_area(54,-672.404,-154.813,1.040,0.0) end},
				{name = "Freak Show",func = function() teleport_area(55,-469.610,-78.906,9.278,90.0) end},
				{name = "Poor Barbershop",func = function() teleport_area(56,-664.978,393.284,2.878,0.0) end},
				{name = "Drop Out Save Zone",func = function() teleport_area(57,-654.664,244.846,15.485,0.0) end},
				{name = "Jocks Save Zone",func = function() teleport_area(59,-749.179,347.235,4.064,0.0) end},
				{name = "Preppies Save Zone",func = function() teleport_area(60,-773.434,349.695,6.869,0.0) end},
				{name = "Greaser Save Zone",func = function() teleport_area(61,-692.013,341.897,3.534,0.0) end},
				{name = "BMX Track",func = function() teleport_area(62,-780.997,634.384,29.298,0.0) end},
			}
		},
	})
end
function setup_menu(menu)
	menu.i = 1 -- each menu table has "i" for index
	menu.n = table.getn(menu) -- and "n" for count
	menu.off = 0 -- offset for drawing stuff (were we're "at" in the menu)
	for _,option in ipairs(menu) do
		if option.menu then
			setup_menu(option.menu) -- put "i", "n", and "off" in every sub-menu too, recursively
		end
	end
	return menu
end

-- menu option functions
function revive_player()
	local hp = PedGetMaxHealth(gPlayer)
	if hp > PlayerGetHealth() then -- only restore player health if not over max health (which we may be if we have the kissing bonus health)
		PlayerSetHealth(hp)
	end
end
function give_cash()
	PlayerAddMoney(50000)
end
function toggle_punish(option)
	-- when we call a menu option in "main", we pass the option table. this will let us rename the option.
	gDisablePunish = not gDisablePunish
	DisablePunishmentSystem(gDisablePunish)
	if gDisablePunish then
		PlayerSetPunishmentPoints(0)
		option.name = "Disable Punishment [ON]"
	else
		option.name = "Disable Punishment [OFF]"
	end
end
function teleport_area(area,x,y,z,h)
	-- the actual functions for these options are defined directly in the menu table
	-- that is so they can call this function (teleport_area) with custom arguments
	while AreaIsLoading() do
		Wait(0) -- first we wait while we may already be changing areas just in case
	end
	PlayerSetPosXYZArea(x,y,z,area) -- then switch area
	while AreaIsLoading() do
		Wait(0) -- wait for this area to load
	end
	PedFaceHeading(gPlayer,h,0) -- then set our heading
end

-- draw menu
function draw_menu(menu)
	local formatting
	local max_shown = 16 -- how many options we can show at once
	local width = 0.4 / GetDisplayAspectRatio() -- we divide the width by the aspect ratio so it looks the same width on any monitor
	local height = 0 -- this will be calculated as we measure options
	local x = 0.1 / GetDisplayAspectRatio()
	local y = 0.3
	if menu.n <= max_shown then
		menu.off = 0 -- there aren't enough options to need to offset the menu, so set off to 0
	elseif menu.i - menu.off > max_shown then
		menu.off = menu.i - max_shown -- move "off" forward to show options near the bottom of the menu
	elseif menu.i <= menu.off then
		menu.off = menu.i - 1 -- move "off" back to show options near the top of the menu
	end
	SetTextFont("Georgia")
	SetTextBold()
	SetTextColor(255,255,255,255)
	SetTextAlign("L","T") -- align left on x-axis and top on y-axis
	formatting = GetTextFormatting() -- since we'll need to setup this text multiple times, we'll save it (but it's still active for now)
	for i = 1,max_shown do
		local option = menu[menu.off+i] -- use "off" as an offset for where we're at in the menu
		if option then -- only measure this option if there is an option at this position
			local w,h = MeasureText(option.name) -- first we're measuring all the text using the active formatting so we can draw a properly sized background
			height = height + h
		end
	end
	DiscardText(formatting) -- we're done measuring so we'll reset text formatting (since measuring text doesn't reset formatting)
	DrawRectangle(x,y,width,height,0,0,0,100) -- draw background
	for i = 1,max_shown do
		local option = menu[menu.off+i]
		if option then -- only draw an option if there is one at this position
			SetTextFormatting(formatting) -- set the text again (we need to do this for each draw since drawing resets)
			if menu.off + i == menu.i then
				local w,h = MeasureText(option.name)
				DrawRectangle(x,y,width,h,255,255,255,100) -- draw a backdrop of an inverted color for the currently selected option
				SetTextColor(0,0,0,255) -- and invert the text color
			end
			SetTextPosition(x,y)
			local w,h = DrawText(option.name)
			y = y + h -- move the y position down using the height of the draw option
		end
	end
end

-- disable d-pad buttons when in the menu (so we don't bring up mission text or change the camera by hitting the arrow keys)
RegisterLocalEventHandler("ControllerUpdating",function(c)
	-- this event is usually the only time you should use the SetButtonPressed function since it happens when the game normally updates buttons
	if gShowing and c == 0 then -- only if the menu is showing and just for the controller that controls the player (which is always 0)
		SetButtonPressed(0,0,false) -- we don't really know which game buttons the arrow keys may be mapped to, but this is a fair assumption usually
		SetButtonPressed(1,0,false) -- buttons 0, 1, 2, and 3 are for the d-pad buttons
		SetButtonPressed(2,0,false)
		SetButtonPressed(3,0,false)
	end
end)
