// Defined before OpenGL and GLUT includes to avoid deprecation message in OSX
#define GL_SILENCE_DEPRECATION
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "cyCodeBase/cyTriMesh.h"
#include "cyCodeBase/cyMatrix.h"
#include "cyCodeBase/cyGL.h"
#include "cyCodeBase/cyPoint.h"
#include "lodepng.h"

int width = 1280, height = 720;
GLFWwindow* window;
cy::TriMesh objectMesh;
cy::TriMesh lightMesh;
cy::GLTextureCubeMap skybox;
cy::GLSLProgram prog, progPlane, progLight, progShadowMap;
bool doObjRotate = false, doObjZoom = false, doLightRotate = false, doPlaneRotate = false;
double rotX = 5.23599, rotY = 0, distZ = 4;
float objRotX = 5.23599, objRotY= 0, objDistZ = 100.0f;
double lastX, lastY;
double lightX = -10, lightY = 30, lightZ = 25;
double shadowBias = -0.000001;
unsigned int textureWidth = 512, textureHeight = 512;
unsigned int relfectionTextureWidth = 1920, relfectionTextureHeight = 1080;
unsigned int shadowTextureWidth = 2560, shadowTextureHeight = 1440;
unsigned int skyboxtextureWidth = 2048, skyboxtextureHeight = 2048;
std::vector<cy::Vec3f> processedVertices, processedNormals;
std::vector<cy::Vec3f> processedVerticesPlane;
std::vector<cy::Vec3f> processedNormalsPlane;
std::vector<cy::Vec2f> processedTexCoords;
std::vector<cy::Vec2f> processedTexCoordsPlane;
std::vector<cy::Vec3f> processedVerticesLight;
std::vector<unsigned char> diffuseTextureData;
std::vector<unsigned char> specularTextureData;
std::vector<std::string> skyboxTexturesNames;
GLuint specularTexID, diffuseTexID, tbo;

GLclampf Red = 0.0f, Green = 0.0f, Blue = 0.0f, Alpha = 1.0f;

void renderScene()
{
    // Clear Color buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(Red, Green, Blue, Alpha);
    glDrawArrays(GL_TRIANGLES, 0, processedVertices.size());
}

bool compileShaders() {
    bool shaderSuccess = prog.BuildFiles("shader.vert", "shader.frag");
    return shaderSuccess;
}

bool compilePlaneShaders() {
    bool shaderSuccess = progPlane.BuildFiles("shaderPlane.vert", "shaderPlane.frag");
    return shaderSuccess;
}

bool compileLightShaders() {
    bool shaderSuccess = progLight.BuildFiles("shaderLight.vert", "shaderLight.frag");
    return shaderSuccess;
}

bool compileDepthShaders() {
    bool shaderSuccess = progShadowMap.BuildFiles("shaderDepth.vert", "shaderDepth.frag");
    return shaderSuccess;
}

void computeVerticesPlane() {
    processedVerticesPlane.push_back(cy::Vec3f(50.0f, 0.0f, -50.0f));
    processedVerticesPlane.push_back(cy::Vec3f(-50.0f, 0.0f, -50.0f));
    processedVerticesPlane.push_back(cy::Vec3f(-50.0f, 0.0f, 50.0f));
    processedVerticesPlane.push_back(cy::Vec3f(-50.0f, 0.0f, 50.0f));
    processedVerticesPlane.push_back(cy::Vec3f(50.0f, 0.0f, 50.0f));
    processedVerticesPlane.push_back(cy::Vec3f(50.0f, 0.0f, -50.0f));
}

void computeNormalsPlane() {
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
}

