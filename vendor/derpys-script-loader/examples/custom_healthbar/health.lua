HUD_R0, HUD_G0, HUD_B0 = 0, 0, 0 -- 0% health
HUD_R1, HUD_G1, HUD_B1 = 230, 37, 37 -- 100% health
HUD_R2, HUD_G2, HUD_B2 = 230, 37, 170 -- 200% health
HUD_SHOW_TIME = 7000
HUD_FADE_OUT = 400
HUD_FADE_IN = 80
HUD_X, HUD_Y = 0.825, 0.3
HUD_SCALE = 0.15
HUD_ALPHA = 191

function MissionSetup()
	while not SystemIsReady() do
		Wait(0)
	end
end
function main()
	local texture = CreateTexture("heart.png") -- load a texture (can only be PNG)
	local hp = PlayerGetHealth()
	local timer = GetTimer()
	local lastframe
	local frame = 0
	local alpha = 0
	local shown
	while true do
		local newhp = PlayerGetHealth()
		if newhp ~= hp or PedIsHit(gPlayer, 2, 100) then
			shown = timer -- show when we are hit or our health changes
			hp = newhp
		elseif shown and isAnyoneNearbyFighting(gPlayer) then
			shown = timer -- keep showing while we are fighting
		elseif IsButtonBeingPressed(0, 0) or IsButtonBeingPressed(1, 0) then
			shown = timer -- show when we hit the mission buttons
		end
		if shown and not AreaIsLoading() then
			local passed = timer - shown
			if passed >= HUD_SHOW_TIME then
				alpha, shown = 0
			elseif passed > HUD_SHOW_TIME - HUD_FADE_OUT then
				alpha = (HUD_SHOW_TIME - passed) / HUD_FADE_OUT
				drawHealthForPed(gPlayer, texture, alpha) -- show fading out
			elseif alpha ~= 1 then
				alpha = math.min(1, alpha + frame / HUD_FADE_IN)
				drawHealthForPed(gPlayer, texture, alpha) -- show fading in
			else
				drawHealthForPed(gPlayer, texture, alpha) -- show solid
			end
		end
		Wait(0)
		lastframe = timer
		timer = GetTimer()
		frame = timer - lastframe
	end
end
function isAnyoneNearbyFighting(ped)
	local x, y, z = PedGetPosXYZ(ped)
	local peds = {PedFindInAreaXYZ(x, y, z, 10)}
	if table.remove(peds, 1) then
		for _, anyone in ipairs(peds) do
			if anyone ~= ped and PedIsValid(anyone) and not PedIsDead(anyone) and PedIsInCombat(anyone) and PedGetTargetPed(anyone) == ped then
				return true -- somebody is fighting ped
			end
		end
	end
	return false -- nobody is fighting ped
end
function drawHealthForPed(ped, texture, alpha)
	local ratio = GetTextureDisplayAspectRatio(texture) -- this function helps preserve a texture's aspect ratio if you use this value * the desired height for the width
	local hp = PedGetHealth(ped) / PedGetMaxHealth(ped)
	if hp < 1 then
		if hp < 0 then
			hp = 0
		end
		SetTextureBounds(texture, 0, 0, 1, 1-hp)
		DrawTexture(texture, HUD_X, HUD_Y, HUD_SCALE*ratio, (1-hp)*HUD_SCALE, HUD_R0, HUD_G0, HUD_B0, HUD_ALPHA*0.6*alpha) -- black background heart
	elseif hp > 1 then
		hp = math.min(hp-1, 1)
		SetTextureBounds(texture, 0, 0, 1, 1-hp)
		DrawTexture(texture, HUD_X, HUD_Y, HUD_SCALE*ratio, (1-hp)*HUD_SCALE, HUD_R1, HUD_G1, HUD_B1, HUD_ALPHA*alpha) -- red 100% health
		SetTextureBounds(texture, 0, 1-hp, 1, 1)
		DrawTexture(texture, HUD_X, HUD_Y+(1-hp)*HUD_SCALE, HUD_SCALE*ratio, hp*HUD_SCALE, HUD_R2, HUD_G2, HUD_B2, HUD_ALPHA*alpha) -- yellow 200% health
		return
	end
	-- setting texture bounds sets normalized UV coords to use with this texture
	SetTextureBounds(texture, 0, 1-hp, 1, 1)
	-- drawing a texture is just like drawing a rectangle, except it takes a texture argument at the start
	DrawTexture(texture, HUD_X, HUD_Y+(1-hp)*HUD_SCALE, HUD_SCALE*ratio, hp*HUD_SCALE, HUD_R1, HUD_G1, HUD_B1, HUD_ALPHA*alpha) -- red 100% health
end
