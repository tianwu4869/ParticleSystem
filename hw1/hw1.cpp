// hw1.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "glad\glad.h"
#include "include\SDL.h"
#include "include\SDL_opengl.h"

//Include order can matter here

#include <cstdio>

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <time.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <list>
#include <mmsystem.h>
#include "objLoader.h"
#include "particleSystem.h"
using namespace std;

int screenWidth = 800;
int screenHeight = 800;
bool saveOutput = false;
float lastTime = 0;
float viewRotate = 0.01;
//SJG: Store the object coordinates
//You should have a representation for the state of each object

float colR = 1, colG = 1, colB = 1;
float rand01() {
	return rand() / (float)RAND_MAX;
}

glm::vec3 position(0, 3.0f, 8.0f);  //Cam Position
glm::vec3 center(0, 1.0f, 0);  //Look at point

bool DEBUG_ON = true;
GLuint InitShader(const char* vShaderFileName, const char* fShaderFileName);
bool fullscreen = false;

verticeAndIndices* Floor;
verticeAndIndices* football;
vbi* floorBuffer;
vbi* footballBuffer;
glm::vec3 floorPosition(0, 0, 0);
glm::vec3 floorVelocity(0, 0, 0);
glm::vec3 footballPosition(-0.5f, 3.0f, 0);
glm::vec3 footballVelocity(0, 0, 0);

glm::vec3 birthplace(0, 0.2f, 0);
glm::vec3 waterColor(0, 0.74f, 1.0f);
glm::vec3 fireColor(0.92f, 1.0f, 0.2f);
glm::vec3 fireworkVelocity(0, 2.0f, 0);
glm::vec3 fireworkColor(0.7f, 0.4f, 0.7f);
GLfloat waterLifespan = 2;
GLfloat fireLifespan = 0.75;
GLfloat explosionLifespan = 0.6f;
GLuint explosionNumber = 40;
GLfloat fountainScale = 1.0;

GLuint generationRate = 1000;

