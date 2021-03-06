/* 
Copyright (c) 2016 William Bundy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * rituals_world.cpp
 */

isize _tw, _th;
uint8* _tiles;
uint8 _get_at(isize x, isize y)
{
	if((x < 0) || (x > _tw) || (y < 0) || (y > _th)) return false;
	isize index = y * _tw + x;
	if((index < 0) || (index >= _tw * _th)) return false;
	return _tiles[index];

}

void generate_statics_for_tilemap(Simulator* sim, Tilemap* tilemap)
{
	start_temp_arena(game->temp_arena);
	_tw = tilemap->w;
	_th = tilemap->h;
	isize map_size = tilemap->w * tilemap->h;
	uint8* tiles = Arena_Push_Array(game->temp_arena, uint8, map_size + 1);
	for(isize i = 0; i < map_size; ++i) {
		tiles[i] = tilemap->info[tilemap->tiles[i]].solid;
	}
	_tiles = tiles;
	isize work = 0;

	Rect2i* rects = Arena_Push_Array(game->temp_arena, Rect2i, map_size / 2);
	isize rects_count = 0;
	isize last_rects = 0;
	do {
		last_rects = rects_count;
		for(isize y = 0; y < tilemap->h; ++y) {
			for(isize x = 0; x < tilemap->w; ++x) {
				if(_get_at(x, y)) {
					if(!_get_at(x, y - 1)) {
						Rect2i* r = rects + rects_count++;
						r->x = x;
						r->y = y;
						r->w = 1;
						r->h = 1;
						do {
							x++;
						}
						while(_get_at(x, y) && !_get_at(x, y - 1) && (x < tilemap->w));

						if(x != r->x) {
							r->w = x - r->x;
						}
					}	
				}
			}
		}

		for(isize i = last_rects; i < rects_count; ++i) {
			Rect2i* r = rects + i;
			bool solid = true;
			isize y = r->y;
			while(solid && (y < tilemap->h)) {
				for(isize local_x = 0; local_x < r->w; ++local_x) {
					solid = solid && _get_at(r->x + local_x, y + 1);
					if(!solid) break;
				}
				if(solid) {
					y++;
					r->h++;
				}
			}
		}

		for(isize i = 0; i < rects_count; ++i) {
			Rect2i* r = rects + i;
			for(isize local_y = 0; local_y < r->h; ++local_y) {
				for(isize local_x = 0; local_x < r->w; ++local_x) {
					isize index = (local_y + r->y) * tilemap->w + (local_x + r->x);
					//printf("%d ", index);
					tiles[index] = false;
				}
			}
		}
		work = 0;
		for(isize i = 0; i < map_size; ++i) {
			work += tiles[i];
		}
	} while(work);
	
	for(isize i = 0; i < rects_count; ++i) {
		Rect2i* r = rects + i;
		Sim_Body* e = sim_get_next_body(sim);
		e->shape.center.x = (r->x + r->w / 2.0f) * Tile_Size;//+ Half_TS;
		e->shape.center.y = (r->y + r->h / 2.0f) * Tile_Size;// + Half_TS;
		e->shape.hw = r->w * Half_TS;
		e->shape.hh = r->h * Half_TS;
		e->restitution = 0.3f;
		e->inv_mass = 0.0f;
		e->flags = Body_Flag_Static;
	}
	end_temp_arena(game->temp_arena);
}

typedef struct World_Area World_Area;

enum Direction 
{
	Direction_North,
	Direction_South,
	Direction_East,
	Direction_West
};


#define Entity_Max_Callbacks_Per_Event (16)
#define Entity_On_Activate_Macro(name) void name(Entity* entity, World_Area* area)
typedef Entity_On_Activate_Macro((*Entity_On_Activate));

/*
enum Entity_Event_Type
{
	Entity_Event_On_Activate
}
struct Entity_Event 
{
	union {
		Entity_On_Activate on_activate;

	} event;
	isize hash;
	const char* name;
	real priority;
};
*/

struct Entity
{
	isize id;
	isize body_id;
	Sim_Body* body;
	Sprite sprite;

	int32 counter;

	int32 facing;
	Direction direction;
	
	World_Area* area;
	
	//TODO(will) Implement entity callback hashtable
	// Right now save and load with function pointers will break
	// Can't verify whether pointers will be the same between dll loads
	Entity_On_Activate* event_on_activate;
	isize event_on_activate_count;

	//userdata
	isize held_entity_id;
};

#define _entity_get_id(e) (e.id)
Generate_Quicksort_For_Type(entity_sort_on_id, Entity, _entity_get_id)
Generate_Binary_Search_For_Type(entity_search_for_id, Entity, isize, _entity_get_id)


