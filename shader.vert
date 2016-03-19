#version 440

// the texture is 2D, and the text draws in a 2D plane, so this demo has 
// jammed the XY of the screen coordinates (where it draws on the GL 
// viewport) into the vec4's X and Y, and it jams the XY of the texture
// coordinate into the vec4's Z and W
attribute vec4 coord;

// ??change "coord" into two vec2s instead to make it clearer? and "coord" a location or a value or both??

// output to frag shader
smooth out vec2 texpos;

void main(void) {
    // drawing on the 2D plane of the GL viewport, so Z is irrelevant and the
    // W is also, but put it at 1 because W=1 is standard
    gl_Position = vec4(coord.xy, 0, 1);

    // this is a texture coordinate of the three corners of the triangle that 
    // is being drawn
    texpos = coord.zw;
}

//attribute vec4 coord;
//varying vec2 texpos;
//
//void main(void) {
//  gl_Position = vec4(coord.xy, 0, 1);
//  texpos = coord.zw;
//}