int main(int argc, char *argv[]) {
	SDL_Init(SDL_INIT_VIDEO);  //Initialize Graphics (for OpenGL)
							   //Ask SDL to get a recent version of OpenGL (3 or greater)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	//Create a window (offsetx, offsety, width, height, flags)
	SDL_Window* window = SDL_CreateWindow("My OpenGL Program", 100, 100, screenWidth, screenHeight, SDL_WINDOW_OPENGL);

	//Create a context to draw in
	SDL_GLContext context = SDL_GL_CreateContext(window);

	//Load OpenGL extentions with GLAD
	if (gladLoadGLLoader(SDL_GL_GetProcAddress)) {
		printf("\nOpenGL loaded\n");
		printf("Vendor:   %s\n", glGetString(GL_VENDOR));
		printf("Renderer: %s\n", glGetString(GL_RENDERER));
		printf("Version:  %s\n\n", glGetString(GL_VERSION));
	}
	else {
		printf("ERROR: Failed to initialize OpenGL context.\n");
		return -1;
	}

	//load Models
	Floor = objLoader("models/floor/floor.obj", "models/floor/floor.mtl", "models/floor/");
	football = objLoader("models/football/football.obj", "models/football/football.mtl", "models/football/");

	for (int i = 0; i < Floor->num_vertices; i++) {
		Floor->vertices[i * 8] /= 8;
		Floor->vertices[i * 8 + 1] /= 4;
		Floor->vertices[i * 8 + 2] /=8 ;
	}
	
	for (int i = 0; i < football->num_vertices; i++) {
		football->vertices[i * 8] /= 240;
		football->vertices[i * 8 + 1] /= 240;
		football->vertices[i * 8 + 2] /= 240;
	}
	GLfloat max = -999, min = 999;
	for (int i = 0; i < football->num_vertices; i++) {
		if (football->vertices[i * 8 + 1] > max) {
			max = football->vertices[i * 8 + 1];
		}
		if (football->vertices[i * 8 + 1] < min) {
			min = football->vertices[i * 8 + 1];
		}
	}
	cout << endl;
	cout << min << ' ' << max << endl;

	int texturedShader = InitShader("vertexTex.glsl", "fragmentTex.glsl");
	int particleShader = InitShader("particleVertex.glsl", "particleFragment.glsl");

	//create buffer for models
	floorBuffer = newBufferForModel(Floor, texturedShader);
	footballBuffer = newBufferForModel(football, texturedShader);

	//load texture for models
	generateTexture(Floor);
	generateTexture(football);

	GLint uniView = glGetUniformLocation(texturedShader, "view");
	GLint uniProj = glGetUniformLocation(texturedShader, "proj");

	GLint uniView2 = glGetUniformLocation(particleShader, "view");
	GLint uniProj2 = glGetUniformLocation(particleShader, "proj");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PROGRAM_POINT_SIZE);
	//glEnable(GL_POINT_SIZE_GRANULARITY);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	GLuint vao, vbo;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	list<particle> fountain;
	list<particle> fire;
	list<particle> firework;
	int nbFrames = 0;
	double lastTime = SDL_GetTicks() / 1000.f;
	//Event Loop (Loop forever processing each event as fast as possible)
	SDL_Event windowEvent;
	bool quit = false;
	double frameLastTime = SDL_GetTicks() / 1000.f;
	while (!quit) {
		while (SDL_PollEvent(&windowEvent)) {  //inspect all events in the queue
			if (windowEvent.type == SDL_QUIT) quit = true;
			//List of keycodes: https://wiki.libsdl.org/SDL_Keycode - You can catch many special keys
			//Scancode referes to a keyboard position, keycode referes to the letter (e.g., EU keyboards)
			if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_ESCAPE)
				quit = true; //Exit event loop
			if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_f) { //If "f" is pressed
				fullscreen = !fullscreen;
				SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0); //Toggle fullscreen 
			}

			//SJG: Use key input to change the state of the object
			//     We can use the ".mod" flag to see if modifiers such as shift are pressed
			if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_UP) { //If "up key" is pressed
				glm::vec3 direction = center - position;
				position.x = position.x + direction.x * 0.01;
				position.z = position.z + direction.z * 0.01;
				center.x = center.x + direction.x * 0.01;
				center.z = center.z + direction.z * 0.01;
			}
			if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_DOWN) { //If "down key" is pressed
				glm::vec3 direction = center - position;
				position.x = position.x - direction.x * 0.01;
				position.z = position.z - direction.z * 0.01;
				center.x = center.x - direction.x * 0.01;
				center.z = center.z - direction.z * 0.01;
			}
			if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_SPACE) { //If "space key" is pressed
				glm::vec3 direction = center - position;
				position.y = position.y + 0.03;
				center.y = center.y + 0.03;
			}
			if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_x) { //If "x key" is pressed
				glm::vec3 direction = center - position;
				position.y = position.y - 0.03;
				center.y = center.y - 0.03;
			}
			if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_LEFT) { //If "LEFT key" is pressed
				glm::vec3 direction = center - position;
				glm::mat4 rot;
				rot = glm::rotate(rot, viewRotate * 3.14f, glm::vec3(0.0f, 1.0f, 0.0f));
				glm::vec4 new_direction = rot * glm::vec4(direction, 1);
				center.x = new_direction.x + position.x;
				center.z = new_direction.z + position.z;
			}
			if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_RIGHT) { //If "RIGHT key" is pressed
				glm::vec3 direction = center - position;
				glm::mat4 rot;
				rot = glm::rotate(rot, viewRotate * -3.14f, glm::vec3(0.0f, 1.0f, 0.0f));
				glm::vec4 new_direction = rot * glm::vec4(direction, 1);
				center.x = new_direction.x + position.x;
				center.z = new_direction.z + position.z;
			}
			if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_n) { //If "n key" is pressed
				if ((fountainScale - 0.05f) > 0) {
					fountainScale -= 0.05f;
				}
			}
			if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_m) { //If "m key" is pressed
				fountainScale += 0.05f;
			}
		}

		// Clear the screen to default color
		glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(particleShader);

		GLenum err;
		while ((err = glGetError()) != GL_NO_ERROR) {
			cerr << "OpenGL error: " << err << endl;
		}

		//if (!saveOutput) lastTime = SDL_GetTicks() / 1000.f;
		//if (saveOutput) lastTime += .07; //Fix framerate at 14 FPS

		glm::mat4 view = glm::lookAt(
			position,						//Cam Position
			center,						//Look at point
			glm::vec3(0.0f, 1.0f, 0.0f));   //Up
		glUniformMatrix4fv(uniView2, 1, GL_FALSE, glm::value_ptr(view));
		
		glm::mat4 proj = glm::perspective(3.14f / 4, screenWidth / (float)screenHeight, 0.01f, 10.0f); //FOV, aspect, near, far
		glUniformMatrix4fv(uniProj2, 1, GL_FALSE, glm::value_ptr(proj));

		float dt;
		dt = SDL_GetTicks() / 1000.f - lastTime;
		//cout << dt <<endl;
		lastTime = SDL_GetTicks() / 1000.f;
		/*fountain = manageFountain(fountain, generationRate, dt, birthplace, waterColor, waterLifespan, fountainScale, footballPosition, 0.26f);
		sendDataToBuffer(fountain, vao, vbo, particleShader);*/
		/*fire = manageFire(fire, generationRate, dt, birthplace, fireColor, fireLifespan);
		sendDataToBuffer(fire, vao, vbo, particleShader);*/
		firework = blastOff(firework, dt, fireworkVelocity, birthplace, fireworkColor, explosionLifespan, explosionNumber);
		firework = manageFireworks(firework, dt);
		sendDataToBuffer(firework, vao, vbo, particleShader);
		
		
		double currentTime = SDL_GetTicks() / 1000.f;
		nbFrames++;
		if (currentTime - frameLastTime >= 1.0) { // If last prinf() was more than 1 sec ago
											 // printf and reset timer
			cout <<"Number of particles: "<<firework.size()<<" ;"<<"FPS: "<<nbFrames<<endl;
			//printf("%f ms/frame\n", 1000.0 / double(nbFrames));
			nbFrames = 0;
			frameLastTime += 1.0;
		}
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//cout << fountain.size()<<endl;
		glPointSize(6.0f);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glDrawArrays(GL_POINTS, 0, firework.size());
		glBindVertexArray(0);
		

		glUseProgram(texturedShader);
		glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));
		drawGeometry(texturedShader, dt);
		
		SDL_GL_SwapWindow(window); //Double buffering
	}
	glDeleteProgram(particleShader);
	glDeleteProgram(texturedShader);
	//Clean Up
	
	//glDeleteBuffers(1, vbo);
	//glDeleteVertexArrays(1, &vao);

	SDL_GL_DeleteContext(context);
	SDL_Quit();
	return 0;
}