void processNormalKeyCB(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case GLFW_KEY_F6:
            std::cout << "Recompiling shaders..." << std::endl;
            if (!compileShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            if (!compilePlaneShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            if (!compileLightShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            if (!compileDepthShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            break;
        case GLFW_KEY_UP:
            shadowBias += 0.000001f;
            std::cout << "Shadow bias: " << shadowBias << std::endl;
            break;
        case GLFW_KEY_DOWN:
            shadowBias -= 0.000001f;
            std::cout << "Shadow bias: " << shadowBias << std::endl;
            break;
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            doLightRotate = true;
            break;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT:
            doPlaneRotate = true;
            break;
        }
    } else if (action == GLFW_RELEASE) {
        switch (key) {
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            doLightRotate = false;
            break;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT:
            doPlaneRotate = false;
            break;
        }
    }
}

static void error_callback(int error, const char* description)
{
    std::cerr << "Error: " << description << std::endl;
}
double deg2rad (double degrees) {
    return degrees * 4.0 * atan (1.0) / 180.0;
}

void processMouseButtonCB(GLFWwindow* window, int button, int action, int mods)
{
    double xpos, ypos;
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        doObjZoom = true;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        doObjZoom = false;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        doObjRotate = true;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        doObjRotate = false;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    }
}

void processMousePosCB(GLFWwindow* window, double xpos, double ypos)
{
    double xDiff = lastX - xpos;
    double yDiff = lastY - ypos;
    if (doLightRotate && doObjRotate) {
        lightX += xDiff * 0.1;
        lightZ += yDiff * 0.1;
    }
    // Calculate camera zoom based on mouse movement in y direction
    if (doObjZoom && !doPlaneRotate) {
        objDistZ += yDiff * 0.1;
    }

    // Calculate plane zoom based on mouse movement in y direction
    if (doPlaneRotate && doObjZoom) {
        distZ += yDiff * 0.05;
    }

    // Calculate camera rotation based on mouse movement in x direction
    if (doObjRotate && !doLightRotate && !doPlaneRotate) {
        objRotX += yDiff * 0.005;
        objRotY += xDiff * 0.005;
    }

    // Calculate plane rotation based on mouse movement in x direction
    if (doPlaneRotate && doObjRotate) {
        rotX -= yDiff * 0.005;
        rotY -= xDiff * 0.005;
    }

    lastX  = xpos;
    lastY = ypos;
}

int main(int argc, char** argv)
{
    glfwSetErrorCallback(error_callback);
    
    // Initialize GLFW
    if (!glfwInit())
        exit(EXIT_FAILURE);
        
    // Create a windowed mode window and its OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(width, height, "Shadow Mapping", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);

    //Initialize GLEW
    glewExperimental = GL_TRUE; 
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        std::cout << "Error: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    // Print and test OpenGL context infos
    std::cout << glGetString(GL_VERSION) << std::endl;
    std::cout << glGetString(GL_RENDERER) << std::endl;

    // Setup GLFW callbacks
    glfwSetKeyCallback(window, processNormalKeyCB);
    glfwSetMouseButtonCallback(window, processMouseButtonCB);
    glfwSetCursorPosCallback(window, processMousePosCB);
    glfwSwapInterval(1);

    //OpenGL initializations
    // glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    // CY_GL_REGISTER_DEBUG_CALLBACK;

    //Load object mesh and texture mapping
    bool meshSuccess = objectMesh.LoadFromFileObj(argv[1]);
    if (!meshSuccess)
    {
        std::cout << "Error loading mesh" << std::endl;
        return 1;
    }

    //Compute normals
    if (objectMesh.HasNormals()) {
        objectMesh.ComputeNormals();
    }

    //Load vertices for object faces
    for (int i = 0; i < objectMesh.NF(); i++) 
    {
        cy::TriMesh::TriFace face = objectMesh.F(i);
        cy::TriMesh::TriFace textureface;
        if (objectMesh.HasTextureVertices()) {
            textureface = objectMesh.FT(i);
        }
        for (int j = 0; j < 3; j++) 
        {
            cy::Vec3f pos = objectMesh.V(face.v[j]);
            processedVertices.push_back(pos);

            if (objectMesh.HasNormals()) {
                cy::Vec3f norm = objectMesh.VN(face.v[j]);
                processedNormals.push_back(norm);
            }

            if (objectMesh.HasTextureVertices()) {
                cy::Vec3f texcoord = objectMesh.VT(textureface.v[j]);
                processedTexCoords.push_back(cy::Vec2f(texcoord.x, texcoord.y));
            }
        }
    }

    if (objectMesh.NM() > 0) {
        // Load diffuse texture data from png file
        bool ambientTextureError = lodepng::decode(diffuseTextureData, textureWidth, textureHeight, objectMesh.M(0).map_Kd.data);
        if (ambientTextureError)
        {
            std::cout << "Error loading texture" << std::endl;
            return 1;
        }

        // Load sepcular texture data from png file
        bool specularTextureError = lodepng::decode(specularTextureData, textureWidth, textureHeight, objectMesh.M(0).map_Ks.data);
        if (specularTextureError)
        {
            std::cout << "Error loading texture" << std::endl;
            return 1;
        }
    }

    //Compute bounding box
    objectMesh.ComputeBoundingBox();
    cy::Vec3f objectCenter = (objectMesh.GetBoundMax() - objectMesh.GetBoundMin())/2;
    cy::Vec3f objectOrigin = objectMesh.GetBoundMin() + objectCenter;
    
    // Compute vertices and tex coord for plane
    computeVerticesPlane();
    computeNormalsPlane();

    //Load light mesh for light source
    meshSuccess = lightMesh.LoadFromFileObj("sphere.obj");
    if (!meshSuccess)
    {
        std::cout << "Error loading mesh" << std::endl;
        return 1;
    }

    //Compute normals
    lightMesh.ComputeNormals();

    //Load vertices for light faces
    for (int i = 0; i < lightMesh.NF(); i++) 
    {
        cy::TriMesh::TriFace face = lightMesh.F(i);
        for (int j = 0; j < 3; j++) 
        {
            cy::Vec3f pos = lightMesh.V(face.v[j]);
            processedVerticesLight.push_back(pos);
        }
    }

    //Create vertex array object
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    //Create VBO for vertex data
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedVertices.size(), &processedVertices[0], GL_STATIC_DRAW);
    
    //Create VBO for normal data
    GLuint nbo;
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedNormals.size(), &processedNormals[0], GL_STATIC_DRAW);
    
    // Create VBO for plane vertex data
    GLuint vboplane;
    glGenBuffers(1, &vboplane);
    glBindBuffer(GL_ARRAY_BUFFER, vboplane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedVerticesPlane.size(), &processedVerticesPlane[0], GL_STATIC_DRAW);

    GLuint nboplane;
    glGenBuffers(1, &nboplane);
    glBindBuffer(GL_ARRAY_BUFFER, nboplane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedNormalsPlane.size(), &processedNormalsPlane[0], GL_STATIC_DRAW);

    //Create VBO for plane texture coordinates data
    GLuint tboplane;
    glGenBuffers(1, &tboplane);
    glBindBuffer(GL_ARRAY_BUFFER, tboplane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedTexCoordsPlane.size(), &processedTexCoordsPlane[0], GL_STATIC_DRAW);

    //Create VBO for light vertex data
    GLuint vbolight;
    glGenBuffers(1, &vbolight);
    glBindBuffer(GL_ARRAY_BUFFER, vbolight);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedVerticesLight.size(), &processedVerticesLight[0], GL_STATIC_DRAW);

    if (objectMesh.NM() > 0) {
        //Create VBO for texture data
        glGenBuffers(1, &tbo);
        glBindBuffer(GL_ARRAY_BUFFER, tbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec2f)*processedTexCoords.size(), &processedTexCoords[0], GL_STATIC_DRAW);

        //Send diffuse texture data to GPU
        glGenTextures(1, &diffuseTexID);
        glBindTexture(GL_TEXTURE_2D, diffuseTexID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &diffuseTextureData[0]);
        
        //Set diffuse texture parameters
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        //Send specular texture data to GPU
        glGenTextures(1, &specularTexID);
        glBindTexture(GL_TEXTURE_2D, specularTexID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &specularTextureData[0]);

        //Set ambient texture parameters
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    //Setup Shader program
    std::cout << "Compiling shaders..." << std::endl;
    bool shaderSuccess = compileShaders();
    if (!shaderSuccess)
    {
        std::cout << "Error compiling shaders" << std::endl;
        return 1;
    }

    //Setup Shader program
    std::cout << "Compiling plane shaders..." << std::endl;
    bool planeshaderSuccess = compilePlaneShaders();
    if (!planeshaderSuccess)
    {
        std::cout << "Error compiling plane shaders" << std::endl;
        return 1;
    }


    //Setup Shader program
    std::cout << "Compiling cube shaders..." << std::endl;
    bool lightshaderSuccess = compileLightShaders();
    if (!lightshaderSuccess)
    {
        std::cout << "Error compiling cube shaders" << std::endl;
        return 1;
    }

    //Setup Shader program
    std::cout << "Compiling depth shaders..." << std::endl;
    bool depthshaderSuccess = compileDepthShaders();
    if (!depthshaderSuccess)
    {
        std::cout << "Error compiling cube shaders" << std::endl;
        return 1;
    }

    //Loop until the user closes the window
    while (!glfwWindowShouldClose(window))
    {
        // Calculate camera transformations for object
        cy::Matrix4f transMatrix = cy::Matrix4f::Translation(-objectOrigin);
        cy::Matrix4f cameraPosTrans = cy::Matrix4f::RotationXYZ(objRotX, objRotY, 0.0f);
        cy::Vec4f cameraPos =  cameraPosTrans * cy::Vec4f(0, 0, objDistZ, 0);
        cy::Matrix4f viewMatrix = cy::Matrix4f::View(cy::Vec3f(cameraPos.x, cameraPos.y, cameraPos.z), cy::Vec3f(0, 0, 0), cy::Vec3f(0, 1, 0));
        cy::Matrix4f projMatrix = cy::Matrix4f::Perspective(deg2rad(60), float(width)/float(height), 0.1f, 1000.0f);
        cy::Matrix4f m =  transMatrix;
        cy::Matrix4f mv = viewMatrix * m;
        cy::Matrix4f mvp = projMatrix * mv;
        cy::Matrix4f mv_3x3_it = mv.GetInverse().GetTranspose();
        
        // Render to texture binding
        cy::GLRenderDepth2D renderBuffer;
        renderBuffer.Initialize(true, shadowTextureWidth, shadowTextureHeight, GL_DEPTH_COMPONENT32F);
        if (!renderBuffer.IsReady())
        {
            std::cout << "Error initializing render buffer" << std::endl;
            return 1;
        }
        renderBuffer.BindTexture();
        renderBuffer.SetTextureWrappingMode(GL_CLAMP, GL_CLAMP);
        renderBuffer.SetTextureFilteringMode(GL_LINEAR, GL_LINEAR);

        renderBuffer.Bind();
        progShadowMap.Bind();

        // cy::Matrix4f modelLight = cy::Matrix4f::Identity();
        cy::Matrix4f modelLight = cy::Matrix4f::Identity();
        cy::Matrix4f viewLight = cy::Matrix4f::View(cy::Vec3f(lightX, lightY, lightZ), cy::Vec3f(0, 0, 0), cy::Vec3f(0, 1, 0));
        cy::Matrix4f projLight = cy::Matrix4f::Perspective(deg2rad(90), float(width)/float(height), 0.1f, 1000.0f);;
        cy::Matrix4f mvpLight = projLight * viewLight * modelLight;
        progShadowMap["mvp"] = mvpLight;
        progShadowMap.SetAttribBuffer("pos", vbo, 3);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(Red, Green, Blue, Alpha);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glDrawArrays(GL_TRIANGLES, 0, processedVertices.size());
        glDisable(GL_CULL_FACE);

        // Check if render buffer is ready
        if (!renderBuffer.IsComplete())
        {
            std::cout << "Error completing render buffer" << std::endl;
            return 1;
        }
        renderBuffer.Unbind();

        //Set Program and Program Attributes for teapot
        prog.Bind();
        prog["mvp"] = mvp;
        prog["mv"] = mv;
        prog["mnorm"] = m.GetInverse().GetTranspose();
        prog["m"] = m;
        prog["mv_3x3_it"] = mv_3x3_it;
        prog["camera_pos"] = cameraPos;
        prog["light_pos"] =  viewMatrix * cy::Vec4f(lightX, lightY, lightZ, 0);
        prog["mvp_light"] = cy::Matrix4f::Translation(cy::Vec3f(0.5f, 0.5f, 0.5f - shadowBias)) * cy::Matrix4f::Scale(0.5f, 0.5f, 0.5f) * mvpLight;
        prog.SetAttribBuffer("pos", vbo, 3);
        if (objectMesh.HasNormals()) {
            prog.SetAttribBuffer("norm", nbo, 3);
        }
        if (objectMesh.NM() > 0) {
            
            prog.SetAttribBuffer("txc", tbo, 2);
            prog.SetUniform("texture_diffuse", 0);
            prog.SetUniform("texture_specular", 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuseTexID);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, specularTexID);
        }
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(Red, Green, Blue, Alpha);
        glDrawArrays(GL_TRIANGLES, 0, processedVertices.size());


        // Set Program and Program Attributes for plane
        progPlane.Bind();
        progPlane["mvp"] = mvp;
        progPlane["m"] = m;
        progPlane["mv"] = mv;
        progPlane["mv_3x3_it"] = mv_3x3_it;
        progPlane["mnorm"] = m.GetInverse().GetTranspose();
        progPlane["camera_pos"] = cameraPos;
        progPlane["light_pos"] =  viewMatrix * cy::Vec4f(lightX, lightY, lightZ, 0);
        progPlane["reflection_size"] = cy::Vec2f(width, height);
        progPlane["mvp_light"] = cy::Matrix4f::Translation(cy::Vec3f(0.5f, 0.5f, 0.5f - shadowBias)) * cy::Matrix4f::Scale(0.5f, 0.5f, 0.5f) * mvpLight;
        progPlane.SetAttribBuffer("pos", vboplane, 3);
        progPlane.SetAttribBuffer("norm", nboplane, 3);

    
        // Render texture to plane
        glDrawArrays(GL_TRIANGLES, 0, processedVerticesPlane.size());

        // Draw light source
        progLight.Bind();
        modelLight = cy::Matrix4f::Translation(cy::Vec3f(lightX, lightY, lightZ)) * cy::Matrix4f::Scale(0.2f);
        viewLight = viewMatrix;
        projLight = projMatrix;
        mvpLight = projLight * viewLight * modelLight;
        progLight["mvp"] = mvpLight;
        progLight.SetAttribBuffer("pos", vbolight, 3);

        glDrawArrays(GL_TRIANGLES, 0, processedVerticesLight.size());

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}