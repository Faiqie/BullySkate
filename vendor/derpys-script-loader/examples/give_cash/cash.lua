-- we'll make a function to be called when the command is done
give_player_cash = function()
	PlayerAddMoney(5000) -- give the player $50
end

-- now since we made the function we can register the command
SetCommand("give_cash",give_player_cash)