void drawGeometry(int shaderProgram, GLfloat dt) {

	GLint uniColor = glGetUniformLocation(shaderProgram, "inColor");
	glm::vec3 colVec(colR, colG, colB);
	glUniform3fv(uniColor, 1, glm::value_ptr(colVec));

	GLint cam = glGetUniformLocation(shaderProgram, "cam_position");
	glUniform3fv(cam, 1, glm::value_ptr(position));

	// central model
	
	footballVelocity += glm::vec3(0, -9.8f, 0) * dt;
	footballPosition += footballVelocity * dt;
	if (footballPosition.y < 0.25f) {
		footballPosition.y = 0.25;
		footballVelocity *= -0.8f;
	}

	glm::mat4 transform3;
	//transform3 = glm::rotate(transform3, lastTime * 3.14f, glm::vec3(0.0, 1.0, 0.0));
	transform3 = glm::translate(transform3, footballPosition);
	//display(shaderProgram, football, footballBuffer, transform3);

	glm::mat4 transform4;
	transform4 = glm::translate(transform4, floorPosition);
	display(shaderProgram, Floor, floorBuffer, transform4);
}

// Create a NULL-terminated string by reading the provided file
static char* readShaderSource(const char* shaderFile) {
	FILE *fp;
	long length;
	char *buffer;

	// open the file containing the text of the shader code
	fp = fopen(shaderFile, "r");

	// check for errors in opening the file
	if (fp == NULL) {
		printf("can't open shader source file %s\n", shaderFile);
		return NULL;
	}

	// determine the file size
	fseek(fp, 0, SEEK_END); // move position indicator to the end of the file;
	length = ftell(fp);  // return the value of the current position

						 // allocate a buffer with the indicated number of bytes, plus one
	buffer = new char[length + 1];

	// read the appropriate number of bytes from the file
	fseek(fp, 0, SEEK_SET);  // move position indicator to the start of the file
	fread(buffer, 1, length, fp); // read all of the bytes

								  // append a NULL character to indicate the end of the string
	buffer[length] = '\0';

	// close the file
	fclose(fp);

	// return the string
	return buffer;
}

