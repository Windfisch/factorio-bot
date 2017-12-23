-- This lua stub is dual-licensed under both the
-- terms of the MIT and the GPL3 license. You can choose which
-- of those licenses you want to use. Note that the MIT option
-- only applies to this file, and not to the rest of
-- factorio-bot (unless stated otherwise).



-- Copyright (c) 2017 Florian Jung
-- 
-- This file is part of factorio-bot.
-- 
-- factorio-bot is free software: you can redistribute it and/or
-- modify it under the terms of the GNU General Public License,
-- version 3, as published by the Free Software Foundation.
-- 
-- factorio-bot is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
-- 
-- You should have received a copy of the GNU General Public License
-- along with factorio-bot. If not, see <http://www.gnu.org/licenses/>.



-- MIT License
-- 
-- Copyright (c) 2017 Florian Jung
-- 
-- Permission is hereby granted, free of charge, to any person obtaining a
-- copy of this factorio lua stub and associated
-- documentation files (the "Software"), to deal in the Software without
-- restriction, including without limitation the rights to use, copy, modify,
-- merge, publish, distribute, sublicense, and/or sell copies of the
-- Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
-- 
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
-- 
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
-- THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
-- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
-- DEALINGS IN THE SOFTWARE.


require "util"
require "math"

local my_client_id = nil

client_local_data = nil -- DO NOT USE, will cause desyncs

outfile="output1.txt"
must_write_initstuff = true

local todo_next_tick = {}
local crafting_queue = {} -- array of lists. crafting_queue[player_idx] is a list

function complain(text)
	print(text)
	game.forces["player"].print(text)
end

function on_init()
	print("on init")
	global.resources = {}
	global.resources.last_index = 0
	global.resources.list = {} -- might be sparse, so the #-operator won't work
	global.resources.map = {}
	global.map_area = {x1=0, y1=0, x2=0, y2=0} -- a bounding box of all charted map chunks

	global.p = {[1]={}} -- player-private data

	global.pathfinding = {}
	global.pathfinding.map = {}

	global.n_clients = 1

	game.write_file(outfile, "", false)
end

function write_file(data)
	if must_write_initstuff then
		must_write_initstuff = false
		writeout_initial_stuff()
	end

	game.write_file(outfile, data, true)
end


function pos_str(pos)
	if #pos ~= 2 then
		return pos.x .. "," .. pos.y
	else
		return pos[1] .. "," .. pos[2]
	end
end

function aabb_str(aabb)
	return pos_str(aabb.left_top) .. ";" .. pos_str(aabb.right_bottom)
end

function distance(a,b)
	local x1
	local y1
	local x2
	local y2

	if a.x ~= nil then x1 = a.x else x1=a[1] end
	if a.y ~= nil then y1 = a.y else y1=a[2] end
	if b.x ~= nil then x2 = b.x else x2=b[1] end
	if b.y ~= nil then y2 = b.y else y2=b[2] end

	return math.sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2))
end

function writeout_initial_stuff()
	writeout_pictures()
	writeout_entity_prototypes()
	writeout_item_prototypes()
	writeout_recipes()
	write_file("STATIC_DATA_END\n")
end

function writeout_proto_picture_dir(name, dir, picspec)
	if picspec.layers ~= nil then
		return writeout_proto_picture_dir(name, dir, picspec.layers[1])
	elseif picspec.hr_version ~= nil then
		return writeout_proto_picture_dir(name, dir, picspec.hr_version)
	elseif #picspec > 0 then
		return writeout_proto_picture_dir(name, dir, picspec[1])
	end
	
	if picspec.filename ~= nil and picspec.width ~= nil and picspec.height ~= nil then
		local shiftx = picspec.shift ~= nil and (picspec.shift[1]) or 0
		local shifty = picspec.shift ~= nil and (picspec.shift[2]) or 0
		local scale = picspec.scale or 1
		local xx = picspec.x or 0
		local yy = picspec.y or 0

		-- this uses "|", "*" and ":" as separators on purpose, because these
		-- may not be used in windows path names, and are thus unlikely to appear
		-- in the image filenames.
		local result = picspec.filename..":"..picspec.width..":"..picspec.height..":"..shiftx..":"..shifty..":"..xx..":"..yy..":"..scale
		print(">>> "..name.."["..dir.."] -> "..result)
		return result
	else
		print(">>> "..name.."["..dir.."] WTF")
		return nil
	end
end

