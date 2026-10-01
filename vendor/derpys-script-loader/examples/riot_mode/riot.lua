-- if "MissionSetup" exists, it is run automatically before "main"
-- we'll use it to make sure the game is ready
function MissionSetup()
	while not SystemIsReady() do
		Wait(0) -- wait until the system *is* ready
	end
end

-- if "main" exists, it is run automatically (but only after "MissionSetup" is done, or doesn't exist)
-- we'll keep it small and just make it call other functions that do most the work
function main()
	while true do -- this loop will run forever since the condition is true
		spawn_more_peds()
		make_peds_riot()
		Wait(0) -- wait one frame (0 ms = 1 frame)
	end
end

-- we manually call the "spawn_more_peds" function from "main", so it should run every frame after the game is ready
-- it will be used to make more peds spawn so there is more of a chance to find peds fighting
function spawn_more_peds()
	-- the simplest way to spawn more peds is to override the ambient population
	-- the first number is the "total" amount of peds allowed, then each following number is for each faction
	--                     Total, Prefects, Nerds, Jocks, Dropouts, Greasers, Preppies, Students, Cops, Teachers, Townpeople, Shopkeep, Bully
	AreaOverridePopulation(20,    0,        3,     3,     3,        3,        3,        2,        0,    0,        1,          0,        2)
end

-- we manually call the "make_peds_riot" function from "main", so it should run every frame after the game is ready
-- we'll make peds hate each other and instantly start fighting nearby peds
function make_peds_riot()
	for ped in AllPeds() do -- for each ped...
		local faction = PedGetFaction(ped)
		if ped ~= gPlayer and faction ~= 0 and faction ~= 7 then -- if they're not Jimmy, a prefect, or a cop...
			for rival = 0,13 do
				if rival ~= faction or rival == 6 then
					PedSetPedToTypeAttitude(ped,rival,0) -- make them hate all factions (besides their own, unless they're a non-clique)
				end
			end
			if not PedIsInCombat(ped) then
				start_ped_fighting(ped) -- and make them fight nearby peds if not already in combat
			end
		end
	end
end

-- notice how making new functions for each job keeps our script organized, even though we *could* have just put it all in one function
-- this function will be used to make a ped fight a nearby ped
function start_ped_fighting(ped)
	local nearest = -1 -- set nearest to an invalid ped to begin with
	local distance = 15 -- maximum distance of 15 meters away
	for other in AllPeds() do
		if other ~= ped and PedGetPedToTypeAttitude(ped,PedGetFaction(other)) == 0 then
			local dist = DistanceBetweenPeds3D(ped,other) -- get the distance from our ped to each hated ped
			if dist < distance then
				nearest,distance = other,dist -- if this other ped is closer, save it (so that by the end of the loop we have the nearest)
			end
		end
	end
	if PedIsValid(nearest) then -- if we found a nearby ped, let's make the ped fight them
		PedSetEmotionTowardsPed(ped,nearest,0)
		PedAttack(ped,nearest,1)
	end
end
