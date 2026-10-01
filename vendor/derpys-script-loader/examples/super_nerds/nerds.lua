gNerds = {} -- peds marked as nerd

-- main
function main()
	while not SystemIsReady() do
		Wait(0) -- wait for the system to be ready
	end
	while true do
		for ped in pairs(gNerds) do
			if not PedIsValid(ped) then
				gNerds[ped] = nil -- remove invalid peds from the table
			end
		end
		for ped in AllPeds() do
			if not gNerds[ped] and PedGetFaction(ped) == 1 then -- if a thread hasn't been made for this ped and they're a nerd...
				GameSetPedStat(ped,6,0) -- set fearless
				GameSetPedStat(ped,7,0) -- set not a chicken
				GameSetPedStat(ped,8,100) -- set high attack frequency
				GameSetPedStat(ped,14,100) -- set super aggressive
				PedSetMaxHealth(ped,PedGetMaxHealth(ped)*3) -- 3x health
				PedSetHealth(ped,PedGetMaxHealth(ped))
				gNerds[ped] = CreateThread("T_Nerd",ped) -- and make a thread for them
			end
		end
		Wait(0)
	end
end

-- nerd thread (created for each nerd ped)
function T_Nerd(ped)
	while PedIsValid(ped) do
		local target = PedGetTargetPed(ped)
		if PedIsValid(target) and PedIsInCombat(ped) and PedMePlaying(ped,"DEFAULT_KEY",true) and PedGetNodeTime(ped) >= 0.5 and DistanceBetweenPeds3D(ped,target) < 1.8 then
			-- if the nerd has a valid target, is in combat, is playing DEFAULT_KEY (not attacking, being hit, or doing anything),
			-- has been playing it for 0.5 seconds, and is nearby... then we will play a custom attack
			set_nerd_attack(ped)
		elseif PedIsPlaying(ped,"/G/ACTIONS/GRAPPLES/FRONT",true) and PedMePlaying(ped,"RCV",true) then
			-- if the nerd is grappled, we'll play a random reversal node
			set_nerd_reversal(ped)
		end
		Wait(0)
	end
end
function set_nerd_attack(ped)
	PedSetActionNode(ped,unpack(RandomTableElement({ -- we unpack the random table we get from our table of tables, thus picking a random action node and tree
		{"/G/B_STRIKER_A/SHORT/STRIKES/LIGHTATTACKS/WINDMILL_R","B_STRIKER_A.ACT"},
		{"/G/BOSS_DARBY/OFFENSE/SHORT/GRAPPLES/HEAVYATTACKS/CATCH_THROW","BOSS_DARBY.ACT"},
		{"/G/DO_STRIKER_A/OFFENSE/SHORT/DO_STRIKECOMBO/LIGHTATTACKS/PUNCH1","DO_STRIKER_A.ACT"},
		{"/G/G_GRAPPLER_A/OFFENSE/SHORT/STRIKES/HEAVYATTACKS/BOOTKICK","G_GRAPPLER_A.ACT"},
		{"/G/G_JOHNNY/OFFENSE/SPECIAL/SPECIALACTIONS/GRAPPLES/DASH","G_JOHNNY.ACT"},
		{"/G/J_MELEE_A/OFFENSE/SHORT/STRIKES/LIGHTATTACKS/RIGHTHAND","G_JOHNNY.ACT"},
		{"/G/NEMESIS/OFFENSE/MEDIUM/STRIKES/LIGHTATTACKS/OVERHANDR","NEMESIS.ACT"},
	})))
end
function set_nerd_reversal(ped)
	local reversals = {"BACKBREAKER","BEARHUG","BODYSLAM","POWERBOMB"}
	local node = "/G/ACTIONS/GRAPPLES/FRONT/GRAPPLES/GRAPPLEMOVES/"..reversals[math.random(1,table.getn(reversals))] -- a random reversal node
	PedSetActionNode(ped,node,"") -- the 3rd argument is an empty string since we don't need to load an action tree (actions are pretty much always loaded)
end

-- keep ped stats (this event is activated whenever the game tries to change the value of a ped's stat)
RegisterLocalEventHandler("PedStatOverriding",function(ped,stat)
	if not gNerds[ped] then
		return true -- by returning true, we don't allow the stat for this ped to be changed (since they're marked as a super nerd)
	end
end)
