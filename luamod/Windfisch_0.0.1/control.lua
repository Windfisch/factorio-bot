require "util"
require "math"

local my_client_id = nil

client_local_data = nil -- DO NOT USE, will cause desyncs

outfile="output1.txt"

function on_init()
	print("on init")
	global.resources = {}
	global.resources.last_index = 0
	global.resources.list = {} -- might be sparse, so the #-operator won't work
	global.resources.map = {}
	global.map_area = {x1=0, y1=0, x2=0, y2=0} -- a bounding box of all charted map chunks

	global.p = {[1]={}} -- player-private data
	global.p[1].walking = {idx=1, waypoints={[1]={x=4,y=3},[2]={x=-3, y=2}, [3]={x=0, y=-4}, [4]={x=2,y=-2}}}

	global.pathfinding = {}
	global.pathfinding.map = {}

	global.n_clients = 1

	game.write_file(outfile, "", false)
end

function pos_str(pos)
	return pos.x .. "," .. pos.y
end

function aabb_str(aabb)
	return pos_str(aabb.left_top) .. ";" .. pos_str(aabb.right_bottom)
end

function writeout_prototypes()
	header = "entity_prototypes: "
	lines = {}
	for name, prot in pairs(game.entity_prototypes) do
		coll = ""
		if prot.collision_mask['player-layer'] then
			coll=coll.."P"
		else
			coll=coll.."p"
		end
		table.insert(lines, prot.name.." "..coll.." "..aabb_str(prot.collision_box))
	end
	game.write_file(outfile, header..table.concat(lines, "$").."\n", true)
end

function on_whoami()
	if client_local_data.whoami == "server" then
		writeout_prototypes()
		-- FIXME: chart all chunks
		
		client_local_data.initial_discovery={}
		client_local_data.initial_discovery.chunks = {}
		for chunk in game.surfaces[1].get_chunks() do
			table.insert(client_local_data.initial_discovery.chunks, chunk)
		end
		client_local_data.initial_discovery.n = #client_local_data.initial_discovery.chunks
		client_local_data.initial_discovery.idx = 1
	end
end

