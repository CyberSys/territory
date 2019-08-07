#include "view.h"

bool View::Init(){
  //Initialize SDL
  if( SDL_Init( SDL_INIT_VIDEO ) < 0 ){
    printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
    return false;
  }
  //Initialize SDL_TTF
	TTF_Init();

  //Initialize the Window and Context
  gWindow = SDL_CreateWindow( "Territory", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
  if( gWindow == NULL ){
    printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
    return false;
  }
  gContext = SDL_GL_CreateContext(	gWindow );

  //Initialize OPENGL Stuff
	SDL_GL_SetSwapInterval(1);
	glewExperimental = GL_TRUE;
	glewInit();

  //Configure Global OpenGL State
  glEnable( GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
  glEnable(GL_BLEND) ;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);

  //Setup Shaders
  setupShaders();
  setupShadow();
  sprite.loadImage("hunter.png");
  sprite.setupBuffer();

  //Stuff to get the sprite looking right
  sprite.model = glm::translate(sprite.model, glm::vec3(0.0f, 0.5f, 0.0f));
  glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f);
  sprite.model = glm::rotate(sprite.model, glm::radians(45.0f), axis);

  return true;
}

void View::setupShaders(){
  //Setup Cube Shader
  cubeShader.setup("default.vs", "default.fs");
  cubeShader.addAttribute(0, "in_Position");
  cubeShader.addAttribute(1, "in_Color");
  cubeShader.addAttribute(2, "in_Normal");

  //Setup Depthshader
  depthShader.setup("rendershadow.vs", "rendershadow.fs");
  depthShader.addAttribute(0, "in_Position");

  //Setup Debugshader
  debugShader.setup("debug.vs", "debug.fs");
  debugShader.addAttribute(0, "in_Quad");
  debugShader.addAttribute(1, "in_Tex");

  //Setup Spriteshader
  spriteShader.setup("sprite.vs", "sprite.fs");
  debugShader.addAttribute(0, "in_Quad");
  debugShader.addAttribute(1, "in_Tex");

}

void View::cleanup(){
  //Cleanup Models
  for(unsigned int i = 0; i < models.size(); i++){
    //Cleanup the Models
    models[i].cleanup();
  }

  //Cleanup Shaders
  cubeShader.cleanup();
  depthShader.cleanup();
  debugShader.cleanup();

  //Cleanup Depthmap Stuff
  glDeleteTextures(1, &depthMap);
  glDeleteFramebuffers(1, &depthMapFBO);

  //Destroy Context and Window
	SDL_GL_DeleteContext( gContext );
	SDL_DestroyWindow( gWindow );

  //Quit SDL subsystems
  TTF_Quit();
  SDL_Quit();
}

void View::loadChunkModels(World world){
  //Empty the Models
  models.clear();

  //For every loaded chunk
  for(unsigned int l = 0; l < world.chunks.size(); l++){
    //Construct a New Model Object
    Model model;

    //Loop over all elements inside the chunk
    for(int i = 0; i < world.chunkSize; i++){
      for(int j = 0; j < world.chunkSize; j++){
        for(int k = 0; k < world.chunkSize; k++){
          //Get the current block type
          BlockType _type = world.chunks[l].data.getPosition(i,j,k);
          if(_type != 0){
            //Set this shit up
            std::ifstream t("/home/nick/Documents/Game Workspace/23_HexelRender/source/include/positions.txt");
            std::string x;
            std::string y;
            std::string z;
            do{
              t >> x;
              t >> y;
              t >> z;
              model.positions.push_back(std::atof(x.c_str())+i);
              model.positions.push_back(std::atof(y.c_str())+j);
              model.positions.push_back(std::atof(z.c_str())+k);
            }
            while (t.good());
            //Remove 3 of them for some reason. It does it once too often.
            model.positions.pop_back();
            model.positions.pop_back();
            model.positions.pop_back();

            //We need to get a color
            glm::vec4 color = world.getColorByID(world.chunks[l].data.getPosition(i, j, k));
            //For all 36 triangles we add a color (12*3)
            for(unsigned int m = 0; m < 36; m++){
              model.colors.push_back(color.x);
              model.colors.push_back(color.y);
              model.colors.push_back(color.z);
              model.colors.push_back(color.w);
            }
          }
        }
      }
    }

    //Compute all Surface Normals
    //For every surface we compute the surface normal
    for(unsigned int m = 0; m < model.positions.size()/9; m++){
      glm::vec3 v1 = glm::vec3(model.positions[m*9], model.positions[m*9+1], model.positions[m*9+2]);
      glm::vec3 v2 = glm::vec3(model.positions[m*9+3], model.positions[m*9+4], model.positions[m*9+5]);
      glm::vec3 v3 = glm::vec3(model.positions[m*9+6], model.positions[m*9+7], model.positions[m*9+8]);
      glm::vec3 edge1 = v3-v1;
      glm::vec3 edge2 = v2-v1;
      glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
      //We add the x,y,z value 3 times (once for each for every vertex)
      for(int n = 0; n < 3; n++){
        model.normals.push_back(normal.x);
        model.normals.push_back(normal.y);
        model.normals.push_back(normal.z);
      }
    }

    //Generate the model from all this data now
    //The question is if we need to
    model.setup();

    //Translate it according to the chunk position relative to the player position
    glm::vec3 axis = world.chunks[l].pos-world.chunkPos;
    axis.x *= world.chunkSize;
    axis.y *= world.chunkSize;
    axis.z *= world.chunkSize;
    axis -= world.playerPos;
    model.translate(axis);

    //Add the Model
    models.push_back(model);
  }
}

