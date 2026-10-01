-- we make a custom version of ShopAddItem to give to the SStores.lua script
function F_ShopAddItem(...)
	arg[5] = 0 -- the 5th argument is price, so we just make that 0
	return ShopAddItem(unpack(arg))
end

-- register an event handler to run when a native game script is loaded
RegisterLocalEventHandler("NativeScriptLoaded",function(name,env)
	if name == "SStores.lua" then
		-- usually, SStores.lua uses the ShopAddItem function to add items
		-- but we'll make it use our custom version by putting it in its environment
		env.ShopAddItem = F_ShopAddItem
	end
end)