// Create a GLSL program object from vertex and fragment shader files
GLuint InitShader(const char* vShaderFileName, const char* fShaderFileName) {
	GLuint vertex_shader, fragment_shader;
	GLchar *vs_text, *fs_text;
	GLuint program;

	// check GLSL version
	printf("GLSL version: %s\n\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	// Create shader handlers
	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	// Read source code from shader files
	vs_text = readShaderSource(vShaderFileName);
	fs_text = readShaderSource(fShaderFileName);

	// error check
	if (vs_text == NULL) {
		printf("Failed to read from vertex shader file %s\n", vShaderFileName);
		exit(1);
	}
	else if (DEBUG_ON) {
		printf("Vertex Shader:\n=====================\n");
		printf("%s\n", vs_text);
		printf("=====================\n\n");
	}
	if (fs_text == NULL) {
		printf("Failed to read from fragent shader file %s\n", fShaderFileName);
		exit(1);
	}
	else if (DEBUG_ON) {
		printf("\nFragment Shader:\n=====================\n");
		printf("%s\n", fs_text);
		printf("=====================\n\n");
	}

	// Load Vertex Shader
	const char *vv = vs_text;
	glShaderSource(vertex_shader, 1, &vv, NULL);  //Read source
	glCompileShader(vertex_shader); // Compile shaders

									// Check for errors
	GLint  compiled;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		printf("Vertex shader failed to compile:\n");
		if (DEBUG_ON) {
			GLint logMaxSize, logLength;
			glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &logMaxSize);
			printf("printing error message of %d bytes\n", logMaxSize);
			char* logMsg = new char[logMaxSize];
			glGetShaderInfoLog(vertex_shader, logMaxSize, &logLength, logMsg);
			printf("%d bytes retrieved\n", logLength);
			printf("error message: %s\n", logMsg);
			delete[] logMsg;
		}
		exit(1);
	}

	// Load Fragment Shader
	const char *ff = fs_text;
	glShaderSource(fragment_shader, 1, &ff, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compiled);

	//Check for Errors
	if (!compiled) {
		printf("Fragment shader failed to compile\n");
		if (DEBUG_ON) {
			GLint logMaxSize, logLength;
			glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &logMaxSize);
			printf("printing error message of %d bytes\n", logMaxSize);
			char* logMsg = new char[logMaxSize];
			glGetShaderInfoLog(fragment_shader, logMaxSize, &logLength, logMsg);
			printf("%d bytes retrieved\n", logLength);
			printf("error message: %s\n", logMsg);
			delete[] logMsg;
		}
		exit(1);
	}

	// Create the program
	program = glCreateProgram();

	// Attach shaders to program
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	// Link and set program to use
	glLinkProgram(program);

	return program;
}