function writeout_proto_picture(name, picspec)
	local n_dirs = 0
	local dirs = {"north","east","south","west"}
	for _,dir in ipairs(dirs) do
		if picspec[dir] ~= nil then
			n_dirs = n_dirs + 1
		end
	end

	local result = {}
	local subresult = nil
	if n_dirs > 0 then
		for _,dir in ipairs(dirs) do
			if picspec[dir] ~= nil then
				subresult = writeout_proto_picture_dir(name, (n_dirs == 1) and "any" or dir, picspec[dir])
				if subresult == nil then
					return nil
				else
					table.insert(result, subresult)
				end
			end
		end
	elseif picspec.sheet ~= nil then
		print("TODO FIXME: cannot handle sheet for "..name.." yet!")
	else
		subresult = writeout_proto_picture_dir(name, "any", picspec)
		if subresult == nil then
			return nil
		else
			table.insert(result, subresult)
		end
	end
	
	-- this uses "|", "*" and ":" as separators on purpose, because these
	-- may not be used in windows path names, and are thus unlikely to appear
	-- in the image filenames.
	return name.."*"..table.concat(result, "*")
end

function writeout_pictures()
	header = "graphics: "
	
	-- this is dirty. in data-final-fixes.lua, we wrote out "serpent.dump(data.raw)" into the
	-- order strings of the "DATA_RAW"..i entities. We had to use multiple of those, because
	-- there's a limit of 200 characters per string.

	if game.entity_prototypes["DATA_RAW_LEN"] == nil then
		print("ERROR: no DATA_RAW_LEN entity prototype?!")
	end

	local n = tonumber(game.entity_prototypes["DATA_RAW_LEN"].order)
	print("n is " .. n)

	local i = 0
	local string = ""

	local step = math.floor(n/20)
	if step <= 0 then step = 1 end
	print("reading data.raw")
	for i = 1,n do
		if (i % step == 0) then print(string.format("%3.0f%%", 100*i/n)) end
		string = string .. game.entity_prototypes["DATA_RAW"..i].order
	end
	data = {raw = loadstring(string)()}

	local lines = {}
	for group,members in pairs(data.raw) do
		if group ~= "recipe" and group ~= "item" then
			for proto, content in pairs(members) do
				for _,child in ipairs({"structure","animation","picture","animations","base_picture"}) do
					if content[child] ~= nil then
						local result = writeout_proto_picture(proto, content[child])
						if result ~= nil then
							table.insert(lines, result)
						end
						break
					end
				end
			end
		end
	end
	--print(serpent.block(data.raw))
	
	-- this uses "|", "*" and ":" as separators on purpose, because these
	-- may not be used in windows path names, and are thus unlikely to appear
	-- in the image filenames.
	write_file(header..table.concat(lines, "|").."\n")
	print(table.concat(lines,"|"))
end

function writeout_entity_prototypes()
	header = "entity_prototypes: "
	lines = {}
	for name, prot in pairs(game.entity_prototypes) do
		local coll = ""
		local mineable = ""
		if prot.collision_mask ~= nil then
			if prot.collision_mask['player-layer'] then coll=coll.."P" else coll=coll.."p" end
			if prot.collision_mask['object-layer'] then coll=coll.."O" else coll=coll.."o" end
		end
		if prot.mineable_properties.mineable then mineable = "1" else mineable = "0" end
		table.insert(lines, prot.name.." "..coll.." "..aabb_str(prot.collision_box).." "..mineable)
	end
	write_file(header..table.concat(lines, "$").."\n")
end

function writeout_item_prototypes()
	header = "item_prototypes: "
	lines = {}
	for name, prot in pairs(game.item_prototypes) do
		table.insert(lines, prot.name.." "..prot.type.." "..(prot.place_result and prot.place_result.name or "nil").." "..prot.stack_size.." "..(prot.fuel_value or 0).." "..(prot.speed or 0).." "..(prot.durability or 0))
	end

	-- FIXME this is a hack
	for name, prot in pairs(game.fluid_prototypes) do
		table.insert(lines, prot.name.." FLUID nil 0 0 0 0")
	end

	write_file(header..table.concat(lines, "$").."\n")
end

