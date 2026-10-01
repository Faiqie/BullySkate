-- DERPY'S SCRIPT LOADER: ANIMATION LOADER
--  this is a built-in script that loads all animation groups using seperate script objects so other scripts don't have to worry about it and potentially corrupt memory

gAgrPerScript = 16
gPrintLoading = false
gPrintUnloading = false
gExcludedGroups = {} -- ["group"] = true
gAnimationGroups = { -- all 550 animation groups
	"1_02BYourSchool","1_02_MeetWithGary","1_03The Setup","1_04TheSlingshot","1_06ALittleHelp","1_07_SaveBucky","1_07_Sk8Board","1_08ThatBitch","1_08_MandPuke","1_09_Candidate",
	"1_10Betrayal","1_11B_HeBigPrank","1_G1_TheDiary","1_S01HatVsGall","2_01LastMinuteShop","2_02ComicKlepto","2_05TadsHouse","2_06MovieTickets","2_07BeachRumble","2_08WeedKiller",
	"2_4RichAreaRace","2_G2CarnivalDate","2_G2_GiftExchange","2_R03PaperRoute","2_S02CharSheets","2_S04CharSheets","2_S05_CooksCrush","2_S06PantyRaid","3_01JealousJohnny","3_04WrongPtTown",
	"3_05TheTenements","3_BFightJohnnyV","3_G3","3_R05ChemicalDeliv","3_R08RaceLeague","3_S03CheatinTime","4_01Paparazzi","4_04_FunhouseFun","4_06BigGame","4_B2_JockBossBattle",
	"5_01Grp","5_01Rats","5_02PrVandalized","5_05Zoe","5_09MakingAMark","6B_PARA","AGymLght","Ambient","Ambient2","Ambient3",
	"ANIBBALL","AniBroom","AniDice","AniFooty","AniGlobe","AnimSave","AniPillo","ARC3D","Area_Asylum","Area_Funhouse",
	"Area_GirlsDorm","Area_Infirmary","Area_School","Area_Tenements","Armor","AsyBars","AsyDoorB","AsyDoors","AsyGate","AsyLever",
	"AsySwtch","AtcPlank","Authority","BANANA","barelLad","BarrGate","BATON","BBALL_21","bbgun","BCatcher",
	"BdrDoorL","BeardLady","Bike","BikeGar","BoldRoll","BoltCutt","Boxing","BoxRopes","BRDoor","BrkSwtch",
	"BROCKETL","BRSwitch","BusDoors","Butcher","BXPBag","B_Striker","CarnCurt","CARNI01","carnies","Car_Ham",
	"Cavalier","Cheer_Cool1","Cheer_Cool2","Cheer_Cool3","Cheer_Gen1","Cheer_Gen2","Cheer_Gen3","Cheer_Girl1","Cheer_Girl2","Cheer_Girl3",
	"Cheer_Girl4","Cheer_Nerd1","Cheer_Nerd2","Cheer_Nerd3","Cheer_Posh1","Cheer_Posh2","Cheer_Posh3","Chem_Set","ChLead_Idle","CLadderA",
	"CnGate","Coaster","COPBIKE","Cop_Frisk","CV_Female","CV_Male","C_Wrestling","DartBrd","DartCab","DodgeBall",
	"DodgeBall2","DoorStr1","DO_Edgar","DO_Grap","DO_StrikeCombo","DO_Striker","DRBrace","Drumming","DuffBag","DunkBttn",
	"DunkSeat","Earnest","EnglishClass","ErrandCrab","Errand_BUS","Errand_IND","Errand_RIC","Errand_SCH","ESCDoorL","ESCDoorR",
	"ExtWind","FDoor","FDoorB","FDoorC","Ferris","FGhost","FGoblin","FlagA","FLbBook","FlbLader",
	"FLbPaint","FLbTable","FMCntrl","FMDoor","FMTrapDr","FMTrapSw","FortTell","funCart","funCurtn","funMiner",
	"funRocks","FunTeeth","FXTestG","F_Adult","F_BULLY","F_Crazy","F_Douts","F_Girls","F_Greas","F_Jocks",
	"F_Nerds","F_OldPeds","F_Pref","F_Preps","GarbCanA","GatCool","GEN_SOCIAL","Gfight","GhostDrs","Gift",
	"Go_Cart","Grap","GymHoop","GymWLad","G_Grappler","G_Johnny","G_Striker","Halloween","HallWind","Hang_Jock",
	"Hang_Moshing","Hang_Talking","Hang_Workout","Hobos","Hobo_Cheer","HSdinger","HUMIL_4-10_B","HUMIL_4-10_C","HUMIL_5-8F_A","HUMIL_5-8F_B",
	"HUMIL_5-8V4-10","HUMIL_5-8V6-1","HUMIL_5-8VPLY","HUMIL_5-8_A","HUMIL_5-8_B","HUMIL_5-8_C","HUMIL_6-1V4-10","HUMIL_6-1V6-1","HUMIL_6-1VPLY","HUMIL_6-1_A",
	"HUMIL_6-1_B","HUMIL_6-1_C","HUMIL_6-5V4-10","HUMIL_6-5V6-1","HUMIL_6-5VPLY","HUMIL_6-5_A","HUMIL_6-5_B","HUMIL_6-5_C","IDLE_AUTH_A","IDLE_AUTH_B",
	"IDLE_AUTH_C","IDLE_AUTH_D","IDLE_BULLY_A","IDLE_BULLY_B","IDLE_BULLY_C","IDLE_BULLY_D","IDLE_CIVF_A","IDLE_CIVF_B","IDLE_CIVF_C","IDLE_CIVM_A",
	"IDLE_CIVM_B","IDLE_CIVM_C","IDLE_DOUT_A","IDLE_DOUT_B","IDLE_DOUT_C","IDLE_DOUT_D","IDLE_FATG_A","IDLE_FATG_B","IDLE_FATG_C","IDLE_FAT_A",
	"IDLE_FAT_B","IDLE_FAT_C","IDLE_GREAS_A","IDLE_GREAS_B","IDLE_GREAS_C","IDLE_GREAS_D","IDLE_GSF_A","IDLE_GSF_B","IDLE_GSF_C","IDLE_GSM_A",
	"IDLE_GSM_B","IDLE_GSM_C","IDLE_JOCK_A","IDLE_JOCK_B","IDLE_JOCK_C","IDLE_JOCK_D","IDLE_NERD_A","IDLE_NERD_B","IDLE_NERD_C","IDLE_NERD_D",
	"IDLE_NGIRL","IDLE_PREP_A","IDLE_PREP_B","IDLE_PREP_C","IDLE_PREP_D","IDLE_SEXY_A","IDLE_SEXY_B","IDLE_SEXY_C","INDgateC","JPhoto",
	"JunkCarA","JV_Asylum","J_Damon","J_Grappler","J_Melee","J_Ranged","J_Striker","KISS1","KISS2","KISS3",
	"KISS4","KissAdult","KISSB","KISSF","LckrGymA","LE_Officer","LE_Orderly","Mermaid","MG_Craps","MINIBIKE",
	"MINICHEM","MINIDARTS","MINIDunk","MINIGraf","MINIHACKY","MINI_Arm","MINI_BallToss","MINI_Lock","MINI_React","Miracle",
	"MOWER","MPostA","N2B Dishonerable","Nemesis","nerdBar1","NIS_0_00A","NIS_1_02","NIS_1_02B","NIS_1_03","NIS_1_04",
	"NIS_1_05","NIS_1_07","NIS_1_08_1","NIS_1_09","NIS_1_11","NIS_2_01","NIS_2_03","NIS_2_04","NIS_2_06_1","NIS_2_B",
	"NIS_2_S04","NIS_3_01","NIS_3_02","NIS_3_04","NIS_3_05","NIS_3_06","NIS_3_08","NIS_3_11","NIS_3_B","NIS_3_G3",
	"NIS_3_R09_D","NIS_3_R09_G","NIS_3_R09_J","NIS_3_R09_N","NIS_3_R09_P","NIS_3_S03","NIS_3_S03_B","NIS_3_S11","NIS_4_01","NIS_4_05",
	"NIS_4_06","NIS_4_B2","NIS_5_01","NIS_5_02","NIS_5_03","NIS_5_04","NIS_5_05","NIS_5_07","NIS_5_G5","NIS_6_02",
	"NIS_6_03","NLock01A","NPC_Adult","NPC_AggroTaunt","NPC_Chat_1","NPC_Chat_2","NPC_Chat_F","NPC_Cheering","NPC_Love","NPC_Mascot",
	"NPC_NeedsResolving","NPC_Principal","NPC_Shopping","NPC_Spectator","N_Ranged","N_Striker","N_Striker_A","N_Striker_B","ObsDoor","OBSMotor",
	"ObsPtf_1","ObsPtf_2","Pageant","PedCoaster","Player_Tired","Player_VTired","POI_Booktease","POI_Cafeteria","POI_ChLead","POI_Gen",
	"POI_Smoking","POI_Telloff","POI_WarmHands","POI_Worker","PortaPoo","PrepDoor","PunchBag","pxHoop","pxLad10M","Px_Arcade",
	"Px_Bed","Px_Fountain","Px_Garb","Px_Gen","Px_Ladr","Px_Rail","Px_RedButton","Px_Sink","Px_Tlet","Px_Tree",
	"P_Grappler","P_Striker","QPed","RAT_PED","Reeper","RMailbox","Russell","Russell_PBomb","Santa_Lap","SAUTH_A",
	"SAUTH_F","SAUTH_U","SAUTH_X","SBarels1","SBULL_A","SBULL_F","SBULL_S","SBULL_U","SBULL_X","Scaffold",
	"SCbanpil","SCBell","SCDoor","ScGate","SCgrdoor","scObsDr","ScoolBus","SCOOTER","SecDoorL","SecDoorR",
	"Sedan","SFAT_A","SFAT_F","SFAT_I","SFAT_S","SGEN_A","SGEN_F","SGEN_I","SGEN_S","SGIRLS",
	"SGIRL_A","SGIRL_D","SGIRL_F","SGIRL_S","SGTargB","ShopBike","SHUMIL_01","SHWR","SIAMESE","Siamese2",
	"Sitting_Boys","SK8Board","Skateboard","SkeltonMan","Slingsh","SNERD_A","SNERD_F","SNERD_I","SNERD_S","SNGIRLS",
	"SNGIRL_D","SNGIRL_F","SnowBlob","SnowMND","SnowWall","SOLD_A","SOLD_F","SOLD_I","SOLD_S","SPLAY_A",
	"SPLAY_B","SprayCan","SpudG","Squid","StalDoor","Straf_Dout","Straf_Fat","Straf_Female","Straf_Male","Straf_Nerd",
	"Straf_Prep","Straf_Savage","Straf_Wrest","SUV","TadGates","TadShud","TE_Female","TGKFlag","ToolBox","TrackSW",
	"TreeFall","Truck","Try_Clothes","TSGate","UBO","Umbrella","VDMilo","VFlytrap","V_Bike","V_Bike_Races",
	"V_COPBIKE","V_SCOOTER","WBalloon","WeaponUnlock","Ween_Fem","WHCrane","WheelBrl","WPCannon","WPSheldB","WPShield",
	"WPTurret","W_BBall","W_BBallBat","W_BRocket","W_Camera","W_CherryBomb","W_CHShield","W_FlashLight","W_Fountain","W_Itchpowder",
	"W_JBroom","W_Lid","W_PooBag","W_PRANK","W_Slingshot","W_Snowball","W_snowshwl","W_SprayCan","W_SpudGun","W_Stick",
	"W_Thrown","W_wtrpipe","x_cas1","x_cas2","x_cas3","x_ccane","X_Chair","x_cndl","x_sleigh","x_tedy"
}
function main()
	local groups = table.getn(gAnimationGroups)
	local scripts = math.ceil(groups / gAgrPerScript)
	while not SystemIsReady() do -- we should not load any animation groups before the system is ready
		Wait(0)
	end
	for offset = 1,scripts do
		StartVirtualScript("agr"..offset,function() -- only a few animation groups can be loaded by a single script, so we make many "virtual" scripts
			for index = 1,gAgrPerScript do
				local agr = gAnimationGroups[(offset-1)*gAgrPerScript+index]
				if not agr then
					break
				end
				if not gExcludedGroups[agr] then
					if gPrintLoading then
						PrintOutput("[SCRIPT "..offset.." | AGR "..((offset-1)*gAgrPerScript+index).."] "..agr)
					end
					LoadAnimationGroup(agr)
				end
			end
		end)
	end
	while gPrintUnloading do
		local index = 1
		local group = gAnimationGroups[index]
		while group do
			if HasAnimGroupLoaded(group) then
				index = index + 1
			elseif not gExcludedGroups[group] then
				PrintWarning("[AGR "..index.."] "..group.." unloaded")
				table.remove(gAnimationGroups,index)
			end
			group = gAnimationGroups[index]
		end
		Wait(0)
	end
	gAnimationGroups = nil -- removing references to big tables will let them be free'd from memory
	gExcludedGroups = nil
	while gDerpyScriptLoader < 3 do
		Wait(0) -- before DSL3, main needs to stay alive to keep the script from shutting down
	end
end
