// the OpenGL version include also includes all previous versions
// Build note: Due to a minefield of preprocessor build flags, the gl_load.hpp must come after 
// the version include.
// Build note: Do NOT mistakenly include _int_gl_4_4.h.  That one doesn't define OpenGL stuff 
// first.
// Build note: Also need to link opengl32.lib (unknown directory, but VS seems to know where it 
// is, so don't put in an "Additional Library Directories" entry for it).
// Build note: Also need to link glload/lib/glloadD.lib.
#include "glload/include/glload/gl_4_4.h"
#include "glload/include/glload/gl_load.hpp"

// Build note: Must be included after OpenGL code (in this case, glload).
// Build note: Also need to link freeglut/lib/freeglutD.lib.  However, the linker will try to 
// find "freeglut.lib" (note the lack of "D") instead unless the following preprocessor 
// directives are set either here or in the source-building command line (VS has a
// "Preprocessor" section under "C/C++" for preprocessor definitions).
// Build note: Also need to link winmm.lib (VS seems to know where it is, so don't put in an 
// "Additional Library Directories" entry).
#define FREEGLUT_STATIC
#define _LIB
#define FREEGLUT_LIB_PRAGMAS 0
#include "freeglut/include/GL/freeglut.h"

// FreeType builds everything with a lot of pre-defined header paths that are all relative to the
// "freetype-2.6.1/include/" folder, so unfortunately (for the sake of a barebones demo) that 
// folder has to be added to the list of "additional include directories" that the project looks
// through when compiling
#include <ft2build.h>
#include FT_FREETYPE_H  // also defined relative to "freetype-2.6.1/include/"

// this linking approach is very useful for portable, crude, barebones demo code, but it is 
// better to link through the project building properties
#pragma comment(lib, "glload/lib/glloadD.lib")
#pragma comment(lib, "opengl32.lib")            // needed for glload::LoadFunctions()
#pragma comment(lib, "freeglut/lib/freeglutD.lib")
#ifdef WIN32
#pragma comment(lib, "winmm.lib")               // Windows-specific; freeglut needs it
#endif

// apparently the FreeType lib also needs a companion file, "freetype261d.pdb"
#pragma comment (lib, "freetype-2.6.1/objs/vc2010/Win32/freetype261d.lib")

// for making program from shader collection
#include <string>
#include <fstream>
#include <sstream>

// for printf(...)
#include <stdio.h>


#define DEBUG     // uncomment to enable the registering of DebugFunc(...)

// TODO: put this in an atlas
struct point {
    GLfloat x;
    GLfloat y;
    GLfloat s;
    GLfloat t;
};


// these should really be encapsulated off in some structure somewhere, but for the sake of this
// barebones demo, keep them here
GLuint gVbo;
FT_Library gFtLib;
FT_Face gFtFace;
GLuint gProgramId;
GLint gUniformTextTextureLoc;   // uniform location within program
GLint gUniformTextColorLoc;     // uniform location within program

/*-----------------------------------------------------------------------------------------------
Description:
    For use with glutInitContextFlags(flagA | flagB | ... | GLUT_DEBUG).  If GLUT_DEBUG is 
    enabled, then apparently the OpenGL global glext_ARB_debug_output will be enabled, which in
    turn will cause the init(...) function to register this function in the OpenGL pipeline.  
    Then, if an error occurs, this function is called automatically to report the error.  This 
    is handy so that glGetError() doesn't have to be called everywhere, complete with debug flags 
    for every single call.

Parameters:
    Unknown.  The function pointer is provided to glDebugMessageCallbackARB(...), and that
    function is responsible for calling this one as it sees fit.
Returns:    None
Exception:  Safe
Creator:
    John Cox (2014)
-----------------------------------------------------------------------------------------------*/
void APIENTRY DebugFunc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
    const GLchar* message, const GLvoid* userParam)
{
    std::string srcName;
    switch (source)
    {
    case GL_DEBUG_SOURCE_API_ARB: srcName = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB: srcName = "Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: srcName = "Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB: srcName = "Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION_ARB: srcName = "Application"; break;
    case GL_DEBUG_SOURCE_OTHER_ARB: srcName = "Other"; break;
    }

    std::string errorType;
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR_ARB: errorType = "Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: errorType = "Deprecated Functionality"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB: errorType = "Undefined Behavior"; break;
    case GL_DEBUG_TYPE_PORTABILITY_ARB: errorType = "Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE_ARB: errorType = "Performance"; break;
    case GL_DEBUG_TYPE_OTHER_ARB: errorType = "Other"; break;
    }

    std::string typeSeverity;
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH_ARB: typeSeverity = "High"; break;
    case GL_DEBUG_SEVERITY_MEDIUM_ARB: typeSeverity = "Medium"; break;
    case GL_DEBUG_SEVERITY_LOW_ARB: typeSeverity = "Low"; break;
    }

    printf("%s from %s,\t%s priority\nMessage: %s\n",
        errorType.c_str(), srcName.c_str(), typeSeverity.c_str(), message);
}