function writeout_recipes()
	-- FIXME: this assumes that there is only one player force
	header = "recipes: "
	lines = {}
	for name, rec in pairs(game.forces["player"].recipes) do
		ingredients = {}
		products = {}
		for _,ing in ipairs(rec.ingredients) do
			table.insert(ingredients, ing.name.."*"..ing.amount)
		end

		for _,prod in ipairs(rec.products) do
			if prod.amount ~= nil then
				amount = prod.amount
			else
				amount = (prod.amount_min + prod.amount_max) / 2 * prod.probability
			end
			table.insert(products, prod.name.."*"..amount)
		end

		table.insert(lines, rec.name.." "..(rec.enabled and "1" or "0").." "..rec.energy.." "..table.concat(ingredients,",").." "..table.concat(products,","))
	end
	write_file(header..table.concat(lines, "$").."\n")
end

function on_whoami()
	if client_local_data.whoami == "server" then
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
		if global.p[idx] and player.connected and player.character then -- TODO FIXME
			if global.p[idx].walking then
				local w = global.p[idx].walking
				local pos = player.character.position
				local dest = w.waypoints[w.idx]

				local dx = dest.x - pos.x
				local dy = dest.y - pos.y

				if (math.abs(dx) < 0.3 and math.abs(dy) < 0.3) then
					w.idx = w.idx + 1
					if w.idx > #w.waypoints then
						player.walking_state = {walking=false}
						action_completed(w.action_id)
						global.p[idx].walking = nil
						dx = 0
						dy = 0
					else
						dest = w.waypoints[w.idx]
						dx = dest.x - pos.x
						dy = dest.y - pos.y
					end
				end

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
				end
			end

			if global.p[idx].mining then
				local ent = global.p[idx].mining.entity

				if ent and ent.valid then -- mining complete
					if distance(player.position, ent.position) > 6 then
						player.print("warning: too far too mine oO?")
					end

					-- unfortunately, factorio doesn't offer a "select this entity" function
					-- we need to select stuff depending on the cursor position, which *might*
					-- select something different instead. (e.g., a tree or the player in the way)
					player.update_selected_entity(ent.position)
					local ent2 = player.selected

					if (ent2 == nil) then
						print("wtf, not mining any target")
					elseif (ent.name ~= ent2.name or ent.position.x ~= ent2.position.x or ent.position.y ~= ent2.position.y) then
						if ent2.type == "tree" then
							print("mining: there's a tree in our way. deforesting...")
							player.mining_state = { mining=true, position=ent.position }
						else
							print("wtf, not mining the expected target (expected: "..ent.name..", found: "..ent2.name..")")
						end
					else
						player.mining_state = { mining=true, position=ent.position }
					end
				else
					--write_file("complete: mining "..idx.."\n")
					action_completed(global.p[idx].mining.action_id)
					global.p[idx].mining = nil
				end
			end
		end
	end

	if event.tick % 120 == 0 then
		local who = "?"
		if client_local_data.whoami then who = client_local_data.whoami end
		if my_client_id ~= nil then print(my_client_id.."."..who) end
	end

	if event.tick % 10 == 0 then
		writeout_players()
	end

	-- periodically update the objects around the player to ensure that nothing is missed
	-- This is merely a safety net and SHOULD be unnecessary, if all other updates don't miss anything
	if event.tick % 300 == 0 and false then -- don't do that for now, as it eats up too much cpu on the c++ part
		for idx, player in pairs(game.players) do
			if player.connected and player.character then
				local x = math.floor(player.character.position.x/32)*32
				local y = math.floor(player.character.position.x/32)*32

				for xx = x-96, x+96, 32 do
					for yy = y-96, y+96, 32 do
						writeout_objects(player.surface, {left_top={x=xx,y=yy}, right_bottom={x=xx+32,y=yy+32}})
					end
				end
			end
		end
	end

	if #todo_next_tick > 0 then
		print("on_tick executing "..#todo_next_tick.." stored callbacks")
		for _,func in ipairs(todo_next_tick) do
			func()
		end
		todo_next_tick = {}
	end
end

function writeout_players()
	local players={}
	for idx, player in pairs(game.players) do
		if player.connected and player.character then
			table.insert(players, idx.." "..player.character.position.x.." "..player.character.position.y)
		end
	end
	write_file("players: "..table.concat(players, ",").."\n")
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

	write_file(header..table.concat(line, ",").."\n")
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
	write_file(header..table.concat(lines,"").."\n")

	line=nil
end

function writeout_objects(surface, area)
	--if my_client_id ~= 1 then return end
	header = "objects "..area.left_top.x..","..area.left_top.y..";"..area.right_bottom.x..","..area.right_bottom.y..": "
	line = ''
	lines={}
	for idx, ent in pairs(surface.find_entities(area)) do
		if area.left_top.x <= ent.position.x and ent.position.x < area.right_bottom.x and area.left_top.y <= ent.position.y and ent.position.y < area.right_bottom.y then
			if ent.prototype.collision_mask ~= nil and ent.prototype.collision_mask['player-layer'] then
				line=line..","..ent.name.." "..ent.position.x.." "..ent.position.y
				if idx % 100 == 0 then
					table.insert(lines,line)
					line=''
				end
			end
		end
	end
	table.insert(lines,line)
	write_file(header..table.concat(lines,"").."\n")

	line=nil
end

function rangestr(area)
	return coord({x=area.x1, y=area.y1}).." -- "..coord({x=area.x2, y=area.y2})
end

function coord(pos)
	return "("..pos.x.."/"..pos.y..")"
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

function on_player_mined_item(event)
	name = event.item_stack.name
	count = event.item_stack.count
	if count == nil then
		print("count was nil")
		count = 1
	end

	write_file("mined_item: "..event.player_index.." "..name.." "..count.."\n")
end


function action_completed(action_id)
	write_file("action_completed: ok "..action_id.."\n")
end

function action_failed(action_id)
	write_file("action_completed: fail "..action_id.."\n")
end

function on_some_entity_created(event)
	local ent = event.entity or event.created_entity or nil
	if ent == nil then
		complain("wtf, on_some_entity_created has nil entity")
		return
	end

	writeout_objects(ent.surface, {left_top={x=math.floor(ent.position.x), y=math.floor(ent.position.y)}, right_bottom={x=math.floor(ent.position.x)+1, y=math.floor(ent.position.y)+1}})

	complain("on_some_entity_created: "..ent.name.." at "..ent.position.x..","..ent.position.y)
end

function on_some_entity_deleted(event)
	local ent = event.entity
	if ent == nil then
		complain("wtf, on_some_entity_created has nil entity")
		return
	end

	-- we can't do this now, because the entity still exists at this point. instead, we schedule the writeout for the next tick
	
	local surface = ent.surface
	local area = {left_top={x=math.floor(ent.position.x), y=math.floor(ent.position.y)}, right_bottom={x=math.floor(ent.position.x)+1, y=math.floor(ent.position.y)+1}}

	table.insert(todo_next_tick, function () writeout_objects(surface, area ) end)
	
	complain("on_some_entity_deleted: "..ent.name.." at "..ent.position.x..","..ent.position.y)
end

function on_player_crafted_item(event)
	queue = crafting_queue[event.player_index]
	if queue == nil then
		complain("player "..game.players[event.player_index].name.." unexpectedly crafted "..event.recipe.name)
	else
		if queue[1].recipe == event.recipe.name then
			if queue[1].id == nil then
				complain("player "..game.players[event.player_index].name.." has crafted "..queue[1].recipe..", but that's not all")
			else
				complain("player "..game.players[event.player_index].name.." has finished crafting "..queue[1].recipe.." with id "..queue[1].id)
				action_completed(queue[1].id)
			end
			table.remove(queue,1)
			if #queue == 0 then
				complain("done crafting")
				crafting_queue[event.player_index] = nil
			end
		else
			complain("player "..game.players[event.player_index].name.." crafted "..event.recipe.name.." which is probably an intermediate product")
		end
	end
end

script.on_init(on_init)
script.on_load(on_load)
script.on_event(defines.events.on_tick, on_tick)
script.on_event(defines.events.on_player_joined_game, on_player_joined_game)
script.on_event(defines.events.on_sector_scanned, on_sector_scanned)
script.on_event(defines.events.on_chunk_generated, on_chunk_generated)
script.on_event(defines.events.on_player_mined_item, on_player_mined_item)

script.on_event(defines.events.on_biter_base_built, on_some_entity_created) --entity
script.on_event(defines.events.on_built_entity, on_some_entity_created) --created_entity
script.on_event(defines.events.on_robot_built_entity, on_some_entity_created) --created_entity

script.on_event(defines.events.on_entity_died, on_some_entity_deleted) --entity
script.on_event(defines.events.on_player_mined_entity, on_some_entity_deleted) --entity
script.on_event(defines.events.on_robot_mined_entity, on_some_entity_deleted) --entity
script.on_event(defines.events.on_resource_depleted, on_some_entity_deleted) --entity

script.on_event(defines.events.on_player_crafted_item, on_player_crafted_item)



function rcon_test(foo)
	print("rcon_test("..foo..")")
end

function rcon_set_waypoints(action_id, player_id, waypoints) -- e.g. waypoints= { {0,0}, {3,3}, {42,1337} }
	tmp = {}
	for i = 1, #waypoints do
		tmp[i] = {x=waypoints[i][1], y=waypoints[i][2]}
	end
	global.p[player_id].walking = {idx=1, waypoints=tmp, action_id=action_id}
end

function rcon_set_mining_target(action_id, player_id, name, position)
	local player = game.players[player_id]
	local ent = nil

	if name ~= nil and position ~= nil then
		ent = player.surface.find_entity(name, position)
	end

	if ent and ent.minable then
		global.p[player_id].mining = { entity = ent, action_id = action_id}
	elseif name == "stop" then
		global.p[player_id].mining = nil
	else
		print("no entity to mine")
		global.p[player_id].mining = nil
		action_failed(action_id)
	end
end

function rcon_place_entity(player_id, item_name, entity_position, direction)
	local entproto = game.item_prototypes[item_name].place_result
	local player = game.players[player_id]
	local surface = game.players[player_id].surface

	if entproto == nil then
		complain("cannot place item '"..item_name.."' because place_result is nil")
		return
	end

	if player.get_item_count(item_name) <= 0 then
		complain("cannot place item '"..item_name.."' because the player '"..player.name.."' does not have any")
		return
	end
	
	if not surface.can_place_entity{name=entproto.name, position=entity_position, direction=direction, force=player.force} then
		complain("cannot place item '"..item_name.."' because surface.can_place_entity said 'no'")
		return
	end

	player.remove_item({name=item_name,count=1})
	result = surface.create_entity{name=entproto.name,position=entity_position,direction=direction,force=player.force, fast_replace=true, player=player, spill=true}

	if result == nil then
		complain("placing item '"..item_name.."' failed, surface.create_entity returned nil :(")
	end
end

function rcon_insert_to_inventory(player_id, entity_name, entity_pos, inventory_type, items)
	local player = game.players[player_id]
	local entity = player.surface.find_entity(entity_name, entity_pos)
	if entity == nil then
		complain("cannot insert to inventory of nonexisting entity "..entity_name.." at "..pos_str(entity_pos))
		return
	end

	local inventory = entity.get_inventory(inventory_type)
	if inventory == nil then
		complain("cannot insert to nonexisting inventory of entity "..entity_name.." at "..pos_str(entity_pos))
		return
	end

	local count = 1
	if items.count ~= nil then count=items.count end
	local real_n = inventory.insert(items)

	if count ~= real_n then
		complain("tried to insert "..count.." "..items.name.." but inserted " .. real_n)
	end
end
function rcon_remove_from_inventory(player_id, entity_name, entity_pos, inventory_type, items)
	local player = game.players[player_id]
	local entity = player.surface.find_entity(entity_name, entity_pos)
	if entity == nil then
		complain("cannot remove from inventory of nonexisting entity "..entity_name.." at "..pos_str(entity_pos))
		return
	end

	local inventory = entity.get_inventory(inventory_type)
	if inventory == nil then
		complain("cannot remove from nonexisting inventory of entity "..entity_name.." at "..pos_str(entity_pos))
		return
	end

	local count = 1
	if items.count ~= nil then count=items.count end
	local real_n = inventory.remove(items)

	if count ~= real_n then
		complain("tried to remove "..count.." "..items.name.." but removed " .. real_n)
	end
end

function rcon_whoami(who)
	if client_local_data.whoami == nil then
		client_local_data.whoami = who
		on_whoami()
	end
end

function rcon_start_crafting(action_id, player_id, recipe, count)
	local player = game.players[player_id]
	local ret = player.begin_crafting{count=count, recipe=recipe}
	if ret ~= count then
		complain("could not have player "..player.name.." craft "..count.." "..recipe.." (but only "..ret..")")
	end

	for i = 1,count do
		local aid = nil
		if i == count then aid = action_id end
		if crafting_queue[player_id] == nil then crafting_queue[player_id] = {} end
		table.insert(crafting_queue[player_id], {recipe=recipe, id=aid})
	end
end

remote.add_interface("windfisch", {
	test=rcon_test,
	set_waypoints=rcon_set_waypoints,
	set_mining_target=rcon_set_mining_target,
	place_entity=rcon_place_entity,
	start_crafting=rcon_start_crafting,
	insert_to_inventory=rcon_insert_to_inventory,
	remove_from_inventory=rcon_remove_from_inventory,
	whoami=rcon_whoami
})
