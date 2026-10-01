-- save player coords with /pos
-- saves in this format: x, y, z, heading

SetCommand("pos",function(comment)
	local h,x,y,z = math.deg(PedGetHeading(gPlayer)),PlayerGetPosXYZ()
	PrintOutput(string.format("%.3f, %.3f, %.3f, %.1f",x,y,z,h))
	if CanWriteFiles() and not IsScriptZipped() then
		local file = OpenFile("coords.txt","ab")
		if comment then
			WriteFile(file,string.format("%.3f,%.3f,%.3f,%.1f -- %s\r\n",x,y,z,h,comment))
		else
			WriteFile(file,string.format("%.3f,%.3f,%.3f,%.1f\r\n",x,y,z,h))
		end
		CloseFile(file)
	end
end,true,"Usage: pos [comment]\nShow the player's position and heading, and attempt to dump it in a file.")