/*-----------------------------------------------------------------------------------------------
Description:
    Encapsulates the creation of an OpenGL GPU program, including the compilation and linking of 
    shaders.  It tries to cover all the basics and the error reporting and is as self-contained 
    as possible, only returning a program ID when it is finished.
Parameters: None
Returns:
    The OpenGL ID of the GPU program.
Exception:  Safe
Creator:
    John Cox (2-13-2016)
-----------------------------------------------------------------------------------------------*/
GLuint CreateProgram()
{
    // hard-coded ignoring possible errors like a boss

    // load up the vertex shader and compile it
    // Note: After retrieving the file's contents, dump the stringstream's contents into a 
    // single std::string.  Do this because, in order to provide the data for shader 
    // compilation, pointers are needed.  The std::string that the stringstream::str() function 
    // returns is a copy of the data, not a reference or pointer to it, so it will go bad as 
    // soon as the std::string object disappears.  To deal with it, copy the data into a 
    // temporary string.
    std::ifstream shaderFile("shader.vert");
    std::stringstream shaderData;
    shaderData << shaderFile.rdbuf();
    shaderFile.close();
    std::string tempFileContents = shaderData.str();
    GLuint vertShaderId = glCreateShader(GL_VERTEX_SHADER);
    const GLchar *vertShaderBytes[] = { tempFileContents.c_str() };
    const GLint vertShaderStrLengths[] = { (int)tempFileContents.length() };
    glShaderSource(vertShaderId, 1, vertShaderBytes, vertShaderStrLengths);
    glCompileShader(vertShaderId);
    // alternately (if you are willing to include and link in glutil, boost, and glm), call 
    // glutil::CompileShader(GL_VERTEX_SHADER, shaderData.str());

    GLint isCompiled = 0;
    glGetShaderiv(vertShaderId, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLchar errLog[128];
        GLsizei *logLen = 0;
        glGetShaderInfoLog(vertShaderId, 128, logLen, errLog);
        printf("vertex shader failed: '%s'\n", errLog);
        glDeleteShader(vertShaderId);
        return 0;
    }

    // load up the fragment shader and compiler it
    shaderFile.open("shader.frag");
    shaderData.str(std::string());      // because stringstream::clear() only clears error flags
    shaderData.clear();                 // clear any error flags that may have popped up
    shaderData << shaderFile.rdbuf();
    shaderFile.close();
    tempFileContents = shaderData.str();
    GLuint fragShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *fragShaderBytes[] = { tempFileContents.c_str() };
    const GLint fragShaderStrLengths[] = { (int)tempFileContents.length() };
    glShaderSource(fragShaderId, 1, fragShaderBytes, fragShaderStrLengths);
    glCompileShader(fragShaderId);

    glGetShaderiv(fragShaderId, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLchar errLog[128];
        GLsizei *logLen = 0;
        glGetShaderInfoLog(fragShaderId, 128, logLen, errLog);
        printf("fragment shader failed: '%s'\n", errLog);
        glDeleteShader(vertShaderId);
        glDeleteShader(fragShaderId);
        return 0;
    }

    GLuint programId = glCreateProgram();
    glAttachShader(programId, vertShaderId);
    glAttachShader(programId, fragShaderId);
    glLinkProgram(programId);

    // the program contains binary, linked versions of the shaders, so clean up the compile 
    // objects
    // Note: Shader objects need to be un-linked before they can be deleted.  This is ok because
    // the program safely contains the shaders in binary form.
    glDetachShader(programId, vertShaderId);
    glDetachShader(programId, fragShaderId);
    glDeleteShader(vertShaderId);
    glDeleteShader(fragShaderId);

    // check if the program was built ok
    GLint isLinked = 0;
    glGetProgramiv(programId, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        printf("program didn't compile\n");
        glDeleteProgram(programId);
        return 0;
    }

    // done here
    return programId;
}

