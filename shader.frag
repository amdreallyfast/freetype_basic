#version 440

// must have the same name as its corresponding "out" item in the vert shader
smooth in vec2 texpos;
uniform sampler2D tex;
uniform vec4 color;

// because gl_FragColor is officially deprecated by 4.4
out vec4 finalColor;

void main(void) {
    // the texture is solid, so the texture only provides us (for FreeType 
    // texture anyway) with an alpha channel, then make the color white and
    // multiply it by the color (??wut??)
    finalColor = vec4(1, 1, 1, texture2D(tex, texpos).a) * color;
}

//varying vec2 texpos;
//uniform sampler2D tex;
//uniform vec4 color;
//    
//void main(void) {
//    gl_FragColor = vec4(0.5, 0.5, 0.5, texture2D(tex, texpos).a) * color;
//    //gl_FragColor = vec4(0.5, 0, 1, 1) * color;
//}
    