typedef struct World_Area World_Area;
struct Area_Link
{
	Vec2i position;
	World_Area* link;
};

#define World_Area_Entity_Capacity (8192)
#define World_Area_Tilemap_Width (64)
#define World_Area_Tilemap_Height (64)
struct World_Area
{
	Simulator sim;
	Tilemap map;
	Vec2 offset;

	Entity* player;
	Sim_Body* player_body;
	Entity* entities;
	bool entities_dirty;
	isize entities_count, entities_capacity, next_entity_id;

	Area_Link north;
	Area_Link south;
	Area_Link west;
	Area_Link east;
};


void init_world_area(World_Area* area, Memory_Arena* arena)
{
	init_simulator(&area->sim, World_Area_Entity_Capacity, arena);
	init_tilemap(&area->map, 
			World_Area_Tilemap_Width,
			World_Area_Tilemap_Height,
			arena);

	area->entities = Arena_Push_Array(arena, Entity, World_Area_Entity_Capacity);
	area->entities_count = 0;
	area->entities_capacity = World_Area_Entity_Capacity;
	area->next_entity_id = 0;
	area->entities_dirty = false;

}

void init_entity(Entity* entity)
{
	entity->id = 0;
	entity->body_id = 0;
	init_sprite(&entity->sprite);
	entity->counter = 0;
	entity->area = NULL;
	entity->event_on_activate = NULL;
	entity->event_on_activate_count = 0;
}

isize entity_add_event_on_activate(Entity* e, Entity_On_Activate event)
{
	if(e != NULL) {
		if(e->event_on_activate == NULL) {
			e->event_on_activate = Arena_Push_Array(
					game->play_arena, 
					Entity_On_Activate,
					Entity_Max_Callbacks_Per_Event);
		}
		if(e->event_on_activate_count < Entity_Max_Callbacks_Per_Event) {
			e->event_on_activate[e->event_on_activate_count++] = event;
		} else {
			Log_Error("Entity exceeded on_activate_count callbacks");
		}

	}
	return e->event_on_activate_count - 1;
}



Entity* world_area_get_next_entity(World_Area* area)
{
	if(area->entities_count + 1 >= area->entities_capacity) {
		Log_Error("Ran out of entities");
		return NULL;
	}

	Entity* e = area->entities + area->entities_count++;
	init_entity(e);
	e->area = area;
	e->body = sim_get_next_body(&area->sim);
	e->body_id = e->body->id;
	e->id = area->next_entity_id++;
	e->body->entity = e;
	e->body->entity_id = e->id;
	return e;
}

Entity* world_area_find_entity(World_Area* area, isize id)
{
	if(area->entities_dirty) {
		entity_sort_on_id(area->entities, area->entities_count);
	}
	isize index = entity_search_for_id(id, area->entities, area->entities_count);
	return index == -1 ? NULL : area->entities + index;
}

void world_area_init_player(World_Area* area, Vec2i tile_pos)
{
	Entity* player_entity = world_area_find_entity(area, 0);
	Sim_Body* player = sim_find_body(&area->sim, player_entity->body_id);
	if(player == NULL) {
		printf("Something went wrong! Couldn't find player entity....?");
	}
	///player->shape.center = v2(area->map.w * 16, area->map.h * 16);
	player->shape.center = v2(tile_pos.x * Tile_Size, tile_pos.y * Tile_Size);
	player_entity->sprite.texture = Get_Texture_Coordinates(0, 0, 32, 32);
	player->shape.hext = v2(5, 5);
	player_entity->sprite.size = v2(32, 32);
	player_entity->sprite.center = v2(0,11);
	player->damping = 0.5f;
	player->restitution = 0;
	player->flags = Body_Flag_No_Friction;
	area->offset = player->shape.center;	
}

void world_area_deinit_player(World_Area* area)
{
	Entity* player_entity = world_area_find_entity(area, 0);
	Sim_Body* player = sim_find_body(&area->sim, player_entity->body_id);
	player->shape.center = v2(-1000, -1000);
}

struct World
{
	World_Area* areas;
	isize areas_count, areas_capacity;
	isize areas_width, areas_height;

	World_Area* current_area;
};

void world_switch_current_area(World* world, Area_Link link)
{
	if(link.link == NULL) return;
	World_Area* new_area = link.link;
	world_area_init_player(new_area, link.position);
	world_area_deinit_player(world->current_area);
	world->current_area = new_area;
}

void init_world(World* world, isize width, isize height, Memory_Arena* arena)
{
	world->areas_capacity = width * height * 2;
	world->areas = Arena_Push_Array(arena, World_Area, world->areas_capacity);
	world->areas_count = 0;
	world->areas_width = width;
	world->areas_height = height;
}