// TODO: header
void RenderText(const char loadThisChar, const float x, const float y, const float userScaleX,
    const float userScaleY)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // create a texture (a 2D texture, that is; no 3D here) that will be used to hold 1 glyph
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // tell the frag shader which texture sampler to use (??I think that there are 15 with 0 enabled by default??)
    glActiveTexture(GL_TEXTURE0);   // ??necessary? texture 0 is active by default, but using another doesn't seem to affect it??
    glUniform1i(gUniformTextTextureLoc, 0);

    //??the heck is this??
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // texture configuration setup (I don't understand most of these)
    // - clamp both S and T (texture's X and Y; they have their own axis names) to edges so that any texture coordinates that are provided to OpenGL won't be allowed beyond the [-1, +1] range that texture coordinates are restricted to
    // - linear filtering when the texture needs to be magnified or (I cringe at the term) "minified" based on scaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // save some dereferencing
    FT_GlyphSlot glyph = gFtFace->glyph;

    // copy the glyph's pixel values from the face (the face is a 2D array of alpha values) into 
    // the texture
    // Note: The function returns an FT_Error...thing (I can't find the definition).  I don't 
    // know what it is, but I do know that returning anything other than 0/false is a problem, so
    // if it doesn't equate to false, we're good.
    if (FT_Load_Char(gFtFace, loadThisChar, FT_LOAD_RENDER))
    {
        // returned an error, so jump to the end of the function and clean up before returning
    }
    else
    {
        // the face->glyph loading went fine, so the face's glyph now has data on where in the 
        // face the character's alpha values are located (it's a little rectangle somewhere in 
        // the face's large 2D plain), and the glyph can now be loaded into the texture
        // Note: This is both a texture initialization (telling it how big it needs to be) and
        // loading.

        // some kind of detail thing; leave at default of 0
        GLint level = 0;

        // tell OpenGL that it should store the data as alpha values (no RGB)
        // Note: That is, it should store [alpha, alpha, alpha, etc.].  If GL_RGBA (red, green, 
        // blue, and alpha) were stated instead, then OpenGL would store the data as 
        // [red, green, blue, alpha, red, green, blue, alpha, red, green, blue, alpha, etc.].
        GLint internalFormat = GL_ALPHA;

        // tell OpenGL that the data is being provided as alpha values
        // Note: Similar to "internal format", but this is telling OpenGL how the data is being
        // provided.  It is possible that many different image file formats store their RGBA 
        // in many different formats, so "provided format" may differ from one file type to 
        // another while "internal format" remains the same in order to provide consistency 
        // after file loading.  In this case though, "internal format" and "provided format"
        // are the same.
        GLint providedFormat = GL_ALPHA;

        // knowing the provided format is great and all, but OpenGL is getting the data in the 
        // form of a void pointer, so it needs to be told if the data is a singed byte, unsigned 
        // byte, unsigned integer, float, etc.
        // Note: The shader uses values on the range [0.0, 1.0] for color and alpha values, but
        // the FreeType face uses a single byte for each alpha value.  Why?  Because a single 
        // unsigned byte (8 bits) can specify 2^8 = 256 (0 - 255) different values.  This is 
        // good enough for what FreeType is trying to draw.  OpenGL compensates for the byte by
        // dividing it by 256 to get it on the range [0.0, 1.0].  
        // Also Note: Some file formats provide data as sets of floats, in which case this type
        // would be GL_FLOAT.
        GLenum providedFormatDataType = GL_UNSIGNED_BYTE;

        // tell OpenGL how many "provided format" values are in a single row
        // Note: Textures (1D (a single row), 2D, and 3D) are all stored in arrays.  These are 
        // linear, even 2D and 3D arrays, which are one instance of the sub-array followed by
        // another.  FreeType text glyphs use 2D textures.  This means that they use a "width"
        // and "height" concept in which the units are specified in "provided format".  In this
        // case, the glyphs "bitmap width" says how many alpha values are in a single row of this
        // 2D texture. 
        GLsizei textureWidth = glyph->bitmap.width;

        // tell OpenGL how many rows of "provided format" items exist
        GLsizei textureHeight = glyph->bitmap.rows;

        // no border (??units? how does this work? play with it??)
        GLint border = 0;

        // set texture data
        glTexImage2D(GL_TEXTURE_2D, level, internalFormat, textureWidth, textureHeight, border,
            providedFormat, providedFormatDataType, glyph->bitmap.buffer);

        // so now a texture is loaded, but where to draw it?

        // X screen coordinates are on the range [-1,+1]
        float oneOverScreenPixelWidth = 2.0f / glutGet(GLUT_WINDOW_WIDTH);

        // Y screen coordinates are, like X, on the range [-1,+1]
        float oneOverScreenPixelHeight = 2.0f / glutGet(GLUT_WINDOW_HEIGHT);

        // this needs some explanation (I do, at least)
        // Note: A glyph in the land of fonts formally has an origin, which is a standardized 
        //  lower-left corner that font creators use as the start of the symbol.  The glyph 
        //  mostly goes to the right of it and up from it, but due to the quirks of the font 
        //  creator's drawing, the glyph can also go to the left of and below it.  In 
        //  console-style text like a bash terminal or a MS command prompt, most letters don't 
        //  to the left of the origin, but some characters like 'j', 'g', and 'y' have tails that 
        //  can go below it.  In the font used by this demo, "FreeSans.ttf", the letters 'k' and 
        //  'g' go just a few pixels to the left of the origin.  In fancier text, loops and 
        //  swirls and curves can frequently go to the left and below the origin.
        // Also Note: Bitmap standards use the top left of a rectangle as the origin, and 
        //  FreeType doesn't change this.  But the world of fonts still uses a math-style
        //  "origin is lower left" approach to drawing fonts.  Hooray for mixing standards.
        // Consequence: If I want to scale the text, I must multiply the left, top, width, and 
        //  height by the user-provided scaling factor.  Note that the "left" value is positive
        //  even though it is to the left of the origin (i.e., negative), so subtract this value
        //  from the user-provided X.
        // Ex: "bitmap left" specifies how many "provided format" items (three floats in RGB 
        // pattern, four floats in RGBA, etc.) are between the texture center and its left edge.
        float scaledGlyphLeft = glyph->bitmap_left * oneOverScreenPixelWidth * userScaleX;
        float scaledGlyphWidth = glyph->bitmap.width * oneOverScreenPixelWidth * userScaleX;
        float scaledGlyphTop = glyph->bitmap_top * oneOverScreenPixelHeight * userScaleY;
        float scaledGlyphHeight = glyph->bitmap.rows * oneOverScreenPixelHeight * userScaleY;

        // could these be condensed into the "scaled glyph" calulations? yes, but this is clearer to me
        float screenCoordLeft = x - scaledGlyphLeft;
        float screenCoordRight = scaledGlyphLeft + scaledGlyphWidth;
        float screenCoordTop = y + scaledGlyphTop;
        float screenCoordBottom = scaledGlyphTop - scaledGlyphHeight;

        // use the whole texture
        // Note: Textures use their own 2D coordinate system (S,T) to avoid confusion with screen coordinates (X,Y).
        float sLeft = 0.0f;
        float sRight = 1.0f;
        float tBottom = 0.0f;
        float tTop = 1.0f;

        // OpenGL draws triangles, but a rectangle needs to be drawn, so specify the four corners
        // of the box in such a way that GL_LINE_STRIP will draw the two triangle halves of the 
        // box
        // Note: As long as the lines from point A to B to C to D don't cross each other (draw 
        // it on paper), this is fine.  I am going with the pattern 
        // bottom left -> bottom right -> top left -> top right.
        // Also Note: Bitmap standards (and most other rectangle standards in programming, such 
        // as for GUIs) understand the top left as the origin, and therefore the texture's pixels 
        // are provided in a rectangle that goes from top left to bottom right.  OpenGL is the 
        // odd one out in the world of rectangles, and it draws textures from lower left 
        // ([S=0,T=0]) to upper right ([S=1,T=1]).  This means that the texture, from OpenGL's 
        // perspective, is "upside down".  Yay for different standards.
        point box[4] = {
            { screenCoordLeft, screenCoordBottom, sLeft, tTop },
            { screenCoordRight, screenCoordBottom, sRight, tTop },
            { screenCoordLeft, screenCoordTop, sLeft, tBottom },
            { screenCoordRight, screenCoordTop, sRight, tBottom }
        };

        // set up (??not create??) the vertex buffer that will hold screen coordinates and texture 
        // coordinates
        GLuint vboId;
        glGenBuffers(1, &vboId);
        glBindBuffer(GL_ARRAY_BUFFER, vboId);
        glBufferData(GL_ARRAY_BUFFER, sizeof(box), box, GL_DYNAMIC_DRAW);

        // tell OpenGL how the "box" data above is organized
        // Note: This must be done AFTER binding the buffer or the program WILL crash.

        // 2 floats per screen coord, 2 floats per texture coord, so 1 variable will do
        GLint itemsPerVertexAttrib = 2;
        
        // how many bytes to "jump" until the next instance of the attribute
        GLint bytesPerVertex = 4 * sizeof(float);  
        
        // this is cast as a pointer due to OpenGL legacy stuff
        GLint bufferStartByteOffset = 0;    

        // shorthand for "vertex attribute index"
        GLint vai = 0;  
        
        // screen coordinates first
        // Note: 2 floats starting 0 bytes from set start.
        glEnableVertexAttribArray(vai);
        glVertexAttribPointer(vai, itemsPerVertexAttrib, GL_FLOAT, GL_FALSE, bytesPerVertex,
            (void *)bufferStartByteOffset);

        // texture coordinates second
        // Note: My approach (a common one) is to use the same kind and number of items for each
        // vertex attribute, so this array's settings are nearly identical to the screen 
        // coordinate's, the only difference being an offset (screen coordinate bytes first, then
        // texture coordinate byte; see the box).
        vai++;
        bufferStartByteOffset += itemsPerVertexAttrib * sizeof(float);
        glEnableVertexAttribArray(vai);
        glVertexAttribPointer(vai, itemsPerVertexAttrib, GL_FLOAT, GL_FALSE, bytesPerVertex,
            (void *)bufferStartByteOffset);

        // all that so that this one function call will work
        // Note: Start at vertex 0 (that is, start at element 0 in the GL_ARRAY_BUFFER) and draw 4 of them. 
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDeleteBuffers(1, &vboId);
    }

    // cleanup
    // Note: This is just good practice so that text-specific rendering functionality doesn't
    // get left in place and mess up other rendering.  This is not a risk in this demo, but it is
    // good practice anyway.
    glDisable(GL_BLEND);
    glBlendFunc(0, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteTextures(1, &textureId);

    // do NOT disable vertex attribute arrays when using VAOs
}