function on_tick(event)
	if (client_local_data == nil) then
		client_local_data = {}
		client_local_data.whoami = nil
		game.write_file("players_connected.txt", "server\n", true, 0) -- only on server
	end

	if client_local_data.initial_discovery then
		local id = client_local_data.initial_discovery
		local maxi = id.idx + 1 -1
		if maxi > id.n then maxi = id.n end
		
		print("initial discovery, writing "..id.idx..".."..maxi.." ("..(id.idx/id.n*100).."% done)")
		for i = id.idx, maxi do
			local chunk = id.chunks[i]
			on_chunk_generated({area={left_top={x=chunk.x*32, y=chunk.y*32}, right_bottom={x=32*chunk.x+32, y=32*chunk.y+32}}, surface=game.surfaces[1]})
		end

		id.idx = maxi+1
		if id.idx > id.n then client_local_data.initial_discovery = nil end
	end
		
	for idx, player in pairs(game.players) do
		-- if global.p[idx].walking and player.connected then
		if global.p[idx] and global.p[idx].walking and player.connected then -- TODO FIXME
			local w = global.p[idx].walking
			local pos = player.character.position
			local dest = w.waypoints[w.idx]

			local dx = dest.x - pos.x
			local dy = dest.y - pos.y

			if math.abs(dx) > 0.3 then
				if dx < 0 then dx = -1 else dx = 1 end
			else
				dx = 0
			end

			if math.abs(dy) > 0.3 then
				if dy < 0 then dy = -1 else dy = 1 end
			else
				dy = 0
			end

			local direction
			if dx < 0 then
				direction = "west"
			elseif dx == 0 then
				direction = ""
			elseif dx > 0 then
				direction = "east"
			end

			if dy < 0 then
				direction = "north"..direction
			elseif dy == 0 then
				direction = ""..direction
			elseif dy > 0 then
				direction = "south"..direction
			end

			print("waypoint "..w.idx.." of "..#w.waypoints..", pos = "..coord(pos)..", dest = "..coord(dest).. ", dx/dy="..dx.."/"..dy..", dir="..direction)

			if direction ~= "" then
				player.walking_state = {walking=true, direction=defines.direction[direction]}
			else
				w.idx = w.idx + 1
				if w.idx > #w.waypoints then
					global.p[idx].walking = nil
				end
			end
		end
	end

	if event.tick % 120 == 0 then
		local who = "?"
		if client_local_data.whoami then who = client_local_data.whoami end
		print(my_client_id.."."..who)
	end
end

function on_sector_scanned(event)
	print("sector scanned")
	print(event.radar)
end

function pos_id(x,y)
	return x.."/"..y
end

function max(list, fn)
	local highest = nil
	local highest_idx = nil

	for idx, val in ipairs(list) do
		local fnval = fn(val)
		if highest == nil or fnval > highest then
			highest = fnval
			highest_idx = idx
		end
	end

	return list[highest_idx], highest_idx
end

function on_chunk_generated(event)
	local area = event.area
	local surface = event.surface
	--print("chunk generated at ("..area.left_top.x..","..area.left_top.y..") -- ("..area.right_bottom.x..","..area.right_bottom.y..")")
	local chunk_x = area.left_top.x
	local chunk_y = area.left_top.y
	local chunk_xend = area.right_bottom.x
	local chunk_yend = area.right_bottom.y

	if surface ~= game.surfaces['nauvis'] then -- TODO we only support one surface
		print("unknown surface")
		return
	end

	if chunk_x < global.map_area.x1 then global.map_area.x1 = chunk_x end
	if chunk_y < global.map_area.y1 then global.map_area.y1 = chunk_y end
	if chunk_xend > global.map_area.x2 then global.map_area.x2 = chunk_xend end
	if chunk_yend > global.map_area.y2 then global.map_area.y2 = chunk_yend end

	--chart_chunk_for_resource(surface, area, "iron-ore")
	--chart_chunk_for_resource(surface, area, "copper-ore")
	--chart_chunk_for_resource(surface, area, "stone")
	--chart_chunk_for_resource(surface, area, "coal")
	--chart_chunk_for_resource(surface, area, "crude-oil")

	--chart_chunk_for_pathfinding(surface, area)
	
	writeout_resources(surface, area)
	writeout_objects(surface, area)
	writeout_tiles(surface, area)
end

function writeout_tiles(surface, area) -- SLOW! beastie can do ~2.8 per tick
	--if my_client_id ~= 1 then return end
	local header = "tiles "..area.left_top.x..","..area.left_top.y..";"..area.right_bottom.x..","..area.right_bottom.y..": "
	
	local tile = nil
	local line = {}
	for y = area.left_top.y, area.right_bottom.y-1 do
		for x = area.left_top.x, area.right_bottom.x-1  do
			tile = surface.get_tile(x,y)
			table.insert(line, tile.collides_with('player-layer') and "1" or "0")
		end
	end

	game.write_file(outfile, header..table.concat(line, ",").."\n", true)
end

function writeout_resources(surface, area) -- quite fast. beastie can do > 40, up to 75 per tick
	--if my_client_id ~= 1 then return end
	header = "resources "..area.left_top.x..","..area.left_top.y..";"..area.right_bottom.x..","..area.right_bottom.y..": "
	line = ''
	lines={}
	for idx, ent in pairs(surface.find_entities_filtered{area=area, type='resource'}) do
		line=line..","..ent.name.." "..ent.position.x.." "..ent.position.y
		if idx % 100 == 0 then
			table.insert(lines,line)
			line=''
		end
	end
	table.insert(lines,line)
	game.write_file(outfile, header..table.concat(lines,"").."\n", true)

	line=nil
end

function writeout_objects(surface, area)
	--if my_client_id ~= 1 then return end
	header = "objects "..area.left_top.x..","..area.left_top.y..";"..area.right_bottom.x..","..area.right_bottom.y..": "
	line = ''
	lines={}
	for idx, ent in pairs(surface.find_entities(area)) do
		if ent.prototype.collision_mask['player-layer'] then
			line=line..","..ent.name.." "..ent.position.x.." "..ent.position.y
			if idx % 100 == 0 then
				table.insert(lines,line)
				line=''
			end
		end
	end
	table.insert(lines,line)
	game.write_file(outfile, header..table.concat(lines,"").."\n", true)

	line=nil
end

function chart_chunk_for_pathfinding(surface, area)
	local map = global.pathfinding.map

	-- clear area
	for x = area.left_top.x, area.right_bottom.x do
		if map[x] then
			for y = area.left_top.y, area.right_bottom.y do
				if map[x][y] then map[x][y]=nil end
			end
		end
	end


	-- fill area
	
	-- tiles
	for x = area.left_top.x, area.right_bottom.x do
		for y = area.left_top.y, area.right_bottom.y do
			local tile = surface.get_tile(x,y)
			if tile.prototype.collision_mask['player-layer'] then
				-- this tile is an obstacle. probably water.
				if map[x]==nil then map[x]={} end

				map[x][y] = {blocked="full", cost=1337}
			end
			-- TODO walking speed modifiers
		end
	end

	local my_radius = 0.4

	-- trees increase the path cost
	local trees = surface.find_entities_filtered{type='tree', area=area}
	for _, tree in pairs(trees) do
		local x = math.floor(tree.position.x)
		local y = math.floor(tree.position.y)
		if map[x]==nil then map[x]={} end
		if map[x][y] == nil then map[x][y] = {walkthrough=false, cost=1} end

		map[x][y].cost = 1.2 * map[x][y].cost
	end

	local other = surface.find_entities_filtered{area=area}
	for _, ent in pairs(other) do
		if ent.type ~= "tree" and ent.prototype.collision_mask['player-layer'] then
			local x1,x2,y1,y2,x1f,x2c,y1f,y2c
			local pos = ent.position
			local bb = ent.prototype.collision_box
			x1 = pos.x + bb.left_top.x
			y1 = pos.y + bb.left_top.y
			x2 = pos.x + bb.right_bottom.x
			y2 = pos.y + bb.right_bottom.y

			x1f=math.floor(x1)
			y1f=math.floor(y1)
			x2c=math.ceil(x2)
			y2c=math.ceil(y2)
			
			for x=x1f,x2c do
				if not map[x] then map[x]={} end
				for y=y1f,y2c do
					if not map[x][y] then map[x][y]={} end
					map[x][y].walkthrough = false
				end
			end

			if x1-x1f > my_radius then
				for y = y1f, y2c do
					map[x1f][y].left_corridor = true
				end
			end
			
			if y1-y1f > my_radius then
				for x = x1f, x2c do
					map[x][y1f].top_corridor = true
				end
			end
			
			if x2c-x2 > my_radius then
				for y = y1f, y2c do
					map[x2c][y].right_corridor = true
				end
			end
			
			if y2c-y2 > my_radius then
				for x = x1f, x2c do
					map[x][y2c].bottom_corridor = true
				end
			end
		end
	end

	-- TODO handle enemies
	

	-- TODO handle rails/trains

end


-- searches `area` for the specified resource, groups them together and does one of:
-- a) creates a new resource-field in the globals.resources.{list,map} OR
-- b) integrates the field into a neighboring, existing field, updating the map accordingly
function chart_chunk_for_resource(surface, area, resourcename)
	local map = global.resources.map
	local ents = surface.find_entities_filtered{area=area, name=resourcename}
	local flood_fill_range

	if resourcename == 'crude-oil' then
		flood_fill_range = 41 -- oil is more sparse -> higher search radius
	else
		flood_fill_range = 5
	end

	if #ents == 0 then return end

	-- put entities in the map
	for _,ent in ipairs(ents) do
		local x = math.floor(ent.position.x)
		local y = math.floor(ent.position.y)
		
		if not map[x] then map[x]={} end

		-- TODO FIXME: check whether there is something (should not)
		map[x][y] = {id=nil, entity=ent, name=resourcename}
	end

	-- group the entities into resource patches
	for _,ent in ipairs(ents) do
		local x = math.floor(ent.position.x)
		local y = math.floor(ent.position.y)

		if map[x][y].id == nil then
			global.resources.last_index = global.resources.last_index + 1
			local amount
			local neighbors
			local patch_ents

			amount, patch_ents, neighbors = flood_fill_map(map, x,y, flood_fill_range, global.resources.last_index, resourcename)
			print("ore patch #"..global.resources.last_index.." has size "..amount)

			if set_empty(neighbors) then
				-- easiest case: create a new patch out of these entities
				local patch = {
					entities = patch_ents,
					name = resourcename
				}

				global.resources.list[global.resources.last_index] = patch
			else
				-- merge with one or multiple neighbors

				-- find largest neighboring patch
				local largest
				largest, _ = max(neighbors, function(id) return #(global.resources.list[id].entities) end)
				print("ore patch #"..global.resources.last_index.." is adjacent to "..pretty_list(neighbors).." of which "..largest.." is the largest")

				-- merge and delete all but the largest neighboring patches
				for _,id in ipairs(neighbors) do if id ~= largest then
					-- TODO some cleanup for global.resources.list[id]
					list_concat(global.resources.list[largest].entities, global.resources.list[id].entities)
					update_map(global.resources.list[id].entities, largest)
					global.resources.list[id] = nil -- delete the old patch
				end end
				
				list_concat(global.resources.list[largest].entities, patch_ents)
				update_map(patch_ents, largest)
			end
		end
	end
end

function color_hsv(h, s, v)
	local r, g, b

	local i = math.floor(h * 6);
	local f = h * 6 - i;
	local p = v * (1 - s);
	local q = v * (1 - f * s);
	local t = v * (1 - (1 - f) * s);

	i = i % 6

	if i == 0 then r, g, b = v, t, p
	elseif i == 1 then r, g, b = q, v, p
	elseif i == 2 then r, g, b = p, v, t
	elseif i == 3 then r, g, b = p, q, v
	elseif i == 4 then r, g, b = t, p, v
	elseif i == 5 then r, g, b = v, p, q
	end

	return math.floor(r * 255), math.floor(g * 255), math.floor(b * 255)
end

function color_from_id(id)
	return color_hsv(0.53481*id, 0.8 + 0.2*math.sin(id/21.324), 0.7+0.3*math.sin(id/2.13184))
end

function rangestr(area)
	return coord({x=area.x1, y=area.y1}).." -- "..coord({x=area.x2, y=area.y2})
end

function coord(pos)
	return "("..pos.x.."/"..pos.y..")"
end

function dump_map(filename, area)
	local string = ""
	if area == nil then -- automatically find the area borders
		area = global.map_area
	end

	print("dumping the map "..rangestr(area))

	string = "P3\n"..(area.x2-area.x1+1).." "..(area.y2-area.y1+1).."\n255\n#factorio AI dump\n"
	game.write_file(filename,string)
	string = ""

	local map = global.resources.map

	for y = area.y1, area.y2 do
		for x = area.x1, area.x2 do
			local r,g,b = 0,0,0

			if map[x] and map[x][y] then
				r,g,b=color_from_id(map[x][y].id)
			end

			if (x % 5 == 0) or (x == area.y2) then sep = "\n" else sep = " " end
			
			string = string .. r.." "..g.." "..b..sep

			if (sep == "\n") then
				-- lua seems to be dead slow with large strings
				game.write_file(filename,string,true)
				string=""
			end
		end
	end
	game.write_file(filename, string, true)
	print("  done")
end

-- sets the id of all entities specified in the grid to `id`
function update_map(entities, id)
	-- put entities in the map
	for _,ent in ipairs(entities) do
		local x = math.floor(ent.position.x)
		local y = math.floor(ent.position.y)
		
		global.resources.map[x][y].id = id
	end
end

function list_concat(dest, src)
	for _,val in ipairs(src) do
		table.insert(dest,val)
	end
end

function pretty_list(list)
	local result = ""
	for _,x in ipairs(list) do
		if result == "" then
			result = x
		else
			result = result..", "..x
		end
	end
	return result
end

function set_empty(set)
	-- lua sucks. again.
	if next(set) == nil then
		return true
	else
		return false
	end
end

function set_size(set)
	-- LUAAAAAAAA Y U DO DIS?!
	local len = 0
	for _,_ in set do
		len = len +1
	end
	return len
end 

function pretty_set(set)
	local result = ""
	for x,_ in pairs(set) do
		if result == "" then
			result = x
		else
			result = result..", "..x
		end
	end
	return result
end

function set_insert(set, val)
	for _,v in pairs(set) do
		if v==val then return end
	end
	table.insert(set,val)
end

function flood_fill_map(grid, x, y, kernel, val, resourcename)
	local todo = {}
	local todo_r = 0
	local todo_w = 1
	todo[0] = {x=x,y=y}

	local neighbors = {}
	local ents = {}

	local kernel_x0 = -math.floor(kernel / 2) -- inclusive
	local kernel_x1 = kernel + kernel_x0 - 1  -- inclusive
	local kernel_y0 = -math.floor(kernel / 2) -- inclusive
	local kernel_y1 = kernel + kernel_y0 - 1  -- inclusive

	while todo_r < todo_w do
		local curr = todo[todo_r]
		todo_r = todo_r + 1

		grid[curr.x][curr.y].id = val
		grid[curr.x][curr.y].floodfillstate = "done"

		table.insert(ents, grid[curr.x][curr.y].entity)

		-- check the neighborhood
		for kern_x = kernel_x0, kernel_y1 do
			for kern_y = kernel_y0, kernel_y1 do
				local xx = curr.x + kern_x
				local yy = curr.y + kern_y

				if grid[xx] and grid[xx][yy] and grid[xx][yy].name == resourcename then
					if grid[xx][yy].id and grid[xx][yy].id ~= val then
						-- we're adjacent to an already-known patch.
						-- record this finding in the neighbors table
						set_insert(neighbors, grid[xx][yy].id)
					elseif grid[xx][yy].floodfillstate == nil then
						-- some unseen-yet entity is adjacent to us.
						-- add them to the pending set
						todo[todo_w] = {x=xx, y=yy}
						grid[xx][yy].floodfillstate = "pending"
						todo_w = todo_w + 1
					end
				end
			end
		end
	end

	return todo_w, ents, neighbors
end

function dump_grid(grid)
	for i = 0,31 do
		local out = ""
		for j = 0,31 do
			local g = grid[j][i]
			if g == nil then
				out=out.." ."
			elseif g.id == nil then
				out=out.." *"
			else
				out=out..string.format("%2.0f",g.id)
			end
		end
		print(out)
	end
end

function array2d(xlen, ylen, margin, init)
	array={}
	for j = -margin,(ylen-1+margin) do
		array[j]={}
	end

	for j = 0,(ylen-1) do
		if (init ~= nil) then
			for i = 0,(xlen-1) do
				array[j][i] = init
			end
		end
	end
	return array
end

function on_load()
	print("on load")
	print(game)
	print(global.n_clients)
	my_client_id = global.n_clients
end

function on_player_joined_game(event)
	print("player '"..game.players[event.player_index].name.."' joined")
		
	game.write_file("players_connected.txt", game.players[event.player_index].name..'\n', true, 0) -- only on server
	
	global.n_clients = global.n_clients + 1
end

script.on_init(on_init)
script.on_load(on_load)
script.on_event(defines.events.on_tick, on_tick)
script.on_event(defines.events.on_player_joined_game, on_player_joined_game)
script.on_event(defines.events.on_sector_scanned, on_sector_scanned)
script.on_event(defines.events.on_chunk_generated, on_chunk_generated)

function rcon_test(foo)
	print("rcon_test("..foo..")")
end

function rcon_set_waypoints(waypoints) -- e.g. waypoints= { {0,0}, {3,3}, {42,1337} }
	tmp = {}
	for i = 1, #waypoints do
		tmp[i] = {x=waypoints[i][1], y=waypoints[i][2]}
	end
	global.p[1].walking = {idx=1, waypoints=tmp}
end

function rcon_whoami(who)
	if client_local_data.whoami == nil then
		client_local_data.whoami = who
		on_whoami()
	end
end

remote.add_interface("windfisch", {
	test=rcon_test,
	set_waypoints=rcon_set_waypoints,
	whoami=rcon_whoami
})
