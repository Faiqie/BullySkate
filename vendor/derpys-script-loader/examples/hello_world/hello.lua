-- the "main" function will run automatically
function main()
	while not SystemIsReady() do
		Wait(0) -- wait until the system is ready
	end
	print("Hello, world!") -- will show up in the console
	while true do
		TextPrintString("Hello, world!",0,1) -- draw some text for 0 ms (1 frame) using style 1 (top of the screen)
		Wait(0) -- and wait 1 frame (without waiting, our loop would just run forever and the game would freeze)
	end
end
