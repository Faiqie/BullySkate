gSave = GetSaveDataTable("trash_loot") -- make sure this string is unique. the table returned by this function gets saved with the game save file.

-- if "main" exists, it is run automatically
function main()
	if is_save_corrupted() then
		PrintWarning("save data was corrupted, so it will be reset")
		for k in pairs(gSave) do
			gSave[k] = nil -- clear out the whole table since it was corrupt
		end
	end
	while not SystemIsReady() do
		Wait(0) -- wait until the system is ready
	end
	while true do
		if PedIsPlaying(gPlayer,"/G/PLAYER/BED/GETOUTBED/GETOUTBED",true) or (PedIsPlaying(gPlayer,"/G/PLAYER/DEFAULT_KEY/LOCOMOTION",true) and PedMePlaying(gPlayer,"EXHAUSTED_COLLAPSE",true)) then
			for k in pairs(gSave) do
				gSave[k] = nil -- clear out the looted positions since we slept
			end
		elseif PedIsPlaying(gPlayer,"/G/WPROPS/GARBINTERACT",true) then
			PedSetActionNode(gPlayer,"/G/1_07/PICKUPITEM","1_07.ACT")
			while PedIsPlaying(gPlayer,"/G/1_07/PICKUPITEM",true) do
				if PedGetNodeTime(gPlayer) >= 0.85 then
					if use_trash_can() then
						get_random_item()
					end
					break
				end
				Wait(0)
			end
		end
		Wait(0)
	end
end

-- check save data (as it could be modified wrongly by another script or by editing the save file)
function is_save_corrupted()
	-- we'll use our save table as a list of positions that were looted today (cans are identified by general position)
	for _,pos in ipairs(gSave) do
		if type(pos) ~= "table" or type(pos[1]) ~= "number" or type(pos[2]) ~= "number" or type(pos[3]) ~= "number" then
			return true
		end
	end
	return false
end

-- return true if the nearby trash can is lootable, and mark it as looted
function use_trash_can()
	local x1,y1,z1 = PlayerGetPosXYZ()
	for _,pos in ipairs(gSave) do
		-- we identify trash cans by general position, so if we're within 5 m of one then we return false (since we already used this one)
		if DistanceBetweenCoords3d(x1,y1,z1,unpack(pos)) < 5 then
			return false
		end
	end
	-- if we haven't used a can nearby, then mark this position as looted and return true
	table.insert(gSave,{x1,y1,z1})
	return true
end

-- give the player a random item for trash can loot
function get_random_item()
	local chance = math.random(1,100)
	if chance <= 85 then
		get_random_weapon() -- 85% chance of weapon
	elseif chance <= 98 then
		PlayerAddMoney(math.random(25,500)) -- 13% chance of 25 cents to 5 dollars
	else
		PlayerAddMoney(math.random(100,10000)) -- 2% chance for $1 to $100
	end
end
function get_random_weapon()
	local chance = math.random(1,100)
	if chance <= 65 then -- 65% chance for random weapon
		local weapons = {299,300,301,302,305,307,309,311,314,317,320,321,323,327,338,348,363,372,394,396,397,399,405,410,413,414,415,416,420,432}
		PedSetWeapon(gPlayer,weapons[math.random(1,table.getn(weapons))],1,false)
	elseif chance <= 80 then -- 15% chance for random ammo
		local ammo = {308,316}
		GiveAmmoToPlayer(ammo[math.random(1,table.getn(ammo))],math.random(1,8),false)
	else -- 20% chance for a food item
		local weapons = {310,312,316,358}
		PedSetWeapon(gPlayer,weapons[math.random(1,table.getn(weapons))],1,false)
	end
end
