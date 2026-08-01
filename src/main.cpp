#include "ofMain.h"
#include "ofApp.h"

int main()
{
    ofGLWindowSettings settings;
    settings.setSize(640, 480);
    settings.windowMode = OF_WINDOW;
    settings.setGLVersion(3, 2);

    auto window = ofCreateWindow(settings);

    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();
}