Entity_On_Activate_Macro(test_on_activate)
{
	printf("%d was clicked \n", entity->id);
}
void generate_world(World* world, Tile_Info* info, isize ti_count, uint64 seed, Memory_Arena* arena)
{
	Random r_s;
	Random* r = &r_s;
	init_random(r, seed);


	for(isize i = 0; i < world->areas_height; ++i) {
		for(isize j = 0; j < world->areas_width; ++j) {
			isize index = i * world->areas_width + j;
			World_Area* area = world->areas + index;
			init_world_area(area, arena);
			area->map.info = info;
			area->map.info_count = ti_count;
			generate_tilemap(&area->map, next_random_uint64(r));
			for(isize i = 0; i < World_Area_Tilemap_Width; ++i) {
				Entity* e = world_area_get_next_entity(area);
				Sim_Body* b = sim_find_body(&area->sim, e->body_id);
				e->sprite.texture = Get_Texture_Coordinates(0, 96, 32, 64);
				b->shape.hw = 16;
				b->shape.hh = 12;
				b->inv_mass = 1.0f;
				e->sprite.size = v2(32, 64);
				e->sprite.center = v2(0, 20);
				entity_add_event_on_activate(e, test_on_activate);
				do {
					b->shape.center = v2(
						rand_range(r, 0, area->map.w * 32),
						rand_range(r, 0, area->map.h * 32));
				}
				while (info[tilemap_get_at(&area->map, b->shape.center)].solid);
			}

			generate_statics_for_tilemap(&area->sim, &area->map);

			isize north_link = modulus(i - 1, world->areas_height) * world->areas_width + j;
			isize south_link = modulus(i + 1, world->areas_height) * world->areas_width + j;
			isize west_link = i * world->areas_width + modulus(j - 1, world->areas_width);
			isize east_link = i * world->areas_width + modulus(j + 1, world->areas_width);
			area->north = Area_Link {
				v2i(World_Area_Tilemap_Width / 2,  World_Area_Tilemap_Height - 1), 
				world->areas + north_link
			};
			area->south = Area_Link {
				v2i(World_Area_Tilemap_Width / 2, 1),
				world->areas + south_link
			};
			area->west = Area_Link {
				v2i(World_Area_Tilemap_Width - 1, World_Area_Tilemap_Height / 2),
				world->areas + west_link
			};
			area->east = Area_Link {
				v2i(1, World_Area_Tilemap_Height / 2),
				world->areas + east_link
			};
		}
	}

}

void world_area_sort_entities_on_id(World_Area* area)
{
	entity_sort_on_id(area->entities, area->entities_count);
}

void world_area_synchronize_entities_and_bodies(World_Area* area)
{
	for(isize i = 0; i < area->entities_count; ++i) {
		Entity* e = area->entities + i;
		if(e->body_id == -1) continue;
		Sim_Body* b = sim_find_body(&area->sim, e->body_id);
		b->entity = e;
		b->entity_id = e->id;
		e->body = b;
	}
}

void world_area_remove_entity(World_Area* area, Entity* entity)
{
	sim_remove_body(&area->sim, entity->body_id);
	isize index = entity_search_for_id(entity->id, area->entities, area->entities_count);
	area->entities[index] = area->entities[--area->entities_count];
	world_area_sort_entities_on_id(area);
	world_area_synchronize_entities_and_bodies(area);
}

Entity_On_Activate_Macro(delete_on_activate)
{
	world_area_remove_entity(area, entity);
}


