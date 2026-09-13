#include "ofMain.h"
#include "ofApp.h"

int main()
{
    #ifdef TARGET_OPENGLES
	ofGLESWindowSettings settings;
    settings.setSize(720, 480);
	settings.glesVersion=2;
    settings.windowMode = OF_FULLSCREEN;
    #else
    ofGLWindowSettings settings;
    settings.setSize(720, 480);
    settings.setGLVersion(3, 2);
    settings.windowMode = OF_WINDOW;
    #endif

    auto window = ofCreateWindow(settings);

    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();
}