bool View::setupShadow(){
  //Setup the QUAD VAO and VBO
  //Generate the quadVAO
  GLfloat vert[] = {-1.0, -1.0, -1.0,
                    -1.0, -0.5, -1.0,
                    -0.5, -1.0, -1.0,
                    -0.5, -0.5, -1.0};

  GLfloat tex[] = {0.0, 0.0,
                   0.0, 1.0,
                   1.0, 0.0,
                   1.0, 1.0};

  //Load it into a buffer thingy
  glGenVertexArrays(1, depthVAO);
  glBindVertexArray(depthVAO[0]);
  glGenBuffers(2, depthVBO);

  //Buff' it
  glBindBuffer(GL_ARRAY_BUFFER, depthVBO[0]);
  glBufferData(GL_ARRAY_BUFFER, 12*sizeof(GLfloat), &vert[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

  glBindBuffer(GL_ARRAY_BUFFER, depthVBO[1]);
  glBufferData(GL_ARRAY_BUFFER, 8*sizeof(GLfloat), &tex[0], GL_STATIC_DRAW);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);


  //Generate Framebuffer and Texture
  glGenFramebuffers(1, &depthMapFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);

  //Generate and Bind the Texture
  glGenTextures(1, &depthMap);
  glBindTexture(GL_TEXTURE_2D, depthMap);

  //Generate an Empty Image Here
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  //Add Parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
  glDrawBuffer(GL_NONE);

  //Check Success
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    return false;

  return true;
}


void View::render(World world){
  //Move the light ayy
  //depthCamera = glm::rotate(depthCamera, glm::radians(-0.1f), glm::vec3(0.0, 1.0, 0.0));
  if(!mapView){
    //Render the Regular View
    renderShadow();
    renderScene();
    renderSprite();
  }
  else{
    //Render the Map View
    //Somehow with LOD
  }

  if(debug){
    //Render the Depth Debugquad
    renderDepth();
  }

  //Swap the Window
  SDL_GL_SwapWindow(gWindow);
}

void View::renderSprite(){
  //Render the Sprite
  spriteShader.useProgram();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sprite.texture);
  spriteShader.setInt("spriteTexture", 0);

  //Set the Scale Uniform?
  spriteShader.setMat4("mvp", projection*camera*sprite.model);

  //Draw
  glBindVertexArray(sprite.vao[0]);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void View::renderDepth(){
  //I guess just make a quad
  debugShader.useProgram();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  debugShader.setInt("shadowMap", 0);

  glBindVertexArray(depthVAO[0]);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void View::renderShadow(){
  //Clear the Stuff
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  glCullFace(GL_BACK);
  glClear(GL_DEPTH_BUFFER_BIT);

  //Use the Shader
  depthShader.useProgram();
  depthShader.setMat4("dvp", depthProjection * depthCamera * glm::mat4(1.0f));

  //Use the Framebuffer
  glClear(GL_DEPTH_BUFFER_BIT);

  //Activate the Texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  debugShader.setInt("shadowMap", 0);

  //Loop over the Models to Render to Shadowmap
  for(unsigned int i = 0; i < models.size(); i++){
    //Set the Projection Stuff
    depthShader.setMat4("model", models[i].model);
    //Render the Model
    models[i].render();
  }

  //Unbind the Framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void View::renderScene(){
  //Clear the Color and Stuff
  glClearColor(0.6f, 0.9f, 0.8f, 1.0f); //Blue

  //glClearColor(0.25f, 0.6f, 0.4f, 1.0f); //Green
  //glClearColor(0.25f, 0.15f, 0.25f, 1.0f); //Purple
  glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glCullFace(GL_BACK);

  glm::mat4 biasMatrix(
			0.5, 0.0, 0.0, 0.0,
			0.0, 0.5, 0.0, 0.0,
			0.0, 0.0, 0.5, 0.0,
			0.5, 0.5, 0.5, 1.0
		);

  //Use the Shader
  cubeShader.useProgram();    //Use the model's shader
  cubeShader.setVec3("lightCol", lightCol);
  cubeShader.setVec3("lightPos", lightPos);
  //Set the other matrices
  cubeShader.setMat4("projection", projection);
  cubeShader.setMat4("camera", camera);
  cubeShader.setMat4("dbmvp", biasMatrix * depthProjection * depthCamera * glm::mat4(1.0f));
  cubeShader.setMat4("dmvp", depthProjection * depthCamera * glm::mat4(1.0f));


  //Activate and Bind the Texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  cubeShader.setInt("shadowMap", 0);

  //Loop over the Stuff
  for(unsigned int i = 0; i < models.size(); i++){
    //View Projection Matrix
    cubeShader.setMat4("model", models[i].model);
    //Render the Model
    models[i].render();               //Render
  }
}

void View::calcFPS(){
  //We getting 60 FPS
  FPS = (int)(1000.0f/(SDL_GetTicks()-ticks));
  ticks = SDL_GetTicks();
  std::cout<<FPS<<std::endl;
}