LoadScript("spawns.lua") -- just a table named gSpawns with all our spawn points
MAX_PEDS = 20

gBackupWeapons = {}
gRunPunch = -1 -- last time a ped did a running punch (or -1 for never)
gLevel = 1

-- main stuff
function MissionSetup()
	while AreaIsLoading() do
		Wait(0)
	end
	PlayerSetPosXYZArea(-539.259,-32.151,31.011,36)
	while AreaIsLoading() do
		Wait(0)
	end
	PedFaceHeading(gPlayer,-76.7,0)
	for weapon = 299,444 do
		gBackupWeapons[weapon] = PedGetAmmoCount(gPlayer,weapon) -- save all player weapons
	end
	PedClearAllWeapons(gPlayer)
	DisablePunishmentSystem(true)
	PlayerSetPunishmentPoints(0)
	StopPedProduction(true)
	AreaClearAllPeds()
	PauseGameClock()
end
function MissionCleanup()
	PedClearAllWeapons(gPlayer)
	for weapon,ammo in pairs(gBackupWeapons) do
		GiveAmmoToPlayer(weapon,ammo,false) -- restore all player weapons
	end
	DisablePunishmentSystem(false)
	StopPedProduction(false)
	UnpauseGameClock()
end
function main()
	PlayerSetHealth(PedGetMaxHealth(gPlayer))
	GiveWeaponToPlayer(305,8) -- give spudgun
	show_text_for_ms("Get ready...",3000)
	Wait(5000)
	while true do
		local failed = false
		local algie = spawn_algie() -- algie will be a table of peds
		show_text_for_ms("LEVEL "..gLevel,3000)
		while validate_algie(algie) do -- keep running this level while there are algie alive
			if PedIsDead(gPlayer) or AreaGetVisible() ~= 36 then
				failed = true
				break -- break out of the level loop if we die or leave the area
			end
			for _,ped in ipairs(algie) do
				update_algie_ped(ped) -- update each ped (we know they're all valid because we just called validate_algie the same frame)
			end
			if PedIsPlaying(gPlayer,"/GLOBAL/GUN/GUN/DEFAULT_KEY",true) and PedMePlaying(gPlayer,"RELOAD",true) and PedMePlaying(gPlayer,"SPUDG",true) then
				PedSetActionNode(gPlayer,"/GLOBAL/GUN/GUN/DEFAULT_KEY","")
			end
			Wait(0)
		end
		if failed then
			break -- we didn't actually beat the level so break out now
		end
		cleanup_algie(algie)
		gLevel = gLevel + 1
		Wait(1000)
	end
	show_text_for_ms("You made it to level "..gLevel.."!",5000)
	Wait(5000)
	TerminateCurrentScript()
end

-- text utility
function show_text_for_ms(text,ms)
	-- we create a thread here so that the Wait doesn't hold up the main thread
	CreateThread(function()
		local started = GetTimer()
		repeat
			draw_text(text)
			Wait(0)
		until (GetTimer() - started) >= ms -- keep drawing text until the amount of milliseconds passes
	end)
end
function draw_text(text)
	SetTextFont("Arial")
	SetTextBlack()
	SetTextColor(230,20,20,255)
	SetTextOutline()
	SetTextScale(2)
	SetTextAlign("C","C")
	SetTextPosition(0.5,0.2)
	DrawText(text)
end

-- spawn algie for the current level and return a table of all algie
function spawn_algie()
	local count = 1
	local algie = {}
	if gLevel > 1 then -- level 1 will always just be 1 algie, but past then we'll calculate it differently
		-- 1 + level * 0.5 * modifier
		-- the modifier is a random value between 50% and 200%
		count = 1 + math.floor(gLevel * 0.5 * (math.random(50,200) / 100))
	end
	if count > MAX_PEDS then
		count = MAX_PEDS
	end
	AreaClearAllPeds() -- just to make sure there are no ambient peds before we start spawning
	for i = 1,count do
		local ped = PedCreateXYZ(4,get_spawn_position())
		setup_algie_ped(ped) -- setup this ped
		algie[i] = ped
	end
	return algie
end
function setup_algie_ped(ped)
	local speed_boost = 6 * (gLevel - 1) * (math.random(75,150) / 100) -- add 6% speed each level, and multiply that by 75% - 150% for some randomness
	if math.random(1,100) <= 5 then
		PedSetActionTree(ped,"/GLOBAL/GS_MALE_A","ACT/ANIM/GS_MALE_A.ACT") -- 5% chance algie will use this style where he can run a lot faster than his normal
	end
	PedFaceObject(ped,gPlayer,0)
	PedSetInfiniteSprint(ped,true)
	PedSetPedToTypeAttitude(ped,13,0) -- make them hate the player
	PedSetEmotionTowardsPed(ped,gPlayer,0)
	PedAttackPlayer(ped,3) -- attack the player
	GameSetPedStat(ped,20,100 + speed_boost) -- set their speed stat
	AddBlipForChar(ped,0,26,1)
	GameSetPedStat(ped,1,50) -- half of algies will drop an item
	if math.random(1,100) <= 85 then
		GameSetPedStat(ped,0,316) -- 85% chance the drop item will be spud ammo
	else
		GameSetPedStat(ped,0,362) -- 15% chance of beam cola
	end
	if math.random(1,100) <= 15 then
		PedSetMaxHealth(ped,150) -- 15% chance of strong algie
		PedSetHealth(ped,150)
	else
		PedSetMaxHealth(ped,50) -- fairly weak algie since they'll do so much damage
		PedSetHealth(ped,50)
	end
	PedSetDamageGivenMultiplier(ped,2,4) -- 4x melee damage
	PedClearAllWeapons(ped)
end
function get_spawn_position()
	local count,spawns = 0,{}
	local x1,y1,z1 = PlayerGetPosXYZ()
	for _,spawn in ipairs(gSpawns) do
		local x2,y2,z2 = unpack(spawn) -- each spawn is a table of {x, y, z}
		if math.abs(z2 - z1) < 5 and DistanceBetweenCoords3d(x1,y1,z1,x2,y2,z2) >= 10 then
			count = count + 1
			spawns[count] = spawn -- all spawns that are at least 10m away but not too big of height difference
		end
	end
	if count ~= 0 then
		return unpack(spawns[math.random(1,count)]) -- return x, y, and z of a random spawn
	end
	return unpack(gSpawns[math.random(1,table.getn(gSpawns))]) -- just return a completely random spawn if there aren't any in our table
end

-- remove invalid algie and return if any are still alive
function validate_algie(algie)
	local index = 1
	while algie[index] do
		local ped = algie[index]
		if PedIsValid(ped) then
			index = index + 1 -- check next algie since this one is valid
		else
			table.remove(algie,index) -- remove invalid algie
		end
	end
	for _,ped in ipairs(algie) do
		if not PedIsDead(ped) then
			return true -- return true if there are still peds left
		end
	end
	return false
end

-- make peds do a running attack on the player when near
function update_algie_ped(ped)
	-- do a running attack if playing DEFAULT_KEY and near a player
	-- *but* only if we haven't ever done a running punch *or* it's been 5 seconds since
	if PedMePlaying(ped,"DEFAULT_KEY",true) and DistanceBetweenPeds3D(ped,gPlayer) < 2 and (gRunPunch == -1 or GetTimer() - gRunPunch >= 5000) then
		PedSetActionNode(ped,"/G/ACTIONS/OFFENSE/RUNNINGATTACKS/RUNNINGATTACKSDIRECT","")
		gRunPunch = GetTimer() -- save when we did this run punch so we can delay the next one
	end
end

-- delete all algie
function cleanup_algie(algie)
	for _,ped in ipairs(algie) do
		if PedIsValid(ped) then -- it is usually good practice to make sure peds are valid before doing anything with them
			PedDelete(ped)
		end
	end
end