#define _check(s1, s2, state) ((input->scancodes[SDL_SCANCODE_##s1] == state) || (input->scancodes[SDL_SCANCODE_##s2] == state))
void update_world_area(World_Area* area)
{
	game_set_scale(2.0);
	real movespeed = 800;
	Vec2 move_impulse = v2(0, 0);

	if(_check(LEFT, A, State_Pressed)) {
		move_impulse.x -= movespeed;
	}
	if(_check(RIGHT, D, State_Pressed)) {
		move_impulse.x += movespeed;
	}
	if(_check(UP, W, State_Pressed)) {
		move_impulse.y -= movespeed;
	}
	if(_check(DOWN, S, State_Pressed)) {
		move_impulse.y += movespeed;
	}

	if(fabsf(move_impulse.x * move_impulse.y) > 0.01f) {
		move_impulse *= Math_InvSqrt2;
	}

	play_state->current_time = SDL_GetTicks();
	real dt = (play_state->current_time - play_state->prev_time) / 1000.0;
	dt = clamp(dt, 0, 1.2f);
	play_state->accumulator += dt;
	play_state->prev_time = play_state->current_time;

	sim_sort_bodies_on_id(&area->sim);
	Entity* player_entity = world_area_find_entity(area, 0);
	Sim_Body* player = player_entity->body;

	Tile_Info* player_tile = area->map.info + tilemap_get_at(&area->map, player->shape.center);

	move_impulse *= player_tile->movement_modifier;


	while(play_state->accumulator >= Time_Step) {
		play_state->accumulator -= Time_Step;
		player->velocity += move_impulse;
		sim_update(&area->sim, &area->map, Time_Step);
	}

	Direction old_direction = player_entity->direction;
	if(move_impulse.y < 0) {
		player_entity->direction = Direction_North;
	} else if(move_impulse.y > 0) {
		player_entity->direction = Direction_South;
	}

	if(move_impulse.x < 0) {
		player_entity->facing = -1;
		player_entity->direction = Direction_West;
	} else if(move_impulse.x > 0) {
		player_entity->facing = 1;
		player_entity->direction = Direction_East;
	}
	if(input->scancodes[SDL_SCANCODE_SPACE] == State_Pressed) {
		player_entity->direction = old_direction;
	}
	Sprite* plr_spr = &player_entity->sprite;
	int32 player_frame = 0;
	if(v2_dot(move_impulse, move_impulse) > 0){
		player_entity->counter++;
		player_frame = 1;
		if(player_entity->counter > 15) {
			player_frame = 0;
			if(player_entity->counter > 30) {
				player_entity->counter = 0;
			}
		}
	} else {
		player_entity->counter = 0;
		player_frame = 0;
	}

	if(player_entity->facing == -1) {
		plr_spr->texture = Get_Texture_Coordinates(32 + player_frame * 32, 0, -32, 32);
	} else if(player_entity->facing == 1) {
		plr_spr->texture = Get_Texture_Coordinates(0  + player_frame * 32, 0, 32, 32);
	}

	Vec2 target = player->shape.center;

	if(target.x < 0) {
		world_switch_current_area(play_state->world, area->west);
		play_state->world_xy.x--;
	} else if(target.x > area->map.w * Tile_Size) {
		world_switch_current_area(play_state->world, area->east);
		play_state->world_xy.x++;
	} else if(target.y < 0) {
		world_switch_current_area(play_state->world, area->north);
		play_state->world_xy.y--;
	} else if(target.y > area->map.h * Tile_Size) {
		world_switch_current_area(play_state->world, area->south);
		play_state->world_xy.y++;
	}

	area->offset += (target - area->offset) * 0.1f;
	area->offset -= game->size * 0.5f;
	if(area->offset.x < 0) 
		area->offset.x = 0;
	else if((area->offset.x + game->size.x) > area->map.w * Tile_Size)
		area->offset.x = area->map.w * Tile_Size - game->size.x;

	if(area->offset.y < 0) 
		area->offset.y = 0;
	else if((area->offset.y + game->size.y) > area->map.h * Tile_Size)
		area->offset.y = area->map.h * Tile_Size - game->size.y;

	//TODO(will) refactor into own function?
	/*
	 * Player input code
	 *
	 */ 
	if(input->mouse[SDL_BUTTON_LEFT] == State_Just_Pressed) {
		Entity* ball_entity = world_area_get_next_entity(area);
		Sim_Body* ball = ball_entity->body;
		Vec2 dmouse = v2(input->mouse_x / game->scale, 
						input->mouse_y / game->scale) + area->offset;

		dmouse -= player->shape.center;
		real angle = atan2f(dmouse.y, dmouse.x);
		Vec2 normal = v2(cosf(angle), sinf(angle));
		entity_add_event_on_activate(ball_entity, delete_on_activate);

		ball->damping = 0.9999f;
		ball->shape.hext = v2(8, 16);
		ball->shape.center = normal * ball->shape.hw * 4 + player->shape.center; 
		ball->velocity += normal * 2000;
		ball->shape.hext = v2(8, 6);
		//ball->flags = Body_Flag_No_Friction;
		ball_entity->sprite.size = v2(16, 32);
		ball_entity->sprite.center = v2(0, 10);
		ball_entity->sprite.texture  = Get_Texture_Coordinates(0, 96, 32, 64);
	}


	if(input->mouse[SDL_BUTTON_RIGHT] == State_Just_Pressed) {
		Vec2 dmouse = v2(
			input->mouse_x / game->scale, 
			input->mouse_y / game->scale) + area->offset;
		AABB mbb = aabb(dmouse, 0, 0);
		world_area_synchronize_entities_and_bodies(area);
		for(isize i = 0; i < area->entities_count; ++i) {
			Entity* e = area->entities + i;
			if(aabb_intersect(e->body->shape, mbb)) {
				if(e->id != 0) {
					for(isize j = 0; j < e->event_on_activate_count; ++j) {
						e->event_on_activate[j](e, area);
					}
					break;
				}
			}
		}
	}

	if(input->scancodes[SDL_SCANCODE_F] == State_Just_Pressed) {
		//tilemap_set_at(&area->map, player->shape.center, Tile_Dug_Earth);
		Tile_State* state = tilemap_get_state_at(&area->map, player->shape.center);
		if(state != NULL) {
			state->damage++;
			update_tile_state_at(&area->map, player->shape.center);
		}
	}

	Sprite s;

	if(input->scancodes[SDL_SCANCODE_SPACE] >= State_Pressed) {
		init_sprite(&s);
		s.position = player->shape.center;
		s.size = v2(16, 16);
		s.texture = Get_Texture_Coordinates(0, renderer->texture_height - 16, 16, 16);
		s.color = v4(1, 1, 1, 1);
		switch(player_entity->direction) {
			case Direction_North:
				s.position.y -= s.size.y + player->shape.hh;
				break;
			case Direction_South:
				s.position.y += s.size.y + player->shape.hh;
				break;
			case Direction_East:
				s.position.x += s.size.x + player->shape.hw;
				break;
			case Direction_West:
				s.position.x -= s.size.x + player->shape.hh;
				break;
		}

		if(input->scancodes[SDL_SCANCODE_SPACE] == State_Just_Pressed) {
			//TODO(will) implement good space queries	
			Sim_Body* touching = sim_query_aabb(&area->sim, 
					aabb(s.position, s.size.x / 2, s.size.y / 2));
			if(touching != NULL) {
				if(!Has_Flag(touching->flags, Body_Flag_Static)) 
					player_entity->held_entity_id = touching->entity_id;
			}
		}
	} else {
		player_entity->held_entity_id = -1;
	}

	char debug_str[256];
	if(player_entity->held_entity_id > 0) {
		Entity* e = world_area_find_entity(area, player_entity->held_entity_id);
		if(e != NULL) {
			Sim_Body* b = e->body;
			Vec2 target = player->shape.center; 
			Vec2 diff = b->shape.hext + player->shape.hext + v2(8, 8);
			switch(player_entity->direction) {
				case Direction_North:
					target.y -= diff.y;
					break;
				case Direction_South:
					target.y += diff.y;
					break;
				case Direction_East:
					target.x += diff.x;
					break;
				case Direction_West:
					target.x -= diff.x;
					break;
			}
			
			Vec2 impulse = (target - b->shape.center);
			if(v2_dot(impulse, impulse) > (4 * Tile_Size * Tile_Size)) {
				player_entity->held_entity_id = -1;
			}
			impulse *= 60;
			{
				snprintf(debug_str, 256, "T(%.2f %.2f) j(%.2f %.2f)", target.x, target.y,
						impulse.x, impulse.y);

			}
			if(v2_dot(impulse, impulse) < (1000 * 1000)) 
				b->velocity += impulse;// * b->inv_mass;

		}

	}

	renderer->offset = area->offset;
	area->offset += game->size * 0.5f;
	// throw a ball

	renderer_start();

	Rect2 screen = rect2(
			area->offset.x - game->size.x / 2,
			area->offset.y - game->size.y / 2, 
			game->size.x, game->size.y);

	isize sprite_count_offset = render_tilemap(&area->map, v2(0,0), screen);

	for(isize i = 0; i < area->entities_count; ++i) {
		Entity* e = area->entities + i;
		Sim_Body* b = sim_find_body(&area->sim, e->body_id);

		if (b == NULL) continue;
		e->sprite.position = b->shape.center;
		//e->sprite.size = v2(b->shape.hw * 2, b->shape.hh * 2);
		
		//TODO(will) align entity sprites by their bottom center
		renderer_push_sprite(&e->sprite);
	}

	renderer_push_sprite(&s);
	renderer_sort(sprite_count_offset);

	renderer_draw();

	renderer->offset = v2(0, 0);
	game_set_scale(1.0);
	renderer_start();

	char str[256];
	isize len = snprintf(str, 256, "X:%d Y:%d Tile: %s",
			play_state->world_xy.x, 
			play_state->world_xy.y, 
			player_tile->name);

	render_body_text(str, v2(16, game->size.y - (body_font->glyph_height) - 8), true);
	render_body_text(debug_str, v2(16, 16), true);

	renderer_draw();
}




