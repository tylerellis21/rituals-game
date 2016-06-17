/*
 *
 *
 */


struct Sprite
{
	Vec2 position;
	Vec2 center;
	real angle;
	Vec2 scale;
	Rect2 texture;
	Vec4 color;
	uint32 id;
};

struct Renderer
{
	GLuint shader_program, vbo, vao, texture ;
	isize translation_loc, rotation_loc, color_loc, screen_loc, scale_loc, center_loc;
	Vec4 ortho;

	real* sprite_data;
	isize data_index, sprite_count;


	isize last_sprite_id; 
	
};


void sprite_init(Sprite* s, Renderer* renderer)
{
}


#define _gl_offset(a) ((GLvoid*)(a*sizeof(real)))
void renderer_init(Renderer* renderer, Memory_Arena* arena)
{
	renderer->last_sprite_id = 0;
	renderer->sprite_data = Arena_Push_Array(arena, real, Megabytes(1) / sizeof(real)); 
	glGenVertexArrays(1, &renderer->vao);
 
	glGenBuffers(1, &renderer->vbo);

	usize stride = sizeof(real) * 15;
	//position
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, _gl_offset(0));
	glEnableVertexAttribArray(0);  
	glVertexAttribDivisor(0, 4);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, _gl_offset(2));
	glEnableVertexAttribArray(1);
	glVertexAttribDivisor(1, 4);

	//angle
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, _gl_offset(4));
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 4);

	//scale
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, _gl_offset(5));
	glEnableVertexAttribArray(3);
	glVertexAttribDivisor(3, 4);

	//texcoords
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, _gl_offset(7));
	glEnableVertexAttribArray(4);
	glVertexAttribDivisor(4, 4);

	//color
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, _gl_offset(11));
	glEnableVertexAttribArray(5);
	glVertexAttribDivisor(5, 4);



	glBindVertexArray(0);

	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	const char* vertex_shader_src = 
#include "vert.glsl"
		;
	glShaderSource(vertex_shader, 1, &vertex_shader_src, NULL);
	glCompileShader(vertex_shader);

	{
		GLint success;
		glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
		GLsizei log_size;
		char shader_log[4096];
		glGetShaderInfoLog(vertex_shader, 4096, &log_size, shader_log); 

		if(!success) {
			printf("Error compiling vertex shader \n%s \n", shader_log);
		} else {
			printf("Vertex shader compiled successfully\n");
		}
	}	


	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	const char* fragment_shader_src = 
#include "frag.glsl"
		;
	glShaderSource(fragment_shader, 1, &fragment_shader_src, NULL);
	glCompileShader(fragment_shader);
	{
		GLint success;
		GLchar infoLog[512];
		glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
		if(!success) {
			printf("Error compiling frag shader %.*s \n", 512, infoLog);
		} else {
			printf("Frag shader compiled successfully\n");
		}
	}	

	renderer->shader_program = glCreateProgram();
	glAttachShader(renderer->shader_program, vertex_shader);
	glAttachShader(renderer->shader_program, fragment_shader);
	glLinkProgram(renderer->shader_program);

	glUseProgram(renderer->shader_program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	renderer->screen_loc = glGetUniformLocation(renderer->shader_program, "screen");
}

GLuint ogl_add_texture(uint8* data, isize w, isize h) 
{
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	//TODO(will) do error checking?
	printf("Adding texture: %d \n", glGetError());
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
	return texture;
}
	

GLuint ogl_load_texture(char* filename)
{
	int w, h, n;
	char file[File_Path_Max_Length];
	const char* base_path = SDL_GetBasePath();
	isize len = snprintf(file, File_Path_Max_Length, "%s%s", base_path, filename);
	uint8* data = (uint8*)stbi_load(file, &w, &h, &n, STBI_rgb_alpha);
	//TODO(will) do error checking
	GLuint texture = ogl_add_texture(data, w, h);
	if(texture == NULL) {
		printf("There was an error loading %s \n", filename);
	}
	STBI_FREE(data);
	return texture;
}

void renderer_start(Renderer* renderer, Game* game)
{
	renderer->data_index = 0;
	renderer->sprite_count = 0;
	glUseProgram(renderer->shader_program);
	glUniform4f(renderer->screen_loc, 0, 0, game->width, game->height);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->texture);
}

#define _renderer_push(f) renderer->sprite_data[renderer->data_index++] = f

void renderer_push_sprite(Renderer* renderer, Sprite* s)
{
	renderer->sprite_count++;

	_renderer_push(s->position.x);
	_renderer_push(s->position.y);
	_renderer_push(s->center.x);
	_renderer_push(s->center.y);
	_renderer_push(s->angle);
	_renderer_push(s->scale.x);
	_renderer_push(s->scale.y);
	_renderer_push(s->texture.x);
	_renderer_push(s->texture.y);
	_renderer_push(s->texture.w);
	_renderer_push(s->texture.h);
	_renderer_push(s->color.x);
	_renderer_push(s->color.y);
	_renderer_push(s->color.z);
	_renderer_push(s->color.w);
}

void renderer_draw(Renderer* renderer)
{
	glBindVertexArray(renderer->vao);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	glBufferData(GL_ARRAY_BUFFER, renderer->data_index * sizeof(real), renderer->sprite_data, GL_STREAM_DRAW);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, renderer->sprite_count);
	glBindVertexArray(0);
}

