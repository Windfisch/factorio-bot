local maxlen = 200
local rawstr = serpent.dump(data.raw)

local j = 0
for i = 1, string.len(rawstr), maxlen do
	print (i.." / "..string.len(rawstr))
	j = j + 1
	data:extend({{
		type = "flying-text",
		name = "DATA_RAW"..j,
		time_to_live = 0,
		speed = 1,
		order = string.sub(rawstr,i,i+maxlen-1)
	}});
end
data:extend({{
	type = "flying-text",
	name = "DATA_RAW_LEN",
	time_to_live = 0,
	speed = 1,
	order = tostring(j)
}});