/*-----------------------------------------------------------------------------------------------
Description:
    This is the rendering function.  It tells OpenGL to clear out some color and depth buffers,
    to set up the data to draw, to draw than stuff, and to report any errors that it came across.
    This is not a user-called function.

    This function is registered with glutDisplayFunc(...) during glut's initialization.
Parameters: None
Returns:    None
Exception:  Safe
Creator:
    John Cox (2-13-2016)
-----------------------------------------------------------------------------------------------*/
void display()
{
    glUseProgram(gProgramId);

    // clear existing data
    glClearColor(0.2f, 0.2f, 0.2f, 0.0f);   // make background a dull grey
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // the text will be drawn, in part, via a manipulation of pixel alpha values, and apparently
    // OpenGL's blending does this
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // the shader for this program uses a vec4 (implicit content type is float) for color, so 
    // specify text color as a 4-float array and give it to shader 
    GLfloat color[4] = { 0.5f, 0.5f, 0, 1 };
    glUniform4fv(gUniformTextColorLoc, 1, color);

    // set font size to 48 pixels (??"pixel width" to 0??)
    FT_Set_Pixel_Sizes(gFtFace, 0, 48);

    // draw all the stuff that we want to, then swap the buffers
    // Note: Viable X and Y values are on the range [-1, +1].  No world->camera->clip->screen 
    // space conversions are performed here, so they must be specified in screen space. 
    // (??you sure? X = -1, Y = -1 doesn't draw??)
    float X = 0;
    float Y = 0;
    RenderText('k', X, Y, 1.0f, 1.0f);
    RenderText('k', X, Y, 2.0f, 2.0f);
    RenderText('k', X, Y, 4.0f, 4.0f);

    // tell the GPU to swap out the displayed buffer with the one that was just rendered
    glutSwapBuffers();

    // tell glut to call this display() function again on the next iteration of the main loop
    // Note: https://www.opengl.org/discussion_boards/showthread.php/168717-I-dont-understand-what-glutPostRedisplay()-does
    glutPostRedisplay();

    // clean up bindings
    // Note: This is just good practice, but in reality the bindings can be left as they were 
    // and re-bound on each new call to this rendering function.
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

/*-----------------------------------------------------------------------------------------------
Description:
    Tell's OpenGL to resize the viewport based on the arguments provided.  This is not a 
    user-called function.

    This function is registered with glutReshapeFunc(...) during glut's initialization.
Parameters: 
    w   The width of the window in pixels.
    h   The height of the window in pixels.
Returns:    None
Exception:  Safe
Creator:
    John Cox (2-13-2016)
-----------------------------------------------------------------------------------------------*/
void reshape(int w, int h)
{
    glViewport(0, 0, w, h);
}

/*-----------------------------------------------------------------------------------------------
Description:
    Executes some kind of command when the user presses a key on the keyboard.  This is not a 
    user-called function.

    This function is registered with glutKeyboardFunc(...) during glut's initialization.
Parameters: 
    key     The ASCII code of the key that was pressed (ex: ESC key is 27)
    x       ??
    y       ??
Returns:    None
Exception:  Safe
Creator:
    John Cox (2-13-2016)
-----------------------------------------------------------------------------------------------*/
void keyboard(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 27:
    {
        // ESC key
        glutLeaveMainLoop();
        return;
    }
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------------------------
Description:
    ??what does it do? I picked it up awhile back and haven't changed it??
Parameters:
    displayMode     ??
    width           ??
    height          ??
Returns:    
    ??what??
Exception:  Safe
Creator:
    John Cox (2-13-2016)
-----------------------------------------------------------------------------------------------*/
unsigned int defaults(unsigned int displayMode, int &width, int &height) 
{ 
    return displayMode; 
}

/*-----------------------------------------------------------------------------------------------
Description:
    Convenient for deleting anything that needs cleaning up.
Parameters: None
Returns:    None
Exception:  Safe
Creator:
    John Cox (3-9-2016)
-----------------------------------------------------------------------------------------------*/
void FreeResources()
{
    glDeleteProgram(gProgramId);
    glDeleteBuffers(1, &gVbo);
}

/*-----------------------------------------------------------------------------------------------
Description:
    Governs window creation, the initial OpenGL configuration (face culling, depth mask, even
    though this is a 2D demo and that stuff won't be of concern), the creation of geometry, and
    the creation of a texture.
Parameters:
    argc    (From main(...)) The number of char * items in argv.  For glut's initialization.
    argv    (From main(...)) A collection of argument strings.  For glut's initialization.
Returns:
    False if something went wrong during initialization, otherwise true;
Exception:  Safe
Creator:
    John Cox (3-7-2016)
-----------------------------------------------------------------------------------------------*/
bool init(int argc, char *argv[])
{
    glutInit(&argc, argv);

    // I don't know what this is doing, but it has been working, so I'll leave it be for now
    int windowWidth = 500;  // square 500x500 pixels
    int windowHeight = 500;
    unsigned int displayMode = GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH | GLUT_STENCIL;
    displayMode = defaults(displayMode, windowWidth, windowHeight);
    glutInitDisplayMode(displayMode);

    // create the window
    // ??does this have to be done AFTER glutInitDisplayMode(...)??
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(300, 200);   // X = 0 is screen left, Y = 0 is screen top
    int window = glutCreateWindow(argv[0]);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);

    // OpenGL 4.3 was where internal debugging was enabled, freeing the user from having to call
    // glGetError() and analyzing it after every single OpenGL call, complete with surrounding it
    // with #ifdef DEBUG ... #endif blocks
    // Note: https://blog.nobel-joergensen.com/2013/02/17/debugging-opengl-part-2-using-gldebugmessagecallback/
    int glMajorVersion = 4;
    int glMinorVersion = 4;
    glutInitContextVersion(glMajorVersion, glMinorVersion);
    glutInitContextProfile(GLUT_CORE_PROFILE);
#ifdef DEBUG
    glutInitContextFlags(GLUT_DEBUG);   // if enabled, 
#endif

    // glload must load AFTER glut loads the context
    glload::LoadTest glLoadGood = glload::LoadFunctions();
    if (!glLoadGood)    // apparently it has an overload for "bool type"
    {
        printf("glload::LoadFunctions() failed\n");
        return false;
    }
    else if (!glload::IsVersionGEQ(glMajorVersion, glMinorVersion))
    {
        // the "is version" check is an "is at least version" check
        printf("Your OpenGL version is %i, %i. You must have at least OpenGL %i.%i to run this tutorial.\n",
            glload::GetMajorVersion(), glload::GetMinorVersion(), glMajorVersion, glMinorVersion);
        glutDestroyWindow(window);
        return 0;
    }
    else if (glext_ARB_debug_output)
    {
        // condition will be true if GLUT_DEBUG is a context flag
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
        glDebugMessageCallbackARB(DebugFunc, (void*)15);
    }

    // these OpenGL initializations are for 3D stuff, where depth matters and multiple shapes can
    // be "on top" of each other relative to the most distant thing rendered, and this barebones 
    // code is only for 2D stuff, but initialize them anyway as good practice (??bad idea? only 
    // use these once 3D becomes a thing??)
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0.0f, 1.0f);

    // FreeType needs to load itself into particular variables
    // Note: FT_Init_FreeType(...) returns something called an FT_Error, which VS can't find.
    // Based on the useage, it is assumed that 0 is returned if something went wrong, otherwise
    // non-zero is returned.  That is the only explanation for this kind of condition.
    if (FT_Init_FreeType(&gFtLib))
    {
        fprintf(stderr, "Could not init freetype library\n");
        return false;
    }
    
    // Note: FT_New_Face(...) also returns an FT_Error.
    const char *fontFilePath = "FreeSans.ttf";  // file path relative to solution directory
    if (FT_New_Face(gFtLib, fontFilePath, 0, &gFtFace))
    {
        fprintf(stderr, "Could not open font '%s'\n", fontFilePath);
        return false;
    }

    gProgramId = CreateProgram();
    
    // pick out the attributes and uniforms used in the FreeType GPU program

    char textTextureName[] = "textureSamplerId";
    gUniformTextTextureLoc = glGetUniformLocation(gProgramId, textTextureName);
    if (gUniformTextTextureLoc == -1)
    {
        fprintf(stderr, "Could not bind uniform '%s'\n", textTextureName);
        glDeleteProgram(gProgramId);
        return false;
    }

    //char textColorName[] = "color";
    char textColorName[] = "textureColor";
    gUniformTextColorLoc = glGetUniformLocation(gProgramId, textColorName);
    if (gUniformTextColorLoc == -1)
    {
        fprintf(stderr, "Could not bind uniform '%s'\n", textColorName);
        glDeleteProgram(gProgramId);
        return false;
    }

    // all went well
    return true;
}

/*-----------------------------------------------------------------------------------------------
Description:
    Program start and end.
Parameters: 
    argc    The number of strings in argv.
    argv    A pointer to an array of null-terminated, C-style strings.
Returns:
    0 if program ended well, which it always does or it crashes outright, so returning 0 is fune
Exception:  Safe
Creator:
    John Cox (2-13-2016)
-----------------------------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    if (!init(argc, argv))
    {
        // bad initialization; it will take care of it's own error reporting
        return 1;
    }

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMainLoop();

    // Note: Apparently there is a function FreeResource(...) (note the lack of an appending 's')
    // off in some API (I don't know where).  Use the FreeResources() that was defined in this 
    // file.
    FreeResources();
    return 0;
}